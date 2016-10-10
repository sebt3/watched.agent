#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

struct sock_addr {
	uint32_t	port;
	uint16_t	ip1;
	uint16_t	ip2;
	uint16_t	ip3;
	uint16_t	ip4;
};

struct socket {
	uint32_t	id;
	struct sock_addr source;
	struct sock_addr dest;
};

void set_sock_from(std::string src, struct sock_addr *sa) {
	std::string port = src.substr(src.find(":")+1, src.length());
	sa->port = strtoul(port.c_str(), NULL, 16);
	sa->ip4  = strtoul(src.substr(0,2).c_str(), NULL, 16);
	sa->ip3  = strtoul(src.substr(2,2).c_str(), NULL, 16);
	sa->ip2  = strtoul(src.substr(4,2).c_str(), NULL, 16);
	sa->ip1  = strtoul(src.substr(6,2).c_str(), NULL, 16);
}

int detect(void) {

	// find all LISTEN sockets
	std::ifstream infile("/proc/net/tcp");
	std::string line;
	std::vector<struct socket*> sockets;
	while(infile.good() && getline(infile, line)) {
		std::istringstream iss(line);
		std::istream_iterator<std::string> beg(iss), end;
		std::vector<std::string> tokens(beg,end);
		if (tokens[3] != "0A")
			continue;
		struct socket *s = new struct socket;
		set_sock_from(tokens[1], &(s->source));
		set_sock_from(tokens[2], &(s->dest));
		s->id = atoi(tokens[9].c_str());
		sockets.push_back(s);
	}
	if (infile.good())
		infile.close();

	// find all PID
	DIR *dir;
	struct dirent *ent;
	std::vector<uint32_t> pids;
	std::string file;
	if ((dir = opendir ("/proc/")) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			file = ent->d_name;
			if (std::all_of(file.begin(), file.end(), ::isdigit)) {
				pids.push_back(atoi(file.c_str()));
			}
		}
		closedir (dir);
	}
	// find PID matching sockets
	std::stringstream path;
	std::stringstream s;
	std::string link;
	const std::string soc("socket:[");
	for(std::vector<uint32_t>::iterator p = pids.begin(); p != pids.end(); ++p) {
		path.str("");path << "/proc/" << (*p) << "/fd/";
		if ((dir = opendir (path.str().c_str())) == NULL) continue;
		while ((ent = readdir (dir)) != NULL) {
			// looping throw all /proc/[pid]/fd/[fd]
			if (ent->d_name[0] == '.') continue;
			file = path.str() + ent->d_name;
			char buf[512];
			int count = readlink(file.c_str(), buf, sizeof(buf));
			if (count<=0) continue;
			buf[count] = '\0';
			link = buf;
			if (link.compare(0, soc.length(), soc) !=0) continue; // quit if not a socket
			// find the matching socket
			for(std::vector<struct socket*>::iterator it = sockets.begin(); it != sockets.end(); ++it) {
				s.str("");s << soc << (*it)->id << ']';
				if (link == s.str())
					std::cout << "Match found for " << (*p) << " and " << (*it)->id << "(" << (*it)->source.ip1 << "." << (*it)->source.ip2 << "." << (*it)->source.ip3 << "." << (*it)->source.ip4 << ":" << (*it)->source.port << ")" << std::endl;
			}
		}
		closedir (dir);
	}

	return 0;
}
