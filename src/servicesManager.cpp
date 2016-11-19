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


servicesManager::servicesManager(std::shared_ptr<HttpServer> p_server, std::shared_ptr<Config> p_cfg) : active(false), server(p_server), cfg(p_cfg) {}
void servicesManager::init() {
	Json::Value*		servCfg	  = cfg->getPlugins();
	DIR *			dir;
	std::string		directory = (*cfg->getServices())["config_dir"].asString();
	class dirent*		ent;
	class stat 		st;
	void*			dlib;

	// Load the services configuration
	dir = opendir(directory.c_str());
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
			services.push_back(std::make_shared<service>(full_file_name));
		}
	}
	closedir(dir);

	// Load the services plugins
	directory = (*servCfg)["services_cpp"].asString();
	dir = opendir(directory.c_str());
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
				std::cerr << dlerror() << std::endl; 
				exit(-1);
			}
		}
	}
	closedir(dir);
	
	// Instanciate the detector classes
	for(std::map<std::string, detector_maker_t* >::iterator factit = detectorFactory.begin();factit != detectorFactory.end(); factit++)
		detectors.push_back(factit->second(shared_from_this(), server));

	// then the enhancer
	for(std::map<std::string, enhancer_maker_t* >::iterator factit = enhancerFactory.begin();factit != enhancerFactory.end(); factit++)
		enhancers.push_back(factit->second(shared_from_this()));

	detectors.push_back(std::make_shared<socketDetector>(shared_from_this(), server));
	systemCollectors	= std::make_shared<CollectorsManager>(server,cfg);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	associate(server,"GET","^/$",doGetRootPage);
	associate(server,"GET","^/service/(.*)/status$",doGetServiceStatus);
	associate(server,"GET","^/api/swagger.json$",doGetJson);
}

void servicesManager::startThreads() {
	systemCollectors->startThreads();
	active = true;
	my_thread = std::thread ([this]() {
		Json::Value*		servCfg	  = cfg->getServices();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		do {
			find();
			std::this_thread::sleep_for(std::chrono::seconds((*servCfg)["find_frequency"].asInt()));
		} while (active);
		
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


void	servicesManager::doGetServiceStatus(response_ptr response, request_ptr request) {
	std::stringstream ss;
	std::string id   = request->path_match[1];
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

void servicesManager::doGetRootPage(response_ptr response, request_ptr request) {
	std::stringstream stream;
        stream << "<html><head><title>" << APPS_NAME.c_str() << "</title>\n";
	stream << "</head><body><table width=100%><tr><td><h1>Services</h1>\n";
	for (std::vector< std::shared_ptr<service> >::iterator i=services.begin();i!=services.end();i++)
		(*i)->getIndexHtml(stream);

	stream << "</td><td><h1>System</h1>\n";
	systemCollectors->getIndexHtml(stream);
	stream << "</td></tr></table></body></html>\n";
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
	res["definitions"]["serviceSocket"]["type"]				= "object";
	res["definitions"]["serviceSocket"]["properties"]["name"]["type"]	= "string";
	res["definitions"]["serviceSocket"]["properties"]["status"]["type"]	= "string";
	res["definitions"]["serviceSocket"]["requiered"][0]			= "name";
	res["definitions"]["serviceSocket"]["requiered"][1]			= "status";
	res["definitions"]["services"]["properties"]["host"]["type"]		= "string";
	res["definitions"]["services"]["properties"]["process"]["type"]		= "array";
	res["definitions"]["services"]["properties"]["process"]["items"]["type"]= "#/definitions/serviceProcess";
	res["definitions"]["services"]["properties"]["sockets"]["type"]		= "array";
	res["definitions"]["services"]["properties"]["sockets"]["items"]["type"]= "#/definitions/serviceSocket";
	res["definitions"]["services"]["requiered"][0]				= "sockets";
	res["definitions"]["services"]["requiered"][1]				= "process";
	res["definitions"]["services"]["requiered"][2]				= "host";
	res["paths"]["/service/all/status"]["get"]["responses"]["200"]["description"] = "All services status";
	res["paths"]["/service/all/status"]["get"]["responses"]["200"]["schema"]["type"] = "object";
	res["paths"]["/service/all/status"]["get"]["responses"]["200"]["schema"]["additionalProperties"]["$ref"] = "#/definitions/services";
	res["paths"]["/service/all/status"]["get"]["summary"]			= "All services status";
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
	//TODO: add suport for ipv6 sockets

	std::shared_ptr<servicesManager> mgr = services.lock();
	if (!mgr) return;
	// find all LISTEN TCP sockets
	std::ifstream infile("/proc/net/tcp");
	std::string line;
	std::vector< std::shared_ptr<socket> > sockets;
	while(infile.good() && getline(infile, line)) {
		std::istringstream iss(line);
		std::istream_iterator<std::string> beg(iss), end;
		std::vector<std::string> tokens(beg,end);
		if (tokens[3] != "0A")
			continue; // keep only listening sockets
		if (mgr->haveSocket(atoi(tokens[9].c_str())))
			continue; // socket already associated to a service
		sockets.push_back(std::make_shared<socket>(line, "tcp"));
	}
	if (infile.good())
		infile.close();

	//Find all "listening" UDP sockets
	std::ifstream unfile("/proc/net/udp");
	while(unfile.good() && getline(unfile, line)) {
		std::istringstream iss(line);
		std::istream_iterator<std::string> beg(iss), end;
		std::vector<std::string> tokens(beg,end);
		if (tokens[3] != "07")
			continue;
		sockets.push_back(std::make_shared<socket>(line, "udp"));
	}
	if (unfile.good())
		unfile.close();

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
		if (mgr->havePID(atoi(file.c_str())))
			continue; // process already associated to a service
		std::shared_ptr<process> p = std::make_shared<process>(atoi(file.c_str()));
		p->setSockets();
		for(std::vector< std::shared_ptr<socket> >::iterator it = sockets.begin(); it != sockets.end(); ++it) {
			if (!p->haveSocket((*it)->getID())) continue;
			// socket found, create the service
			std::shared_ptr<service> serv = std::make_shared<service>();
			serv->setSocket(*it);
			serv->addMainProcess(p);
			// remove the now associated socket from the list
			it = sockets.erase(it);
			// trying to associate more sockets to this service
			for(std::vector< std::shared_ptr<socket> >::iterator i = sockets.begin(); i != sockets.end(); ) {
				if (!serv->haveSocket((*i)->getID())) {++i;continue; }
				serv->setSocket(*i);
				i = sockets.erase(i);
			}
			mgr->addService(serv);
			break;
		}
	}
	closedir (dir);
	// TODO: do something with the non-matching sockets too
}

}
