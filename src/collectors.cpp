#include "agent.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h> 

namespace watcheD {

void setResponse404(response_ptr response, std::string content) {
	*response << "HTTP/1.1 404 Not found\r\nServer: watched.agent\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}
void setResponseHtml(response_ptr response, std::string content) {
	*response << "HTTP/1.1 200 OK\r\nServer: watched.agent\r\nContent-Type:text/html; charset=UTF-8\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}
void setResponseJson(response_ptr response, std::string content) {
	*response << "HTTP/1.1 200 OK\r\nServer: watched.agent\r\nContent-Type:application/json\r\nAccess-Control-Allow-Origin:*\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}

/*********************************
 * Ressource
 */
Ressource::Ressource(uint p_size, std::string p_typeName): typeName(p_typeName), size(p_size) {
	globalCounters["ressource"].add++;
}

Ressource::~Ressource(){
	globalCounters["ressource"].del++;
}

void Ressource::nextValue() {
	Json::Value data(Json::objectValue);
	std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();
	if (v.empty())
		v.insert(v.begin(), data);
	else
		v.insert(v.begin(), v[0]);
	globalCounters["values_count"].add++;
	v[0]["timestamp"] = fp_ms.count();
	if (v.size()>size) {
		globalCounters["values_count"].del+=v.size()-size;
		v.resize(size);
	}
}

std::string  Ressource::getHistory(double since) {
	std::stringstream ss;
	bool splitted = true;
	ss << "[\n";
	if (v.size()>=1) for(int i=v.size()-1;i>=0;i--) {
		if (v[i].isMember("timestamp") && v[i]["timestamp"].asDouble() >= since) {
			if (!splitted) {
				ss << ",\n";
			}
			ss << v[i];
			splitted=false;
		}
	}
	ss << "\n]\n";
	return ss.str();
}

std::string  Ressource::getMorrisDesc() {
	std::string keys = "ykeys: [";
	std::string desc = "labels: [";
	std::string sep  = "";
	std::map<std::string,std::string>::iterator b=d.begin();
	for(std::map<std::string,std::string>::iterator i = b;i != d.end();i++) {
		keys+= sep+"'"+i->first+"'";
		desc+= sep+"'"+i->second+"'";
		sep=",";
	}
	return keys+"], "+desc+"]";
}

void Ressource::getDefinition(Json::Value* p_defs) {
	(*p_defs)["type"] = "object";
	(*p_defs)["required"][0] = "timestamp";
	(*p_defs)["properties"]["timestamp"]["description"]	= "timestamp";
	(*p_defs)["properties"]["timestamp"]["type"]		= "number";
	(*p_defs)["properties"]["timestamp"]["format"]		= "double";
	for(std::map<std::string,std::string>::iterator i = d.begin();i != d.end();i++) {
		(*p_defs)["properties"][i->first]["type"] 	= t[i->first];
		(*p_defs)["properties"][i->first]["description"]= i->second;
	}
}


/*********************************
 * pidRessource
 */
void pidRessource::nextValue() {
	Ressource::nextValue();
	for(std::map<std::string,std::map<uint64_t,uint64_t>>::iterator i=values.begin();i!=values.end();i++)
		v[0][i->first] = 0.0;
}

void pidRessource::setPidValue(std::string p_name, uint64_t p_pid, uint64_t p_value) {
	std::map<std::string,std::map<uint64_t,uint64_t>>::iterator pcont=values.find(p_name);
	if (pcont != values.end()) {
		std::map<uint64_t,uint64_t>::iterator pval=values[p_name].find(p_pid);
		if (pval != values[p_name].end() && values[p_name][p_pid]<p_value)
			v[0][p_name] = v[0][p_name].asDouble()+factor*(p_value-values[p_name][p_pid]);
	}
	values[p_name][p_pid]=p_value;
}

/*********************************
 * Collector
 */

Collector::Collector(std::string p_name, std::shared_ptr<HttpServer> p_server, Json::Value* p_cfg, uint p_history, uint p_freq_pool, std::shared_ptr<service> p_serv): cfg(p_cfg), morrisType("Line"), morrisOpts("behaveLikeLine:true,"), name(p_name), basePath("/system/"), haveService(false), active(false),  server(p_server) {
	globalCounters["collector"].add++;
	if(! cfg->isMember("history") && p_history>0) {
		(*cfg)["history"] = p_history;
		(*cfg)["history"].setComment(std::string("/*\t\tNumber of elements to keep*/"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("poll-frequency") && p_freq_pool>0) {
		(*cfg)["poll-frequency"] = p_freq_pool;
		(*cfg)["poll-frequency"].setComment(std::string("/* \tNumber of seconds between snapshots*/"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("enable")) {
		(*cfg)["enable"] = true;
		(*cfg)["enable"].setComment(std::string("/*\t\tEnable this plugin ?*/"), Json::commentAfterOnSameLine);
	}
	if( p_serv != nullptr )
		setService(p_serv);
}

Collector::~Collector() {
	std::unique_lock<std::mutex> locker(lock);
	globalCounters["collector"].del++;
	if (active) {
		active=false;
		timer.kill();
	}
	active=false;
	if (my_thread.joinable()) my_thread.join();
	server->logInfo("Collector::~Collector","Deleting "+name+" collector");
}

void Collector::setService(std::shared_ptr<service> p_serv) {
	onService	= p_serv;
	haveService	= true;
	basePath	= "/service/"+p_serv->getID()+"/";
}

void Collector::startThread() {
	if (!active && (*cfg)["enable"].asBool()) {
		active=true;
		my_thread = std::thread ([this]() {
			int sec = ((*cfg)["poll-frequency"]).asInt();
			while(active) {
				// jsoncpp isnt thread safe
				try {
					std::unique_lock<std::mutex> locker(lock);
					if (active) {// if we've been locked by the destructor...
						collect();
						server->logInfo("Collector::thread", basePath+name+" updated");
					}
				} catch(std::exception &e) {
					server->logWarning("Collector::thread", basePath+name+" failed  to collect: "+e.what());
				}
				timer.wait_for(std::chrono::seconds(sec));
			}
		});
	}
}
void Collector::addRessource(std::string p_name, std::string p_desc, std::string p_typeName) {
	std::map<std::string, std::shared_ptr<Ressource> >::const_iterator it = ressources.find(p_name);
	if( it == ressources.end()) {
		ressources[p_name]	= std::make_shared<Ressource>((*cfg)["history"].asUInt(), p_typeName);
		desc[p_name]		= p_desc;
	}
}

void Collector::addGetMetricRoute() {
	associate(server,"GET","^"+basePath+name+"/(.*)/history$",doGetHistory);
	associate(server,"GET","^"+basePath+name+"/(.*)/history.since=([0-9.]*)$",doGetHistory);
	associate(server,"GET","^"+basePath+name+"/(.*)/graph$",doGetGraph);
}

void Collector::getDefinitions(Json::Value* p_defs) {
	Json::Value data(Json::objectValue);
	if (haveService) {
		data["x-isService"] = true;
	}
	if ((*cfg)["enable"].asBool()) {
		for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
			if (! p_defs->isMember(i->second->typeName))
				(*p_defs)[i->second->typeName] = data;
			i->second->getDefinition( &((*p_defs)[i->second->typeName]) );
		}
	}
}

std::string Collector::getHost() {
	if (host != "")
		return host;

	struct addrinfo hints, *info;
	int gai_result;
	char ghost[1024];
	memset(ghost,0,1024);
	gethostname(ghost,1023);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	if ((gai_result = getaddrinfo(ghost, "http", &hints, &info)) != 0)
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
	else
		host = info->ai_canonname;

	freeaddrinfo(info);
	return host;
}

void Collector::getPaths(Json::Value* p_defs) {
	if ((*cfg)["enable"].asBool()) {
		for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["x-host"] = getHost();
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["summary"] = desc[i->first];
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["parameters"][0]["name"] = "len";
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["parameters"][0]["in"] = "query";
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["parameters"][0]["type"] = "integer";
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["parameters"][0]["required"] = false;
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["parameters"][0]["description"] = "Length of the history to fetch";
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["responses"]["200"]["description"] = desc[i->first];
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["responses"]["200"]["schema"]["type"] = "array";
			(*p_defs)[basePath+name+"/"+i->first+"/history"]["get"]["responses"]["200"]["schema"]["items"]["$ref"] = "#/definitions/"+i->second->typeName; //name+"-"+i->first;
			if (haveService) {
				std::shared_ptr<service> srv = onService.lock();
				if (srv) 
					(*p_defs)[basePath+name+"/"+i->first+"/history"]["x-service"] = srv->getID();
			}
		}
	}
}

void Collector::doGetGraph(response_ptr response, request_ptr request) {
	std::string id  = (*request)[0];
	std::string sub	= "";

	std::map<std::string, std::shared_ptr<Ressource> >::const_iterator it = ressources.find(id);
	if( it == ressources.end()) {
		setResponse404(response, "No such Metric");
		return;
	}
	if (haveService) {
		std::shared_ptr<service> srv = onService.lock();
		if (srv) 
			sub = srv->getID();
	}

	std::stringstream stream;
        stream << server->getHead(desc[id], sub);
	stream << "<div class=\"row\"><div class=\"col-md-12\"><div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">Graphics</h3><div class=\"box-tools pull-right\"><button type=\"button\" class=\"btn btn-box-tool\" onclick=\"toggle();\"><i class=\"fa fa-square-o\" id=\"btn-refresh\"></i>refresh</button></div><div class=\"box-body\"><div class=\"chart\"><div id=\"" << name.c_str() << "-graph\"></div>\n</div></div></div></div></div>\n";
        stream << server->getFoot("var timerId = 0;var enable = false;var timerLen = "+((*cfg)["poll-frequency"].asString())+"*1000;\nfunction start() { timerId = setInterval(function() { updateLiveGraph("+name+"Graph); }, timerLen); }\nfunction stop() { clearInterval(timerId); }\nfunction toggle() { if (enable) { stop();enable=false; $('#btn-refresh').removeClass('fa-check-square-o'); $('#btn-refresh').addClass('fa-square-o'); }else{ start();enable=true; $('#btn-refresh').removeClass('fa-square-o'); $('#btn-refresh').addClass('fa-check-square-o'); } }\nfunction updateLiveGraph("+name+"Graph) {\n  $.getJSON('"+basePath+name+"/"+id.c_str()+"/history', function(results) { "+name+"Graph.setData(results); });\n}\n"+name+"Graph = new Morris."+morrisType+"({element: '"+name+"-graph',  data: [], xkey: 'timestamp', hideHover: true,pointSize:0,fillOpacity:0.3,"+morrisOpts+ressources[id]->getMorrisDesc()+"});\nupdateLiveGraph("+name+"Graph);");
        setResponseHtml(response, stream.str());
}

void Collector::doGetHistory(response_ptr response, request_ptr request) {
	std::string name   = (*request)[0];
	double since  = -1;
	if (request->size()>1) {
		try {
			since=stod((*request)[1]);
		} catch (std::exception &e) { }
	}
        /*if (request.query().has("since"))
		since= stod(request.query().get("since").get());*/

	std::map<std::string, std::shared_ptr<Ressource> >::const_iterator it = ressources.find(name);
	if( it == ressources.end()) {
		setResponse404(response, "No such Metric");
		return;
	}
	// jsoncpp isnt thread safe
	std::unique_lock<std::mutex> locker(lock);
	setResponseJson(response, ressources[name]->getHistory(since));
	server->logInfo("Collector::doGetHistory", basePath+this->name+"/"+name+"/history sent");
}

void Collector::getIndexHtml(std::stringstream& stream ){
	if ((*cfg)["enable"].asBool()) {
		stream << "<div class=\"col-md-3\"><div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">"+name+"</h3></div><div class=\"box-body\">\n";
		if (name == "cpuload") {
			stream << "<a href=\""+basePath+name+"/avg/graph\"><div class=\"clearfix\"><span class=\"pull-left\">Load average (1mn)</span><small class=\"pull-right\">"+ressources["avg"]->getValue("avg1")->asString()+"</small></div><div class=\"clearfix\"><span class=\"pull-left\">Load average (5mn)</span><small class=\"pull-right\">"+ressources["avg"]->getValue("avg5")->asString()+"</small></div><div class=\"clearfix\"><span class=\"pull-left\">Load average (15mn)</span><small class=\"pull-right\">"+ressources["avg"]->getValue("avg15")->asString()+"</small></div></a>\n";
		}else if (name == "uptime") {
			stream << "<a href=\""+basePath+name+"/uptime/graph\"><div class=\"clearfix\"><span class=\"pull-left\">uptime</span><small class=\"pull-right\">"+ressources["uptime"]->getValue("uptime")->asString()+"</small></div></a>\n";
		}else if (name == "diskusage") {
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				double used = 100 - i->second->getValue("pctfree")->asDouble();
				std::string color = "green";
				if	(used>90)	color = "red";
				else if (used>70)	color = "yellow";
				else if (used<20)	color = "blue";
				stream << "<a href=\""+basePath+name+"/"+i->first+"/graph\"><div class=\"clearfix\"><span class=\"pull-left\">"+desc[i->first]+"</span><small class=\"pull-right\">"+std::to_string(used)+"%</small></div><div class=\"progress xs\"><div class=\"progress-bar progress-bar-"+color+"\" style=\"width: "+std::to_string(used)+"%;\"></div></div></a>\n";
			}
		}else if (name == "memory") {
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				double used = 100 - i->second->getValue("pct")->asDouble();
				std::string color = "green";
				if	(used>90)	color = "red";
				else if (used>70)	color = "yellow";
				else if (used<20)	color = "blue";
				stream << "<a href=\""+basePath+name+"/"+i->first+"/graph\"><div class=\"clearfix\"><span class=\"pull-left\">"+desc[i->first]+"</span><small class=\"pull-right\">"+std::to_string(used)+"%</small></div><div class=\"progress xs\"><div class=\"progress-bar progress-bar-"+color+"\" style=\"width: "+std::to_string(used)+"%;\"></div></div></a>\n";
			}
		}else if (name == "cpuuse") {
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				if (desc[i->first].substr(0,3) != "CPU") continue;
				double user = i->second->getValue("user")->asDouble();		// red
				double sys = i->second->getValue("system")->asDouble();		// yellow
				double irq = i->second->getValue("irq")->asDouble();		// light-blue
				double sirq = i->second->getValue("softirq")->asDouble();	// aqua
				double nice = i->second->getValue("nice")->asDouble();		// green
				double iowait = i->second->getValue("iowait")->asDouble();	// blue
				double total = user+sys+irq+sirq+nice+iowait;
				stream << "<a href=\""+basePath+name+"/"+i->first+"/graph\"><div class=\"clearfix\"><span class=\"pull-left\">"+desc[i->first]+"</span><small class=\"pull-right\">"+std::to_string(total)+"%</small></div><div class=\"progress xs\"><div class=\"progress-bar progress-bar-red\" style=\"width: "+std::to_string(user)+"%;\"></div><div class=\"progress-bar progress-bar-yellow\" style=\"width: "+std::to_string(sys)+"%;\"></div><div class=\"progress-bar progress-bar-green\" style=\"width: "+std::to_string(nice)+"%;\"></div><div class=\"progress-bar progress-bar-blue\" style=\"width: "+std::to_string(iowait)+"%;\"></div><div class=\"progress-bar progress-bar-light-blue\" style=\"width: "+std::to_string(irq)+"%;\"></div><div class=\"progress-bar progress-bar-aqua\" style=\"width: "+std::to_string(sirq)+"%;\"></div></div></a>\n";
			}
			stream << "<ul>\n";
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				if (desc[i->first].substr(0,3) == "CPU") continue;
				stream << "<li><a href=\""+basePath+name+"/"+i->first+"/graph\">" << desc[i->first].c_str() << "</a></li>\n";
			}
			stream << "</ul>";
		}else if (name == "cpuspeed") {
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				int used = i->second->getValue("MHz")->asInt();
				stream << "<a href=\""+basePath+name+"/"+i->first+"/graph\"><div class=\"clearfix\"><span class=\"pull-left\">"+desc[i->first]+"</span><small class=\"pull-right\">"+std::to_string(used)+"Mhz</small></div></a>\n";
			}
		}else if (name == "diskstats") {
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				double used = i->second->getValue("iowtime")->asDouble()/10.0;
				std::string color = "green";
				if	(used>90)	color = "red";
				else if (used>70)	color = "yellow";
				else if (used<20)	color = "blue";
				stream << "<a href=\""+basePath+name+"/"+i->first+"/graph\"><div class=\"clearfix\"><span class=\"pull-left\">"+desc[i->first]+"</span><small class=\"pull-right\">"+std::to_string(used)+"%</small></div><div class=\"progress xs\"><div class=\"progress-bar progress-bar-"+color+"\" style=\"width: "+std::to_string(used)+"%;\"></div></div></a>\n";
			}
		}else {
			stream << "<ul>\n";
			for(std::map<std::string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
				stream << "<li><a href=\""+basePath+name+"/"+i->first+"/graph\">" << desc[i->first].c_str() << "</a></li>\n";
			}
			stream << "</ul>";
		}
		stream << "</div></div></div>\n";
	}
}


/*********************************
 * CollectorsManager
 */

std::map<std::string, collector_maker_t *> collectorFactory;

void CollectorsManager::getIndexHtml(std::stringstream& stream ) {
	for(std::map<std::string, std::shared_ptr<Collector> >::iterator i = collectors.begin();i != collectors.end();i++)
		i->second->getIndexHtml(stream);
}
void CollectorsManager::getJson(Json::Value* p_defs) {
	int cnt = 0;
	for(std::map<std::string, std::shared_ptr<Collector> >::iterator i = collectors.begin();i != collectors.end();i++,cnt++) {
		i->second->getDefinitions(&((*p_defs)["definitions"]));
		i->second->getPaths(&((*p_defs)["paths"]));
	}
}


CollectorsManager::CollectorsManager(std::shared_ptr<HttpServer> p_server, std::shared_ptr<Config> p_cfg) : server(p_server), cfg(p_cfg) {
	Json::Value*	servCfg = cfg->getPlugins();
	// 1st : find and load the modules
	std::map<std::string, collector_maker_t *, std::less<std::string> >::iterator factit;
	DIR *dir;
	const std::string directory = (*servCfg)["collectors_cpp"].asString();
	struct dirent *ent;
	struct stat st;
	void *dlib;

	dir = opendir(directory.c_str());
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			const std::string file_name = ent->d_name;
			const std::string full_file_name = directory + "/" + file_name;

			if (file_name[0] == '.')
				continue;
			if (stat(full_file_name.c_str(), &st) == -1)
				continue;
			const bool is_directory = (st.st_mode & S_IFDIR) != 0;
			if (is_directory)
				continue;


			if (file_name.substr(file_name.rfind(".")) == ".so") {
				dlib = dlopen(full_file_name.c_str(), RTLD_NOW);
				if(dlib == NULL) {
					server->logError("CollectorsManager::", std::string(dlerror())+" while loading "+full_file_name); 
					//exit(-1);
				}
			}
		}
		closedir(dir);
	} else	server->logWarning("CollectorsManager::", directory+" doesnt exist. No collectors plugins will be used\n");

	// Instanciate the plugins classes
	for(factit = collectorFactory.begin();factit != collectorFactory.end(); factit++) {
		collectors[factit->first]	= factit->second(server, cfg->getCollector(factit->first));
	}

	// Load the lua plugins
	const std::string dirlua = (*servCfg)["collectors_lua"].asString();
	dir = opendir(dirlua.c_str());
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			const std::string file_name = ent->d_name;
			const std::string name = file_name.substr(0,file_name.rfind("."));
			const std::string full_file_name = dirlua + "/" + file_name;

			if (file_name[0] == '.')
				continue;
			if (stat(full_file_name.c_str(), &st) == -1)
				continue;
			const bool is_directory = (st.st_mode & S_IFDIR) != 0;
			if (is_directory)
				continue;


			if (file_name.substr(file_name.rfind(".")) == ".lua" && collectors.find(name)==collectors.end()) {
				collectors[name] = std::make_shared<LuaCollector>(server, cfg->getCollector(name), full_file_name);
			}
		}
		closedir(dir);
	} else	server->logWarning("CollectorsManager::", dirlua +" doesnt exist. No lua collectors will be used\n");
}

void CollectorsManager::startThreads() {
	for(std::map<std::string, std::shared_ptr<Collector> >::iterator i = collectors.begin();i != collectors.end();i++)
		i->second->startThread();
}

CollectorsManager::~CollectorsManager() {
}


/*********************************
 * SelfCollector
 */
std::map<std::string, counter> globalCounters;
SelfCollector::SelfCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_serv) : Collector("self", p_srv, p_cfg, 150, 10, p_serv) {
	ressources["changes"]	= std::make_shared<tickRessource>((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt(), "watched_agent_count");
	desc["changes"]		= "Internal changes";
	addRessource("values", "Current objects count", "watched_agent_values");
	for(std::map<std::string, counter>::iterator i=globalCounters.begin();i!=globalCounters.end();i++) {
		ressources["values"]->addProperty(i->first, i->first+" current count", "number");
		ressources["changes"]->addProperty(i->first+"_del", i->first+" deleted", "number");
		ressources["changes"]->addProperty(i->first+"_add", i->first+" added", "number");
	}
}

void SelfCollector::collect() {
	std::shared_ptr<tickRessource>	tres = reinterpret_cast<std::shared_ptr<tickRessource>&>(ressources["changes"]);
	ressources["values"]->nextValue();
	tres->nextValue();
	for(std::map<std::string, counter>::iterator i=globalCounters.begin();i!=globalCounters.end();i++) {
		ressources["values"]->setProperty(i->first, i->second.add-i->second.del);
		tres->setTickValue(i->first+"_del", i->second.del);
		tres->setTickValue(i->first+"_add", i->second.add);
	}
}

}
