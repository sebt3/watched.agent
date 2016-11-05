#include "agent.h"
using namespace watcheD;

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

/*********************************
 * Sockets
 */

socket::socket(std::string p_line, std::string p_type) : type(p_type) {
	std::istringstream iss(p_line);
	std::istream_iterator<std::string> beg(iss), end;
	std::vector<std::string> tokens(beg,end);
	setSockFrom(tokens[1], &(source));
	setSockFrom(tokens[2], &(dest));
	id = atoi(tokens[9].c_str());
}

socket::socket(std::string p_source) {
	char dot;
	type = p_source.substr(0,p_source.find(":"));
	auto pos = p_source.find(":")+3;
	std::string port = p_source.substr(p_source.find(":",pos)+1);
	source.port  = strtoul(port.c_str(), NULL, 10);
	std::string ip = p_source.substr(pos, p_source.find(":",pos)-pos);
	std::istringstream s(ip);
	s>>source.ip1>>dot>>source.ip2>>dot>>source.ip3>>dot>>source.ip4;
	//std::cout << "type = " <<type<< ", ip = " <<ip<< " port= " <<source.port<< std::endl;
}
void socket::setSockFrom(std::string src, struct sock_addr *sa) {
	std::string port = src.substr(src.find(":")+1, src.length());
	sa->port = strtoul(port.c_str(), NULL, 16);
	sa->ip4  = strtoul(src.substr(0,2).c_str(), NULL, 16);
	sa->ip3  = strtoul(src.substr(2,2).c_str(), NULL, 16);
	sa->ip2  = strtoul(src.substr(4,2).c_str(), NULL, 16);
	sa->ip1  = strtoul(src.substr(6,2).c_str(), NULL, 16);
}

std::string socket::getSource() {
	std::string ret = type+"://"+std::to_string(source.ip1)+"."+std::to_string(source.ip2)+"."+std::to_string(source.ip3)+"."+std::to_string(source.ip4)+":"+std::to_string(source.port);
	return ret;
}

/*********************************
 * Process
 */
process::process(uint32_t p_pid): pid(p_pid) {
	char buf[512];
	std::string file;
	// get the full_path & base_name
	file="/proc/"+std::to_string(pid)+"/exe";
	int count = readlink(file.c_str(), buf, sizeof(buf));
	if(count>0) {
		buf[count] = '\0';
		full_path = buf;
		base_name = full_path.substr(full_path.rfind("/")+1);
	}
}

process::process(std::string p_fullpath):pid(0), full_path(p_fullpath) {
	base_name = full_path.substr(full_path.rfind("/")+1);
}

void process::setSockets() {
	// get all its opened sockets
	DIR *dir;
	char buf[512];
	struct dirent *ent;
	std::stringstream path;
	std::string file, link;
	const std::string soc("socket:[");
	path << "/proc/" << pid << "/fd/";
	do {
	if ((dir = opendir (path.str().c_str())) == NULL) break;
	while ((ent = readdir (dir)) != NULL) {
		// looping throw all /proc/[pid]/fd/[fd]
		if (ent->d_name[0] == '.') continue;
		file = path.str() + ent->d_name;
		int count = readlink(file.c_str(), buf, sizeof(buf));
		if (count<=0) continue;
		buf[count] = '\0';
		link = buf;
		// check if it's a socket
		if (link.compare(0, soc.length(), soc) !=0)
			continue;
		sockets.push_back(atoi(link.substr(soc.length()).c_str()));
	}
	}while(false);
	closedir (dir);
}

bool process::haveSocket(uint32_t p_socket_id) {
	for(std::vector<uint32_t>::iterator i=sockets.begin();i!=sockets.end();i++) {
		if (*i == p_socket_id)
			return true;
	}
	return false;
}

bool process::getStatus() {
	std::string p = "/proc/"+std::to_string(pid);
	struct stat buffer;
	return (stat (p.c_str(), &buffer) == 0); 
}


/*********************************
 * Services
 */
service::service(std::shared_ptr<HttpServer> p_server): name(""), type("unknown"), uniqName(""), cfg(Json::objectValue), handler(NULL), server(p_server) {
}

service::service(std::shared_ptr<HttpServer> p_server, std::string p_file_path): name(""), type("unknown"), uniqName(""), cfg(Json::objectValue), handler(NULL), cfgFile(p_file_path), server(p_server) {
	std::ifstream cfgif (cfgFile);
	if (cfgif.good()) {
		cfgif >> cfg;
		cfgif.close();
	}
	// sockets have to be loaded before process so the registered name is correct
	if (cfg.isMember("sockets")) 
		for (const Json::Value& line : cfg["sockets"])
			setSocket(std::make_shared<socket>(line.asString()));
	if (cfg.isMember("uniqName"))
		uniqName = cfg["uniqName"].asString();
	if (cfg.isMember("process")) 
		for (const Json::Value& line : cfg["process"])
			addMainProcess(std::make_shared<process>(line.asString()));
}

service::service(const service& p_src) {
	// copy constructor
	name	= p_src.name;
	uniqName= p_src.uniqName;
	type	= p_src.type;
	cfg	= p_src.cfg;
	server	= p_src.server;
	cfgFile = p_src.cfgFile;
	for(std::vector< std::shared_ptr<socket> >::const_iterator i=p_src.sockets.begin();i!=p_src.sockets.end();i++)
		sockets.push_back(*i);
	for(std::vector< std::shared_ptr<process> >::const_iterator i=p_src.mainProcess.begin();i!=p_src.mainProcess.end();i++)
		mainProcess.push_back(*i);
	for(std::vector< std::shared_ptr<Collector> >::const_iterator i=p_src.collectors.begin();i!=p_src.collectors.end();i++)
		collectors.push_back(*i);
	if (mainProcess.size() > 0)
		associate(server,"GET","^/service/"+name + "-" + uniqName+"/status$",doGetStatus);
}
void	service::setDefaultConfig() {
	Json::Value arr(Json::arrayValue);

	if (! cfg.isMember("sockets"))
		cfg["sockets"] = arr;
	else
		cfg["sockets"].clear();
	for(std::vector< std::shared_ptr<socket> >::iterator i=sockets.begin();i!=sockets.end();i++)
		cfg["sockets"].append((*i)->getSource());

	if (! cfg.isMember("process"))
		cfg["process"] = arr;
	else
		cfg["process"].clear();
	for(std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		cfg["process"].append((*i)->getPath());
	cfg["uniqName"] = uniqName;
}
void	service::saveConfigTemplate(std::string p_cfg_dir){
	setDefaultConfig();
	std::string fname = p_cfg_dir+std::string("/")+name+std::string(".json.template");
	std::ofstream cfgof (fname, std::ifstream::out);
	cfgof<<cfg;
	cfgof.close();
}

void	service::setSocket(std::shared_ptr<socket> p_sock) {
	if (sockets.size()==0 && uniqName=="") {
		uniqName = p_sock->getSource();
		uniqName.replace(uniqName.find(":"),3,"_");
		uniqName.replace(uniqName.find(":"),1,"_");
	}
	sockets.push_back(p_sock);
}

void 	service::addMainProcess(std::shared_ptr<process> p_p) { 
	if (mainProcess.size()==0)
		name = p_p->getName();
	mainProcess.push_back(p_p);
	//std::string tmp = name + "-" + uniqName;
	associate(server,"GET","^/service/"+name + "-" + uniqName+"/status$",doGetStatus);
	
	//TODO: detect sub processes (which have ppid=p_p)
}

bool	service::haveSocket(uint32_t p_socket_id) {
	for (std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->haveSocket(p_socket_id)) return true;
	return false;
}

bool	service::havePID(uint32_t p_pid) {
	for (std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->getPID() == p_pid) return true;
	return false;
}

void	service::doGetStatus(response_ptr response, request_ptr request) {
	//string name   = request->path_match[1];
	std::stringstream ss;
	Json::Value ret(Json::objectValue);
	
	int n=0;for(std::vector< std::shared_ptr<socket> >::iterator i=sockets.begin();i!=sockets.end();i++,n++) {
		ret["sockets"][n]["name"]   = (*i)->getSource();
		ret["sockets"][n]["status"] = "ok"; // TODO actually support this for correct service monitoring
	}
	n=0;for(std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++,n++) {
		ret["process"][n]["name"]   = (*i)->getName();
		ret["process"][n]["status"] = "ok";
		if (! (*i)->getStatus()) {
			std::string stts = "failed";
			if (haveHandler() && handler->isBlackout())
				stts = "ok (blackout)";
			ret["process"][n]["status"] = stts;
		}
		else
			ret["process"][n]["pid"]  = (*i)->getPID();
	}
		
	ss << ret;
	
	setResponseJson(response, ss.str());
}

void	service::getIndexHtml(std::stringstream& stream ) {
	stream << "<h3><a href='/service/" << name << "-" << uniqName <<"/status'>" << name << " - " << uniqName << "(" << type << ")</a></h3>\n";
}

void	service::getJson(Json::Value* p_defs) {
	std::string p = "/service/"+name+"/status";
	(*p_defs)["paths"][p]["get"]["responses"]["200"]["schema"]["$ref"] = "#/definitions/services";
	(*p_defs)["paths"][p]["get"]["responses"]["200"]["description"] = name+" status";
	(*p_defs)["paths"][p]["get"]["summary"] = name+" service status";
}

bool	service::haveSocket(std::string p_source) const {
	for(std::vector< std::shared_ptr<socket> >::const_iterator i=sockets.begin();i!=sockets.end();i++) {
		if ((*i)->getSource()  == p_source)
			return true;
	}
	return false;
}

bool	service::operator==(const service& rhs) {
	if (name != rhs.name) return false;
	if (type != rhs.type) return false;
	for(std::vector< std::shared_ptr<socket> >::iterator i=sockets.begin();i!=sockets.end();i++) {
		if (!rhs.haveSocket((*i)->getSource()))
			return false;
	}
	//TODO: service matching should be smarter than this (checking process name too)
	return true;
}

void	service::updateFrom(std::shared_ptr<service> src) {
	mainProcess.clear();
	for(std::vector< std::shared_ptr<process> >::iterator i=src->mainProcess.begin();i!=src->mainProcess.end();i++)
		addMainProcess((*i));
}

/*********************************
 * ServicesManager
 */
namespace watcheD {
std::map<std::string, detector_maker_t* > detectorFactory;
std::map<std::string, enhancer_maker_t* > enhancerFactory;
std::map<std::string, handler_maker_t*  >  handlerFactory;
std::map<std::string, service_maker_t*  >  serviceFactory;
}

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
			services.push_back(std::make_shared<service>(server, full_file_name));
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
		if ( *(*i) == *add ) {
			(*i)->updateFrom(add);
			//delete add;
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
	res["paths"]				= obj;

	systemCollectors->getJson(&res);
	res["definitions"]["serviceProcess"]["type"]				= "object";
	res["definitions"]["serviceProcess"]["properties"]["name"]["type"]	= "string";
	res["definitions"]["serviceProcess"]["properties"]["pid"]["type"]	= "number";
	res["definitions"]["serviceProcess"]["properties"]["status"]["type"]	= "string";
	res["definitions"]["serviceSocket"]["type"]				= "object";
	res["definitions"]["serviceSocket"]["properties"]["name"]["type"]	= "string";
	res["definitions"]["serviceSocket"]["properties"]["status"]["type"]	= "string";
	res["definitions"]["services"]["properties"]["process"]["type"]		= "array";
	res["definitions"]["services"]["properties"]["process"]["items"]["type"]= "#/definitions/serviceProcess";
	res["definitions"]["services"]["properties"]["sockets"]["type"]		= "array";
	res["definitions"]["services"]["properties"]["sockets"]["items"]["type"]= "#/definitions/serviceSocket";
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
	std::vector< std::shared_ptr<process> > processes;
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
			// service found
			std::shared_ptr<service> serv = std::make_shared<service>(server);
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
