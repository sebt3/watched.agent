#include "agent.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h> 

namespace watcheD {

/*********************************
 * ServicesManager
 */
std::map<std::string, detector_maker_t* > detectorFactory;
std::map<std::string, enhancer_maker_t* > enhancerFactory;
std::map<std::string, handler_maker_t*  >  handlerFactory;
std::map<std::string, service_maker_t*  >  serviceFactory;
std::map<std::string, std::pair<parser_maker_t*,std::string> >		  parserFactory;
std::map<std::string, std::pair<service_collector_maker_t*,std::string> > serviceCollectorFactory;
servicesManager::servicesManager(std::shared_ptr<HttpServer> p_server, std::shared_ptr<Config> p_cfg) : active(false), server(p_server), cfg(p_cfg) {}
void servicesManager::init() {
	Json::Value*		servCfg	  = cfg->getPlugins();
	DIR *			dir;
	std::string		directory = (*cfg->getServices())["config_dir"].asString();
	struct dirent*		ent;
	struct stat 		st;
	void*			dlib;

	// Load the services configuration
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

			if (file_name.substr(file_name.rfind(".")) == ".json") {
				services.push_back(std::make_shared<service>(server, full_file_name));
			}
		}
		closedir(dir);
	}  else	server->logWarning("servicesManager::", directory+" doesnt exist. No services configuration will be loaded");

	// Load the services plugins
	directory = (*servCfg)["services_cpp"].asString();
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
				if(dlib == NULL){
					server->logError("servicesManager::", std::string(dlerror())+" while loading "+full_file_name); 
					//exit(-1);
				}
			}
		}
		closedir(dir);
	} else	server->logWarning("servicesManager::", directory+" doesnt exist. No services plugins will be used");

	// Instanciate the detector classes
	for(std::map<std::string, detector_maker_t* >::iterator factit = detectorFactory.begin();factit != detectorFactory.end(); factit++)
		detectors.push_back(factit->second(shared_from_this(), server));

	// then the enhancer
	for(std::map<std::string, enhancer_maker_t* >::iterator factit = enhancerFactory.begin();factit != enhancerFactory.end(); factit++)
		enhancers.push_back(factit->second(shared_from_this()));

	// Load the lua plugins
	const std::string dirlua = (*servCfg)["services_lua"].asString();
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

			if (file_name.substr(file_name.rfind(".")) == ".lua") {
				std::shared_ptr<LuaSorter> q = std::make_shared<LuaSorter>(full_file_name);
				if (q->isType("collector") && (serviceCollectorFactory.find(name) == serviceCollectorFactory.end())) {
					serviceCollectorFactory[name] = std::make_pair(
						[](std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_s, const std::string p_fname) -> std::shared_ptr<Collector> {
							return std::make_shared<LuaCollector>(p_srv, p_cfg, p_fname, p_s);
						}, full_file_name);
				}
				if (q->isType("logParser") && (parserFactory.find(name) == parserFactory.end())) {
					parserFactory[name] = std::make_pair(
						[](const std::string p_logname, Json::Value* p_cfg, const std::string p_luafile) -> std::shared_ptr<logParser> {
							return std::make_shared<LuaParser>(p_logname, p_cfg, p_luafile);
						}, full_file_name);
				}
				if (q->isType("enhancer")) {
					enhancers.push_back(std::make_shared<LuaServiceEnhancer>(shared_from_this(), full_file_name));
				}
				if (q->isType("handler")) {
					// TODO: store the information so it could be reused later
				}
				if (q->isType("detector")) {
					// TODO: Write that LuaDetector class
					// detectors.push_back(std::make_shared<LuaDetector>(shared_from_this(), server, full_file_name));
				}
			}
		}
		closedir(dir);
	} else	server->logWarning("servicesManager::", dirlua+" doesnt exist. No lua services plugins will be used");

	detectors.push_back(std::make_shared<socketDetector>(shared_from_this(), server));

	systemCollectors	= std::make_shared<CollectorsManager>(server,cfg);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	associate(server,"GET","^/$",doGetRootPage);
	associate(server,"GET","^/service/(.*)/status$",doGetServiceStatus);
	associate(server,"GET","^/service/(.*)/log$",doGetServiceLog);
	associate(server,"GET","^/service/(.*)/log.since=([0-9.]*)$",doGetServiceLog);
	associate(server,"GET","^/service/(.*)/html$",doGetServiceHtml);
	associate(server,"GET","^/service/(.*)/(.*)/(.*)/history$",doGetCollectorHistory);
	associate(server,"GET","^/service/(.*)/(.*)/(.*)/history.since=([0-9.]*)$",doGetCollectorHistory);
	associate(server,"GET","^/service/(.*)/(.*)/(.*)/graph$",doGetCollectorGraph);
	associate(server,"GET","^/api/swagger.json$",doGetJson);
	server->logInfo("servicesManager::", "watched.agent started and ready.");
}

servicesManager::~servicesManager() {
	if (active) {
		active=false;
		timer.kill();
		my_thread.join();
	}
}

void servicesManager::startThreads() {
	systemCollectors->startThreads();
	active = true;
	my_thread = std::thread ([this]() {
		Json::Value*		servCfg	  = cfg->getServices();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		do {
			find();
		} while (active && timer.wait_for(std::chrono::seconds((*servCfg)["find_frequency"].asInt())) && active);
		
	});

}

void servicesManager::addService(std::shared_ptr<service> p_serv) {
	std::shared_ptr<service> add = p_serv;
	// try to improve the service
	for(std::vector<std::shared_ptr<serviceEnhancer> >::iterator i=enhancers.begin();i!=enhancers.end();i++) {
		std::shared_ptr<service> s = (*i)->enhance(p_serv);
		if (s!=NULL) {
			add=s;
			break;
		}
	}

	// Detect if the service isnt already known
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++){
		if ( *add == *(*i) ) {
			(*i)->updateFrom(add);
			return;
		}
	}
	// Detect if this is a missing part of a service
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++){
		if ((*i)->needSocketFrom(add) ) {
			(*i)->updateFrom(add);
			return;
		}
	}
	// save the template config
	Json::Value*		servCfg	  = cfg->getServices();
	add->saveConfigTemplate((*servCfg)["config_dir"].asString());
	// adding the service to the list
	services.push_back(add);
}

void servicesManager::find() {
	for (std::vector< std::shared_ptr<serviceDetector> >::iterator i=detectors.begin();i!=detectors.end();i++)
		(*i)->find();
}

bool	servicesManager::haveSocket(uint32_t p_socket_id) {
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++)
		if ((*i)->haveSocket(p_socket_id)) return true;
	return false;
}

std::shared_ptr<service> servicesManager::enhanceFromFactory(std::string p_id, std::shared_ptr<service> p_serv) {
	for(std::map<std::string, service_maker_t* >::iterator i=serviceFactory.begin();i!=serviceFactory.end();i++) {
		if (i->first == p_id)
			return i->second(*p_serv);
	}
	return NULL;
}

bool	servicesManager::havePID(uint32_t p_pid) {
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++)
		if ((*i)->havePID(p_pid)) return true;
	return false;
}

std::shared_ptr<service> servicesManager::getService(uint32_t p_pid) {
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++)
		if ((*i)->havePID(p_pid)) return *i;
	return nullptr;
}

void	servicesManager::handleSubProcess(std::shared_ptr<process> p_p) {
	uint32_t ppid = p_p->getPPID();
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
		if ((*i)->havePID(ppid)) {
			(*i)->addSubProcess(p_p);
			return;
		}
	}
}

void	servicesManager::doGetServiceStatus(response_ptr response, request_ptr request) {
	std::stringstream ss;
	std::string id   = (*request)[0];
	Json::Value obj(Json::objectValue);
	Json::Value res(Json::objectValue);
	if (id == "all") {
		for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
			res[(*i)->getID()] = obj;
			(*i)->getJsonStatus( &(res[(*i)->getID()]) );
		}
		ss << res;
		setResponseJson(response, ss.str());
		return;
	}

	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
		if ( *(*i) == id) {
			(*i)->getJsonStatus( &res );
			ss << res;
			setResponseJson(response, ss.str());
			return;
		}
	}
	// returning an error
	res["error"] = "No service matching '"+id+"' found";
	ss << res;
	setResponseJson(response, ss.str());
}

void	servicesManager::doGetServiceLog(response_ptr response, request_ptr request) {
	std::stringstream ss;
	std::string id  = (*request)[0];
	double since  = -1;
	if (request->size()>1) {
		try {
			since=stod((*request)[1]);
		} catch (std::exception &e) { }
	}
	Json::Value obj(Json::objectValue);
	Json::Value res(Json::objectValue);
	if (id == "all") {
		for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
			res[(*i)->getID()] = obj;
			(*i)->getJsonLogHistory( &(res[(*i)->getID()]), since);
		}
		ss << res;
		setResponseJson(response, ss.str());
		return;
	}

	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
		if ( *(*i) == id) {
			res = obj;
			(*i)->getJsonLogHistory( &res, since);
			ss << res;
			setResponseJson(response, ss.str());
			return;
		}
	}
	// returning an error
	res["error"] = "No service matching '"+id+"' found";
	ss << res;
	setResponseJson(response, ss.str());
}


void	servicesManager::doGetServiceHtml(response_ptr response, request_ptr request) {
	std::stringstream ss;
	std::string id   = (*request)[0];
	Json::Value res(Json::objectValue);
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
		if ( *(*i) == id) {
			ss << server->getHead("Service", (*i)->getID());
			(*i)->getJsonStatus( &res );
			ss << "<div class=\"row\"><div class=\"col-md-4\"><div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">Properties</h3></div><div class=\"box-body\"><table class=\"table table-striped table-hover\"><tr><th>name</th><td>"+res["properties"]["name"].asString()+"</td></tr><tr><th>type</th><td>"+res["properties"]["type"].asString()+"</td></tr><tr><th>subType</th><td>"+res["properties"]["subType"].asString()+"</td></tr><tr><th>Identifier</th><td>"+res["properties"]["uniqName"].asString()+"</td></tr><tr><th>hostname</th><td>"+res["properties"]["host"].asString()+"</td></tr></table></div></div>";
			if (res["sockets"].size()>0) {
				ss << "<div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">Sockets</h3></div><div class=\"box-body\"><table class=\"table table-striped table-hover\"><thead><tr><th>listen</th><th>status</th></tr></thead><tbody>\n";
				for (Json::Value::iterator j = res["sockets"].begin();j!=res["sockets"].end();j++) {
					std::string color = "green";
					if ((*j)["status"].asString() != "ok") 
						color = "red";
					ss << "<tr><td>"+(*j)["name"].asString()+"</td><td class=\"text-"+color+"\">"+(*j)["status"].asString()+"</td></tr>\n";
				}
				ss << "</tbody></table></div></div>";
			}
			ss << "</div><div class=\"col-md-8\"><div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">Process</h3></div><div class=\"box-body\"><table class=\"table table-striped table-hover\"><thead><tr><th>name</th><th>path</th><th>cwd</th><th>username</th><th>PID</th><th>status</th></tr></thead><tbody>\n";
			for (Json::Value::iterator j = res["process"].begin();j!=res["process"].end();j++) {
				std::string color = "green";
				if ((*j)["status"].asString() != "ok") 
					color = "red";
				ss << "<tr><td>"+(*j)["name"].asString()+"</td><td>"+(*j)["full_path"].asString()+"</td><td>"+(*j)["cwd"].asString()+"</td><td>"+(*j)["username"].asString()+"</td><td>"+(*j)["pid"].asString()+"</td><td class=\"text-"+color+"\">"+(*j)["status"].asString()+"</td></tr>\n";
			}
			ss << "</tbody></table></div></div>";
			if (res["subprocess"].size()>0) {
				ss << "<div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">subProcess</h3></div><div class=\"box-body\"><table class=\"table table-striped table-hover\"><thead><tr><th>name</th><th>path</th><th>cwd</th><th>username</th><th>PID</th></tr></thead><tbody>\n";
				for (Json::Value::iterator j = res["subprocess"].begin();j!=res["subprocess"].end();j++) {
					std::string color = "green";
					if ((*j)["status"].asString() != "ok") 
						color = "red";
					ss << "<tr><td>"+(*j)["name"].asString()+"</td><td>"+(*j)["full_path"].asString()+"</td><td>"+(*j)["cwd"].asString()+"</td><td>"+(*j)["username"].asString()+"</td><td>"+(*j)["pid"].asString()+"</td><td class=\"text-"+color+"\">"+(*j)["status"].asString()+"</td></tr>\n";
				}
				ss << "</tbody></table></div></div>";
			}
			(*i)->getJsonLogHistory( &res, -1 );
			if (res["entries"].size()>0) {
				ss << "<div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">Log entries</h3></div><div class=\"box-body\"><table class=\"table table-striped table-hover\"><thead><tr><th>level</th><th>date</th><th>source</th><th>line #</th><th>text</th></tr></thead><tbody>\n";
				for (Json::Value::iterator j = res["entries"].begin();j!=res["entries"].end();j++) {
					ss << "<tr><td>"+(*j)["level"].asString()+"</td><td>"+(*j)["date_mark"].asString()+"</td><td>"+(*j)["source"].asString()+"</td><td>"+(*j)["line_no"].asString()+"</td><td>"+(*j)["text"].asString()+"</td></tr>\n";
				}
				ss << "</tbody></table></div></div>";
			}
			ss << "</div></div>\n<div class=\"row\">";
			(*i)->getCollectorsHtml(ss);
			ss << "</div>\n";
			ss << server->getFoot("");
			setResponseHtml(response, ss.str());
			return;
		}
	}
	// returning an error
	setResponse404(response, "No service matching '" + id + "' found");
}

void	servicesManager::doGetCollectorHistory(response_ptr response, request_ptr request) {
	std::stringstream ss;
	std::string id   = (*request)[0];
	std::string cid   = (*request)[1];
	Json::Value res(Json::objectValue);
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
		if ( *(*i) == id) {
			// TODO look for the collector and get its response here
			std::shared_ptr<Collector> c = (*i)->getCollector(cid);
			if (c == nullptr) {
				res["error"] = "No collector matching '"+cid+"' for service '"+id+"' found";
				ss << res;
				setResponseJson(response, ss.str());
				return;
			}
			request->erase(request->begin());
			request->erase(request->begin());
			c->doGetHistory(response,request);
			return;
		}
	}
	// returning an error
	res["error"] = "No service matching '"+id+"' found";
	ss << res;
	setResponseJson(response, ss.str());
}

void	servicesManager::doGetCollectorGraph(response_ptr response, request_ptr request) {
	std::stringstream ss;
	std::string id   = (*request)[0];
	std::string cid   = (*request)[1];
	Json::Value res(Json::objectValue);
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++) {
		if ( *(*i) == id) {
			std::shared_ptr<Collector> c = (*i)->getCollector(cid);
			if (c == nullptr) {
				res["error"] = "No collector matching '"+cid+"' for service '"+id+"' found";
				ss << res;
				setResponseJson(response, ss.str());
				return;
			}
			request->erase(request->begin());
			request->erase(request->begin());
			c->doGetGraph(response,request);
			return;
		}
	}
	// returning an error
	setResponse404(response, "No service matching '" + id + "' found");
}

void servicesManager::doGetRootPage(response_ptr response, request_ptr request) {
	std::stringstream stream;
        stream << server->getHead("Home");
	stream << "<div class=\"row\"><div class=\"col-md-3\"><div class=\"box box-default\"><div class=\"box-header with-border\"><h3 class=\"box-title\">Services</h3></div><div class=\"box-body\">\n";

	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++)
		(*i)->getIndexHtml(stream);

	stream << "</div></div></div>\n";

	systemCollectors->getIndexHtml(stream);

	stream << "</div>\n";
        stream << server->getFoot("");
        setResponseHtml(response, stream.str());
}

void servicesManager::doGetJson(response_ptr response, request_ptr request) {
	Json::Value res(Json::objectValue);
	Json::StreamWriterBuilder wbuilder;
	Json::Value obj(Json::objectValue);
	std::string document;
	
	res["swagger"]				= "2.0";
	res["info"]["version"]			= "1.0.0";
	res["info"]["title"]			= APPS_NAME;
	res["info"]["description"]		= APPS_DESC;
	//res["info"]["termsOfService"]		= "http://some.url/terms/";
	res["info"]["contact"]["name"]		= "Sebastien Huss";
	res["info"]["contact"]["email"]		= "sebastien.huss@gmail.com";
	res["info"]["contact"]["url"]		= "https://sebt3.github.io/watched/";
	res["info"]["license"]["name"]		= "AGPL";
	res["info"]["license"]["url"]		= "https://www.gnu.org/licenses/agpl-3.0.html";
	// add swaggerised docs
	res["externalDocs"]["description"]	= "Documentation";
	res["externalDocs"]["url"]		= "https://sebt3.github.io/watched/doc/";
	
	res["host"]				= "localhost";
	res["basePath"]				= "/";
	res["schemes"][0]			= "http";
	res["consumes"][0]			= "application/json";
	res["produces"][0]			= "application/json";
	res["definitions"]["services"]["type"]	= "object";

	systemCollectors->getJson(&res);
	res["definitions"]["serviceProcess"]["type"]				= "object";
	res["definitions"]["serviceProcess"]["properties"]["cwd"]["type"]	= "string";
	res["definitions"]["serviceProcess"]["properties"]["full_path"]["type"]	= "string";
	res["definitions"]["serviceProcess"]["properties"]["name"]["type"]	= "string";
	res["definitions"]["serviceProcess"]["properties"]["pid"]["type"]	= "number";
	res["definitions"]["serviceProcess"]["properties"]["status"]["type"]	= "string";
	res["definitions"]["serviceProcess"]["properties"]["username"]["type"]	= "string";
	res["definitions"]["serviceProcess"]["requiered"][0]			= "name";
	res["definitions"]["serviceProcess"]["requiered"][1]			= "status";
	res["definitions"]["serviceProcess"]["requiered"][1]			= "status";
	res["definitions"]["serviceProcess"]["x-isService"]			= false;
	res["definitions"]["serviceSocket"]["properties"]["name"]["type"]	= "string";
	res["definitions"]["serviceSocket"]["properties"]["status"]["type"]	= "string";
	res["definitions"]["serviceSocket"]["requiered"][0]			= "name";
	res["definitions"]["serviceSocket"]["requiered"][1]			= "status";
	res["definitions"]["serviceSocket"]["x-isService"]			= false;
	res["definitions"]["serviceLog"]["properties"]["level"]["type"]		= "string";
	res["definitions"]["serviceLog"]["properties"]["date_mark"]["type"]	= "string";
	res["definitions"]["serviceLog"]["properties"]["line_no"]["type"]	= "number";
	res["definitions"]["serviceLog"]["properties"]["source"]["type"]	= "string";
	res["definitions"]["serviceLog"]["properties"]["text"]["type"]		= "string";
	res["definitions"]["serviceLog"]["properties"]["timestamp"]["format"]	= "double";
	res["definitions"]["serviceLog"]["properties"]["timestamp"]["type"]	= "number";
	res["definitions"]["serviceLog"]["requiered"][0]			= "level";
	res["definitions"]["serviceLog"]["requiered"][1]			= "date_mark";
	res["definitions"]["serviceLog"]["requiered"][2]			= "line_no";
	res["definitions"]["serviceLog"]["requiered"][3]			= "timestamp";
	res["definitions"]["serviceLog"]["x-isService"]				= false;
	res["definitions"]["services"]["properties"]["host"]["type"]		= "string";
	res["definitions"]["services"]["properties"]["process"]["type"]		= "array";
	res["definitions"]["services"]["properties"]["process"]["items"]["type"]= "#/definitions/serviceProcess";
	res["definitions"]["services"]["properties"]["sockets"]["type"]		= "array";
	res["definitions"]["services"]["properties"]["sockets"]["items"]["type"]= "#/definitions/serviceSocket";
	res["definitions"]["services"]["requiered"][0]				= "sockets";
	res["definitions"]["services"]["requiered"][1]				= "process";
	res["definitions"]["services"]["requiered"][2]				= "host";
	res["definitions"]["services"]["x-isService"]				= false;
	res["paths"]["/service/all/status"]["get"]["responses"]["200"]["description"] = "All services status";
	res["paths"]["/service/all/status"]["get"]["responses"]["200"]["schema"]["type"] = "object";
	res["paths"]["/service/all/status"]["get"]["responses"]["200"]["schema"]["additionalProperties"]["$ref"] = "#/definitions/services";
	res["paths"]["/service/all/status"]["get"]["summary"]			= "All services status";
	res["paths"]["/service/all/log"]["get"]["responses"]["200"]["description"] = "All services log events";
	res["paths"]["/service/all/log"]["get"]["responses"]["200"]["schema"]["type"] = "object";
	res["paths"]["/service/all/log"]["get"]["responses"]["200"]["schema"]["additionalProperties"]["$ref"] = "#/definitions/serviceLog";
	res["paths"]["/service/all/log"]["get"]["summary"]			= "All services status";
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++)
		(*i)->getJson(&res);

	wbuilder["indentation"] = "\t";
	document = Json::writeString(wbuilder, res);

	setResponseJson(response, document);
}

/*********************************
 * Detectors  
 */
void socketDetector::find(void) {
	std::shared_ptr<servicesManager> mgr = services.lock();
	uint16_t count = 0;
	uint16_t coun2 = 0;
	if (!mgr) return;
	std::map<std::string,std::string> files;
	files["tcp"]  = "/proc/net/tcp";
	files["udp"]  = "/proc/net/udp";
	files["tcp6"] = "/proc/net/tcp6";
	files["udp6"] = "/proc/net/udp6";

	std::vector< std::shared_ptr<socket> > sockets;
	for (std::map<std::string,std::string>::iterator i=files.begin();i!=files.end();i++) {
		std::ifstream infile(i->second);
		std::string line;
		while(infile.good() && getline(infile, line)) {
			std::istringstream iss(line);
			std::istream_iterator<std::string> beg(iss), end;
			std::vector<std::string> tokens(beg,end);
			if (tokens[3] != "0A" && i->first.substr(0,3) == "tcp")
				continue; // keep only listening sockets
			if (tokens[3] != "07" && i->first.substr(0,3) == "udp")
				continue; // keep only listening sockets
			sockets.push_back(std::make_shared<socket>(line, i->first));
		}
		if (infile.good())
			infile.close();
	}

	if (sockets.size()<1) 
		return;
	// find all processes
	DIR *dir;
	struct dirent *ent;
	//std::vector< std::shared_ptr<process> > processes;
	std::string file;

	if ((dir = opendir ("/proc/")) == NULL) return;
	while ((ent = readdir (dir)) != NULL) {
		file = ent->d_name;
		if (!std::all_of(file.begin(), file.end(), ::isdigit)) continue;
		uint32_t pid = atoi(file.c_str());
		if (mgr->havePID(pid)) {
			// process already associated to a service, updating the known socket list
			std::shared_ptr<service> serv = mgr->getService(pid);
			if(!serv) continue;
			std::shared_ptr<process> p = serv->getProcess(pid);
			if(!p) continue;
			p->setSockets();
			for(std::vector< std::shared_ptr<socket> >::iterator i = sockets.begin(); i != sockets.end();i++) {
				if (!serv->haveSocket((*i)->getID())) continue;
				serv->setSocket(*i);
				coun2++;
			}
			continue;
		}
		std::shared_ptr<process> p = std::make_shared<process>(pid);
		if (mgr->havePID(p->getPPID())) {
			mgr->handleSubProcess(p);
			continue;
		}
		p->setSockets();

		for(std::vector< std::shared_ptr<socket> >::iterator it = sockets.begin(); it != sockets.end(); ++it) {
			if (!p->haveSocket((*it)->getID())) continue;
			// socket found, create the service
			std::shared_ptr<service> serv = std::make_shared<service>(server);
			serv->setSocket(*it);
			serv->addMainProcess(p);
			// trying to associate more sockets to this service
			for(std::vector< std::shared_ptr<socket> >::iterator i = sockets.begin(); i != sockets.end();i++) {
				if (!serv->haveSocket((*i)->getID())) continue;
				serv->setSocket(*i);
			}
			mgr->addService(serv);
			count++;
			break;
		}
	}
	closedir (dir);
	server->logNotice("socketDetector::find", "found "+std::to_string(count)+" services, updated "+std::to_string(coun2));
	// TODO: do something with the non-matching sockets too
}

}
