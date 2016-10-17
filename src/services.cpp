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


/*********************************
 * Services
 */
service::service(const service& p_src) {
	// copy constructor
	name	= p_src.name;
	type	= p_src.type;
	cfg	= p_src.cfg;
	server	= p_src.server;
	for(std::vector<socket *>::const_iterator i=p_src.sockets.begin();i!=p_src.sockets.end();i++)
		sockets.push_back(*i);
	for(std::vector<process *>::const_iterator i=p_src.mainProcess.begin();i!=p_src.mainProcess.end();i++)
		mainProcess.push_back(*i);
	
}

void	service::setSocket(socket *p_sock) {
	sockets.push_back(p_sock);
}

void 	service::addMainProcess(process *p_p) { 
	if (mainProcess.size()==0)
		name = p_p->getName();
	mainProcess.push_back(p_p);
	
	//TODO: detect sub processes (which have ppid=p_p)
}

bool	service::haveSocket(uint32_t p_socket_id) {
	for (std::vector<process *>::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->haveSocket(p_socket_id)) return true;
	return false;
}

bool	service::havePID(uint32_t p_pid) {
	for (std::vector<process *>::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->getPID() == p_pid) return true;
	return false;
}

void	service::getIndexHtml(std::stringstream& stream ) {
		stream << "<h3>" << name << "(" << type << ")</h3><ul>\n";
		for(std::vector<socket *>::iterator i=sockets.begin();i!=sockets.end();i++)
			stream << "<li>" << (*i)->getSource() << "</li>\n";
		for(std::vector<process *>::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
			stream << "<li>" << (*i)->getPath() << "</li>\n";
		stream << "</ul>\n";
}

/*********************************
 * ServicesManager
 */
namespace watcheD {
std::map<std::string, detector_maker_t *> detectorFactory;
std::map<std::string,  service_maker_t *>  serviceFactory;
std::map<std::string, enhancer_maker_t *> enhancerFactory;
}

servicesManager::servicesManager(HttpServer *p_server, Config* p_cfg) : server(p_server), cfg(p_cfg) {
	Json::Value*		servCfg	  = cfg->getServer();
	DIR *			dir;
	const std::string	directory = (*servCfg)["services_cpp"].asString();
	class dirent*		ent;
	class stat 		st;
	void*			dlib;

	// Load the services plugins
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
	for(std::map<std::string, detector_maker_t *>::iterator factit = detectorFactory.begin();factit != detectorFactory.end(); factit++)
		detectors.push_back(factit->second(this, server));

	// then the enhancer
	for(std::map<std::string, enhancer_maker_t *>::iterator factit = enhancerFactory.begin();factit != enhancerFactory.end(); factit++)
		enhancers.push_back(factit->second(this));

	detectors.push_back(new socketDetector(this, server));
	systemCollectors	= new CollectorsManager(server,cfg);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	associate(server,"GET","^/$",doGetRootPage);
	associate(server,"GET","^/api/swagger.json$",doGetJson);
}
void servicesManager::startThreads() {
	systemCollectors->startThreads();
}

void servicesManager::addService(service *p_serv) {
	// TODO: detect if the service isnt already known
	service *add = p_serv;
	// try to improve the service
	for(std::vector<serviceEnhancer *>::iterator i=enhancers.begin();i!=enhancers.end();i++) {
		service *s = (*i)->enhance(p_serv);
		if (s!=NULL) {
			add=s;
			break;
		}
	}
	services.push_back(add);
}

void servicesManager::find() {
	for (std::vector<serviceDetector *>::iterator i=detectors.begin();i!=detectors.end();i++)
		(*i)->find();
}

bool	servicesManager::haveSocket(uint32_t p_socket_id) {
	for (std::vector<service *>::iterator i=services.begin();i!=services.end();i++)
		if ((*i)->haveSocket(p_socket_id)) return true;
	return false;
}

service *servicesManager::enhanceFromFactory(std::string p_id, service *p_serv) {
	for(std::map<std::string, service_maker_t *>::iterator i=serviceFactory.begin();i!=serviceFactory.end();i++) {
		if (i->first == p_id)
			return i->second(*p_serv);
	}
	return NULL;
}

bool	servicesManager::havePID(uint32_t p_pid) {
	for (std::vector<service *>::iterator i=services.begin();i!=services.end();i++)
		if ((*i)->havePID(p_pid)) return true;
	return false;
}

void servicesManager::doGetRootPage(response_ptr response, request_ptr request) {
	std::stringstream stream;
        stream << "<html><head><title>" << APPS_NAME.c_str() << "</title>\n";
	stream << "</head><body><h1>Services</h1>\n";
	for (std::vector<service *>::iterator i=services.begin();i!=services.end();i++)
		(*i)->getIndexHtml(stream);

	stream << "<h1>System</h1>\n";
	systemCollectors->getIndexHtml(stream);
	stream << "</body></html>\n";
        setResponseHtml(response, stream.str());
}

void servicesManager::doGetJson(response_ptr response, request_ptr request) {
	Json::Value res(Json::objectValue);
	Json::StreamWriterBuilder wbuilder;
	Json::Value obj(Json::objectValue);
	std::string document;
	
	res["swagger"]			= "2.0";
	res["info"]["version"]		= "1.0.0";
	res["info"]["title"]		= APPS_NAME;
	res["info"]["description"]	= APPS_DESC;
	//res["info"]["termsOfService"]	= "http://some.url/terms/";
	res["info"]["contact"]["name"]	= "Sebastien Huss";
	res["info"]["contact"]["email"]	= "sebastien.huss@gmail.com";
	//res["info"]["contact"]["url"]	= "http://sebastien.huss.free.fr/";
	res["info"]["license"]["name"]	= "AGPL";
	res["info"]["license"]["url"]	= "https://www.gnu.org/licenses/agpl-3.0.html";
	// add swaggerised docs
	//res["externalDocs"]["description"]	= "API documentation";
	//res["externalDocs"]["url"]		= "http://some.url/docs";
	
	res["host"]			= "localhost";
	res["basePath"]			= "/";
	res["schemes"][0]		= "http";
	res["consumes"][0]		= "application/json";
	res["produces"][0]		= "application/json";
	res["definitions"]		= obj;
	res["paths"]			= obj;

	systemCollectors->getJson(&res);

	wbuilder["indentation"] = "\t";
	document = Json::writeString(wbuilder, res);

	setResponseJson(response, document);
}



/*********************************
 * Detectors  
 */
void socketDetector::find(void) {
	//TODO: add suport for ipv6 sockets

	// find all LISTEN TCP sockets
	std::ifstream infile("/proc/net/tcp");
	std::string line;
	std::vector<socket*> sockets;
	while(infile.good() && getline(infile, line)) {
		std::istringstream iss(line);
		std::istream_iterator<std::string> beg(iss), end;
		std::vector<std::string> tokens(beg,end);
		if (tokens[3] != "0A")
			continue; // keep only listening sockets
		if (services->haveSocket(atoi(tokens[9].c_str())))
			continue; // socket already associated to a service
		sockets.push_back(new socket(line, "tcp"));
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
		sockets.push_back(new socket(line, "udp"));
	}
	if (unfile.good())
		unfile.close();

	if (sockets.size()<1) 
		return;
	// find all processes
	DIR *dir;
	struct dirent *ent;
	std::vector<process *> processes;
	std::string file;
	process *p;
	bool found;
	if ((dir = opendir ("/proc/")) == NULL) return;
	while ((ent = readdir (dir)) != NULL) {
		file = ent->d_name;
		if (!std::all_of(file.begin(), file.end(), ::isdigit)) continue;
		if (services->havePID(atoi(file.c_str())))
			continue; // process already associated to a service
		p = new process(atoi(file.c_str()));
		p->setSockets();
		found = false;
		for(std::vector<socket*>::iterator it = sockets.begin(); it != sockets.end(); ++it) {
			if (!p->haveSocket((*it)->getID())) continue;
			// service found
			service *serv = new service(server);
			serv->addMainProcess(p);
			serv->setSocket(*it);
			// remove the now associated socket from the list
			sockets.erase(it);
			// trying to associate more sockets to this service
			for(std::vector<socket*>::iterator i = sockets.begin(); i != sockets.end(); ) {
				if (!serv->haveSocket((*i)->getID())) {++i;continue; }
				serv->setSocket(*i);
				i = sockets.erase(i);
			}
			services->addService(serv);
			found = true;
			break;
		}
		// free unused process
		if(!found) delete p;
	}
	closedir (dir);
	// TODO: do something with the non-matching sockets too

	// cleaning non-matching sockets
	for(std::vector<socket*>::iterator i=sockets.begin();i!=sockets.end();i++)
		delete *i;
}
