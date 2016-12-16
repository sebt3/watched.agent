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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pwd.h>
namespace watcheD {

/*********************************
 * Sockets
 */

socket::socket(std::string p_line, std::string p_type) : type(p_type) {
	std::istringstream iss(p_line);
	std::istream_iterator<std::string> beg(iss), end;
	std::vector<std::string> tokens(beg,end);
	if (type == "tcp" || type == "udp") {
		setSockFrom(tokens[1], &(source));
		setSockFrom(tokens[2], &(dest));
	} else {
		setSockFrom6(tokens[1], &(source));
		setSockFrom6(tokens[2], &(dest));
	}
	id = atoi(tokens[9].c_str());
}

socket::socket(std::string p_source) {
	char dot;
	type = p_source.substr(0,p_source.find(":"));
	auto pos = p_source.find(":")+3;
	std::string port = p_source.substr(p_source.rfind(":")+1);
	source.port  = strtoul(port.c_str(), NULL, 10);
	std::string ip = p_source.substr(pos, p_source.rfind(":")-pos);
	if (type == "tcp" || type == "udp") {
		std::istringstream s(ip);
		s>>source.ip1>>dot>>source.ip2>>dot>>source.ip3>>dot>>source.ip4;
	} else {
		pos = ip.find(":");
		source.ip1 = strtoul(ip.substr(0,pos).c_str(), NULL, 16);
		source.ip2 = strtoul(ip.substr(pos+1,ip.find(":",pos+1)).c_str(), NULL, 16);pos = ip.find(":",pos+1);
		source.ip3 = strtoul(ip.substr(pos+1,ip.find(":",pos+1)).c_str(), NULL, 16);pos = ip.find(":",pos+1);
		source.ip4 = strtoul(ip.substr(pos+1,ip.find(":",pos+1)).c_str(), NULL, 16);pos = ip.find(":",pos+1);
		source.ip5 = strtoul(ip.substr(pos+1,ip.find(":",pos+1)).c_str(), NULL, 16);pos = ip.find(":",pos+1);
		source.ip6 = strtoul(ip.substr(pos+1,ip.find(":",pos+1)).c_str(), NULL, 16);pos = ip.find(":",pos+1);
		source.ip7 = strtoul(ip.substr(pos+1,ip.find(":",pos+1)).c_str(), NULL, 16);pos = ip.find(":",pos+1);
		source.ip8 = strtoul(ip.substr(pos+1).c_str(), NULL, 16);
	}
}
void socket::setSockFrom(std::string src, struct sock_addr *sa) {
	std::string port = src.substr(src.find(":")+1, src.length());
	sa->port = strtoul(port.c_str(), NULL, 16);
	sa->ip4  = strtoul(src.substr(0,2).c_str(), NULL, 16);
	sa->ip3  = strtoul(src.substr(2,2).c_str(), NULL, 16);
	sa->ip2  = strtoul(src.substr(4,2).c_str(), NULL, 16);
	sa->ip1  = strtoul(src.substr(6,2).c_str(), NULL, 16);
}
void socket::setSockFrom6(std::string src, struct sock_addr *sa) {
	std::string buf;
	std::string port = src.substr(src.find(":")+1, src.length());
	sa->port = strtoul(port.c_str(), NULL, 16);
	buf = src.substr( 2,2)+src.substr( 0,2);	sa->ip2  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr( 6,2)+src.substr( 4,2);	sa->ip1  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr(10,2)+src.substr( 8,2);	sa->ip4  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr(14,2)+src.substr(12,2);	sa->ip3  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr(18,2)+src.substr(16,2);	sa->ip6  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr(22,2)+src.substr(20,2);	sa->ip5  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr(26,2)+src.substr(24,2);	sa->ip8  = strtoul(buf.c_str(), NULL, 16);
	buf = src.substr(30,2)+src.substr(28,2);	sa->ip7  = strtoul(buf.c_str(), NULL, 16);
}

std::string socket::getSource() {
	std::string ret;
	if (type == "tcp" || type == "udp")
		ret = type+"://"+std::to_string(source.ip1)+"."+std::to_string(source.ip2)+"."+std::to_string(source.ip3)+"."+std::to_string(source.ip4)+":"+std::to_string(source.port);
	else {
		std::stringstream stream;
		stream << type+"://" << std::hex << source.ip1 << ":" << source.ip2 << ":" << source.ip3 << ":" << source.ip4 << ":" << source.ip5 << ":" << source.ip6 << ":" << source.ip7 << ":" << source.ip8  << ":"+std::to_string(source.port);

		ret = stream.str();
	}
	return ret;
}

/*********************************
 * Process
 */
process::process(uint32_t p_pid): pid(p_pid),username("") {
	char buf[1024];
	// get the full_path & base_name
	std::string file = "/proc/"+std::to_string(pid)+"/exe";
	int count = readlink(file.c_str(), buf, sizeof(buf));
	if(count>0) {
		buf[count] = '\0';
		full_path = buf;
		base_name = full_path.substr(full_path.rfind("/")+1);
	}
}

process::process(std::string p_fullpath):pid(0),username(""), full_path(p_fullpath) {
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

std::string	process::getUsername() {
	if (username != "" || !getStatus())
		return username;
	uint32_t uid = 0;
	bool found = false;
	std::string line;
	struct passwd *pw;
	std::ifstream	infile("/proc/"+std::to_string(pid)+"/status");
	while(infile.good() && getline(infile, line)) {
		std::istringstream iss(line);
		std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
		if (tokens[0] == "Uid:") {
			uid  = atoi(tokens[1].c_str());
			found=true;
			break;
		}
	}
	if (infile.good())
		infile.close();

	if (!found)
		return username;
	pw = getpwuid (uid);
	if (! pw) return std::to_string(uid);
 	username = pw->pw_name;
	return username;
}

std::string	process::getCWD() {
	if (cwd != "" || !getStatus())
		return cwd;
	char buf[1024];
	std::string file = "/proc/"+std::to_string(pid)+"/cwd";
	int count = readlink(file.c_str(), buf, sizeof(buf));
	if (count>0) {
		buf[count] = '\0';
		cwd = buf;
	}
	return cwd;
}

/*********************************
 * Services
 */
service::service(std::shared_ptr<HttpServer> p_server): name(""), host(""), type("unknown"), uniqName(""), cfg(Json::objectValue), handler(NULL), server(p_server) {
	setDefaultHost();
}

service::service(std::shared_ptr<HttpServer> p_server, std::string p_file_path): name(""), host(""), type("unknown"), uniqName(""), cfg(Json::objectValue), handler(NULL), cfgFile(p_file_path), server(p_server) {
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
	if (cfg.isMember("host"))
		host = cfg["host"].asString();
	if (cfg.isMember("process")) 
		for (const Json::Value& line : cfg["process"])
			addMainProcess(std::make_shared<process>(line.asString()));
	setDefaultHost();
}

service::service(const service& p_src) {
	// copy constructor
	name	= p_src.name;
	uniqName= p_src.uniqName;
	host	= p_src.host;
	type	= p_src.type;
	cfg	= p_src.cfg;
	cfgFile = p_src.cfgFile;
	server	= p_src.server;
	for(std::vector< std::shared_ptr<socket> >::const_iterator i=p_src.sockets.begin();i!=p_src.sockets.end();i++)
		sockets.push_back(*i);
	for(std::vector< std::shared_ptr<process> >::const_iterator i=p_src.mainProcess.begin();i!=p_src.mainProcess.end();i++)
		mainProcess.push_back(*i);
	for(std::map< std::string, std::shared_ptr<Collector> >::const_iterator i=p_src.collectors.begin();i!=p_src.collectors.end();i++)
		if (collectors.find(i->first) == collectors.end())
			collectors[i->first] = i->second;
	setDefaultHost();
}

void	service::addCollector(const std::string p_name) {
	if ( collectors.find(p_name) != collectors.end())
		return; // already added
	if ( serviceCollectorFactory.find(p_name) == serviceCollectorFactory.end())
		return; // no factory matching this name
	collectors[p_name] = serviceCollectorFactory[p_name].first(server, getCollectorCfg(p_name), shared_from_this(), serviceCollectorFactory[p_name].second);
	collectors[p_name]->startThread();
}

std::shared_ptr<Collector>	service::getCollector(std::string p_name) {
	if ( collectors.find(p_name) == collectors.end())
		return nullptr; // not existing
	return collectors[p_name];
}

void	service::getCollectorsHtml(std::stringstream& stream) {
	for(std::map< std::string, std::shared_ptr<Collector> >::iterator i=collectors.begin();i!=collectors.end();i++)
		i->second->getIndexHtml(stream);
}


std::shared_ptr< std::vector<uint32_t> >	service::getPIDs() {
	std::shared_ptr< std::vector<uint32_t> > ret = std::make_shared<std::vector<uint32_t>>();

	for (std::vector< std::shared_ptr<process> >::iterator i = mainProcess.begin();i!=mainProcess.end();i++)
		ret->push_back((*i)->getPID());

	return ret;
}

void	service::setDefaultHost() {
	if (host != "") return;
	struct addrinfo hints, *info;//, *p;
	int gai_result;
	char ghost[1024];
	memset(ghost,0,1024);
	gethostname(ghost,1023);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	if ((gai_result = getaddrinfo(ghost, "http", &hints, &info)) != 0) {
		std::cerr << "getaddrinfo: " << gai_strerror(gai_result) << std::endl;
		return;
	}
	/*for(p = info; p != NULL; p = p->ai_next) {
		printf("hostname: %s\n", p->ai_canonname);
	}*/
	host = info->ai_canonname;
	freeaddrinfo(info);
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
	cfg["host"] = host;
}

Json::Value* 	service::getCollectorCfg(std::string p_name) {
	Json::Value obj_value(Json::objectValue);
	if(! cfg.isMember("collectors")) {
		cfg["collectors"][p_name] = obj_value;
		cfg["collectors"].setComment(std::string("/*\tConfigure the collectors plugins */"), Json::commentBefore);
	}else if(! cfg["collectors"].isMember(p_name) ) {
		cfg["collectors"][p_name] = obj_value;
	}
	return &(cfg["collectors"][p_name]); 
}

void	service::saveConfigTemplate(std::string p_cfg_dir){
	setDefaultConfig();
	std::string fname = p_cfg_dir+std::string("/")+name+"-"+uniqName+std::string(".json.template");
	std::ofstream cfgof (fname, std::ifstream::out);
	if (!cfgof) {
		std::cerr << "Warning: Cannot write on " << fname << ". Service configuration template NOT updated\n";
		return;
	}
	cfgof<<cfg;
	cfgof.close();
}

void	service::setSocket(std::shared_ptr<socket> p_sock) {
	if (sockets.size()==0 && uniqName=="") {
		uniqName = p_sock->getSource();
		uniqName.replace(uniqName.find(":"),3,"_");
		uniqName.replace(uniqName.rfind(":"),1,"_");
	}
	for(std::vector< std::shared_ptr<socket> >::iterator i=sockets.begin();i!=sockets.end();i++)
		if ((*i)->getSource()  == p_sock->getSource())
			return;
	sockets.push_back(p_sock);
}

void 	service::addMainProcess(std::shared_ptr<process> p_p) { 
	if (mainProcess.size()==0 && name == "")
		name = p_p->getName();
	mainProcess.push_back(p_p);
	
	//TODO: detect sub processes (which have ppid=p_p)
}

bool	service::haveSocket(uint32_t p_socket_id) {
	for (std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->getStatus() && (*i)->haveSocket(p_socket_id)) return true;
	return false;
}
/*
bool	service::haveProcessSocket(std::string p_socket_source) {
	for (std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->getSource() == p_socket_source) return true;
	return false;
}*/

bool	service::havePID(uint32_t p_pid) {
	for (std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++)
		if ((*i)->getPID() == p_pid) return true;
	return false;
}

void	service::getJsonStatus(Json::Value* ref) {
	int n=0;for(std::vector< std::shared_ptr<socket> >::iterator i=sockets.begin();i!=sockets.end();i++,n++) {
		(*ref)["sockets"][n]["name"]   = (*i)->getSource();
		std::string stts = "ok";
		if (! haveSocket((*i)->getID())) {
			stts = "failed";
			if (haveHandler() && handler->isBlackout())
				stts = "ok (blackout)";
		}
		(*ref)["sockets"][n]["status"] = stts;
	}
	n=0;for(std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++,n++) {
		(*ref)["process"][n]["name"]   = (*i)->getName();
		(*ref)["process"][n]["full_path"]   = (*i)->getPath();
		(*ref)["process"][n]["status"] = "ok";
		if (! (*i)->getStatus()) {
			std::string stts = "failed";
			if (haveHandler() && handler->isBlackout())
				stts = "ok (blackout)";
			(*ref)["process"][n]["status"] = stts;
		} else {
			(*ref)["process"][n]["pid"]  = (*i)->getPID();
			(*ref)["process"][n]["cwd"]   = (*i)->getCWD();
			(*ref)["process"][n]["username"]   = (*i)->getUsername();
		}
	}
	(*ref)["host"] = host;
	(*ref)["properties"]["host"] = host;
	(*ref)["properties"]["name"] = name;
	(*ref)["properties"]["type"] = type;
	(*ref)["properties"]["subType"] = subType;
	(*ref)["properties"]["uniqName"] = uniqName;
}

void	service::getIndexHtml(std::stringstream& stream ) {
	int ok=0;
	int failed=0;
	for(std::vector< std::shared_ptr<socket> >::iterator i=sockets.begin();i!=sockets.end();i++) {
		if ((! haveSocket((*i)->getID())) && !(haveHandler() && handler->isBlackout()))
			failed++;
		else	ok++;
	}
	for(std::vector< std::shared_ptr<process> >::iterator i=mainProcess.begin();i!=mainProcess.end();i++) {
		if ( (! (*i)->getStatus()) && !(haveHandler() && handler->isBlackout()))
			failed++;
		else	ok++;
	}
	double pct_ok = (100.0*ok)/(ok+failed);
	double pct_fail = (100.0*failed)/(ok+failed);
	
	stream << "<a href=\"/service/"+getID()+"/html\"><div class=\"clearfix\"><span class=\"pull-left\">"+name+" - "+uniqName+"</span></div><div class=\"progress xs\"><div class=\"progress-bar progress-bar-green\" style=\"width: "+std::to_string(pct_ok)+"%;\"></div><div class=\"progress-bar progress-bar-red\" style=\"width: "+std::to_string(pct_fail)+"%;\"></div></div></a>\n";
}

void	service::getJson(Json::Value* p_defs) {
	std::string p = "/service/"+getID()+"/status";
	(*p_defs)["paths"][p]["get"]["responses"]["200"]["schema"]["$ref"] = "#/definitions/services";
	(*p_defs)["paths"][p]["get"]["responses"]["200"]["description"] = name+" status";
	(*p_defs)["paths"][p]["get"]["summary"] = name+" service status";
	for(std::map< std::string, std::shared_ptr<Collector> >::iterator i=collectors.begin();i!=collectors.end();i++) {
		i->second->getDefinitions(&((*p_defs)["definitions"]));
		i->second->getPaths(&((*p_defs)["paths"]));
	}
}

bool	service::haveSocket(std::string p_source) const {
	for(std::vector< std::shared_ptr<socket> >::const_iterator i=sockets.begin();i!=sockets.end();i++) {
		if ((*i)->getSource()  == p_source)
			return true;
	}
	return false;
}
bool	service::needSocketFrom(std::shared_ptr<service> p_service) {
	for(std::vector< std::shared_ptr<socket> >::iterator i = sockets.begin();i!=sockets.end();i++) {
		if (haveSocket((*i)->getID()))
			continue; // skip working ssocket
		for(std::vector< std::shared_ptr<socket> >::iterator j = p_service->sockets.begin();j!=p_service->sockets.end();j++) {
			if ( (*j)->getSource() == (*i)->getSource() )
				return true;
		}
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
	//TODO: service matching should be smarter than this (checking process name, username too)
	return true;
}

bool	service::operator==(const std::string rhs) {
	return (rhs == name+"-"+uniqName );
}

void	service::updateFrom(std::shared_ptr<service> src) {
	// Copy the process
	for(std::vector< std::shared_ptr<process> >::iterator i=src->mainProcess.begin();i!=src->mainProcess.end();i++) {
		for(std::vector< std::shared_ptr<process> >::iterator j=mainProcess.begin();j!=mainProcess.end();j++) {
			if (! (*j)->getStatus() && (*j)->getPath() == (*i)->getPath()) {
				j = mainProcess.erase(j);
				break;
			}
		}

		addMainProcess((*i));
	}
	// Copy the sockets
	for(std::vector< std::shared_ptr<socket> >::iterator i=src->sockets.begin();i!=src->sockets.end();i++) {
		for(std::vector< std::shared_ptr<socket> >::iterator j=sockets.begin();j!=sockets.end();j++) {
			if ((*j)->getSource() == (*i)->getSource()) {
				j = sockets.erase(j);
				break;
			}
		}

		setSocket((*i));
	}

	// TODO: copy the collectors, updating the service too

	// TODO: copy the configuration, not just the collectors conf
	for (Json::Value::iterator j = src->cfg["collectors"].begin();j!=src->cfg["collectors"].end();j++) {
		cfg["collectors"][j.key().asString()] = *j;
	}

}

}
