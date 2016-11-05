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
namespace watcheD {

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

}
