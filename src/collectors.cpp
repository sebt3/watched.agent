#include "agent.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h> 

using namespace std;

namespace watcheD {

void setResponse404(response_ptr response, std::string content) {
	*response << "HTTP/1.1 404 Not found\r\nServer: watched.agent\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}
void setResponseHtml(response_ptr response, std::string content) {
	*response << "HTTP/1.1 200 OK\r\nServer: watched.agent\r\nAccess-Control-Allow-Origin:*\r\nContent-Type:text/html; charset=UTF-8\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}
void setResponseJson(response_ptr response, std::string content) {
	*response << "HTTP/1.1 200 OK\r\nServer: watched.agent\r\nContent-Type:application/json\r\nAccess-Control-Allow-Origin:*\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}

/*********************************
 * Ressource
 */
void Ressource::nextValue() {
	Json::Value data(Json::objectValue);
	chrono::duration<double, std::milli> fp_ms = chrono::system_clock::now().time_since_epoch();
	if (v.empty())
		v.insert(v.begin(), data);
	else
		v.insert(v.begin(), v[0]);
	v[0]["timestamp"] = fp_ms.count();
	while(v.size()>=size)
		v.pop_back();
}

string  Ressource::getHistory(double since) {
	stringstream ss;
	bool splitted = true;
	ss << "[\n";
	if (v.size()>1) for(uint i=v.size()-1;i>0;i--) {
		if (v[i].isMember("timestamp") && v[i]["timestamp"].asDouble() > since) {
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

string  Ressource::getMorrisDesc() {
	string keys = "ykeys: [";
	string desc = "labels: [";
	string sep  = "";
	map<string,string>::iterator b=d.begin();
	for(map<string,string>::iterator i = b;i != d.end();i++) {
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
	for(map<string,string>::iterator i = d.begin();i != d.end();i++) {
		(*p_defs)["properties"][i->first]["type"] 	= t[i->first];
		(*p_defs)["properties"][i->first]["description"]= i->second;
	}
}


/*********************************
 * Collector
 */

Collector::Collector(string p_name, std::shared_ptr<HttpServer> p_server, Json::Value* p_cfg, uint p_history, uint p_freq_pool): cfg(p_cfg), morrisType("Line"), morrisOpts("behaveLikeLine:true,"), name(p_name), basePath("/system/"), active(false),  server(p_server) {
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
}
Collector::~Collector() {
	if (active) {
		active=false;
		my_thread.join();
	}
}
void Collector::startThread() {
	if (!active && (*cfg)["enable"].asBool()) {
		active=true;
		my_thread = std::thread ([this]() {
			int sec = ((*cfg)["poll-frequency"]).asInt();
			while(active) {
				collect();
				this_thread::sleep_for(chrono::seconds(sec));
			}
		});
	}
}
void Collector::addRessource(string p_name, string p_desc, std::string p_typeName) {
	map<string, std::shared_ptr<Ressource> >::const_iterator it = ressources.find(p_name);
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
	if ((*cfg)["enable"].asBool()) {
		for(map<string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
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

	return host;
}

void Collector::getPaths(Json::Value* p_defs) {
	if ((*cfg)["enable"].asBool()) {
		for(map<string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
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
		}
	}
}

void Collector::doGetGraph(response_ptr response, request_ptr request) {
	string id   = (*request)[0];
	string str;
	const char* name_c  = name.c_str();

	map<string, std::shared_ptr<Ressource> >::const_iterator it = ressources.find(id);
	if( it == ressources.end()) {
		setResponse404(response, "No such Metric");
		return;
	}
	std::stringstream stream;
        stream << "<html><head><title>" << APPS_NAME.c_str() << " - " << desc[id].c_str() << "</title>\n";
        stream << "<link rel=\"stylesheet\" href=\"http://cdnjs.cloudflare.com/ajax/libs/morris.js/0.5.1/morris.css\">";
	stream << "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.9.0/jquery.min.js\"></script>";
	stream << "<script src=\"http://cdnjs.cloudflare.com/ajax/libs/raphael/2.1.0/raphael-min.js\"></script>";
	stream << "<script src=\"http://cdnjs.cloudflare.com/ajax/libs/morris.js/0.5.1/morris.min.js\"></script>\n";
	stream << "</head><body><div id=\"" << name_c << "-graph\"></div>\n<script>\n";
	stream << "function updateLiveGraph(" << name_c << "Graph) {\n";
	stream << "  $.getJSON('"+basePath << name_c << "/" << id.c_str() << "/history', function(results) { " << name_c;
	stream << "Graph.setData(results); });\n}\n" << name_c << "Graph = new Morris."<< morrisType.c_str() << "({element: '" << name_c;
	stream << "-graph',  data: [], xkey: 'timestamp', pointSize:0,fillOpacity:0.3," << morrisOpts.c_str();
	stream << ressources[id]->getMorrisDesc().c_str() << "});\n";
	stream << "updateLiveGraph(" << name_c << "Graph);setInterval(function() { updateLiveGraph(" << name_c;
	stream << "Graph); }, " << (*cfg)["history"].asString().c_str() << ");\n";
	stream << "</script></body></html>\n";
        setResponseHtml(response, stream.str());
}

void Collector::doGetHistory(response_ptr response, request_ptr request) {
	string name   = (*request)[0];
	double since  = -1;
	if (request->size()>1) {
		try {
			since=stod((*request)[1]);
		} catch (std::exception &e) { }
	}
        /*if (request.query().has("since"))
		since= stod(request.query().get("since").get());*/

	map<string, std::shared_ptr<Ressource> >::const_iterator it = ressources.find(name);
	if( it == ressources.end()) {
		setResponse404(response, "No such Metric");
		return;
	}
	setResponseJson(response, ressources[name]->getHistory(since));
}

void Collector::getIndexHtml(std::stringstream& stream ){
	if ((*cfg)["enable"].asBool()) {
		stream << "<h3>" << name.c_str() << "</h3><ul>\n";
		for(map<string, std::shared_ptr<Ressource> >::const_iterator i = ressources.begin();i!=ressources.end();i++) {
			stream << "<li><a href=\""+basePath << name.c_str() << "/" << i->first.c_str() << "/graph\">" << desc[i->first].c_str() << "</a></li>\n";
		}
		stream << "</ul>\n";
	}
}


/*********************************
 * CollectorsManager
 */

map<string, collector_maker_t *> collectorFactory;

void CollectorsManager::getIndexHtml(std::stringstream& stream ) {
	for(std::map<std::string, std::shared_ptr<Collector> >::iterator i = collectors.begin();i != collectors.end();i++) {
		i->second->getIndexHtml(stream);
	} 
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
	map<string, collector_maker_t *, less<string> >::iterator factit;
	DIR *dir;
	const string directory = (*servCfg)["collectors_cpp"].asString();
	class dirent *ent;
	class stat st;
	void *dlib;

	dir = opendir(directory.c_str());
	while ((ent = readdir(dir)) != NULL) {
		const string file_name = ent->d_name;
		const string full_file_name = directory + "/" + file_name;

		if (file_name[0] == '.')
			continue;
		if (stat(full_file_name.c_str(), &st) == -1)
			continue;
		const bool is_directory = (st.st_mode & S_IFDIR) != 0;
		if (is_directory)
			continue;


		if (file_name.substr(file_name.rfind(".")) == ".so") {
			dlib = dlopen(full_file_name.c_str(), RTLD_NOW);
			if(dlib == NULL){
				cerr << dlerror() << endl; 
				exit(-1);
			}
		}
	}
	closedir(dir);
	
	// Instanciate the plugins classes
	for(factit = collectorFactory.begin();factit != collectorFactory.end(); factit++) {
		collectors[factit->first]	= factit->second(server, cfg->getCollector(factit->first));
	}

	// Load the lua plugins
	const string dirlua = (*servCfg)["collectors_lua"].asString();
	dir = opendir(dirlua.c_str());
	while ((ent = readdir(dir)) != NULL) {
		const string file_name = ent->d_name;
		const string name = file_name.substr(0,file_name.rfind("."));
		const string full_file_name = dirlua + "/" + file_name;

		if (file_name[0] == '.')
			continue;
		if (stat(full_file_name.c_str(), &st) == -1)
			continue;
		const bool is_directory = (st.st_mode & S_IFDIR) != 0;
		if (is_directory)
			continue;


		if (file_name.substr(file_name.rfind(".")) == ".lua") {
			collectors[name] = std::make_shared<LuaCollector>(server, cfg->getCollector(name), full_file_name);
		}
	}
	closedir(dir);
}

void CollectorsManager::startThreads() {
	for(std::map<std::string, std::shared_ptr<Collector> >::iterator i = collectors.begin();i != collectors.end();i++)
		i->second->startThread();
}

CollectorsManager::~CollectorsManager() {
	// TODO free the ressources here
}

}
