#pragma once
#include "agent.h"
#include <string>
#include <vector>


namespace watcheD {
/*********************************
 * Sockets
 */

struct sock_addr {
	uint32_t	port;
	uint16_t	ip1;
	uint16_t	ip2;
	uint16_t	ip3;
	uint16_t	ip4;
};

//class service;
class socket {
//friend class service;
public:
	socket(std::string p_line, std::string p_type);
	uint32_t getID() { return id; }
	std::string getSource();
protected:
	uint32_t	 id;
	std::string	 type;
	struct sock_addr source;
	struct sock_addr dest;
private:
	void setSockFrom(std::string src, struct sock_addr *sa);
};

/*********************************
 * Process
 */
class process {
public:
	process(uint32_t p_pid);
	bool haveSocket(uint32_t p_socket_id);
	uint32_t getPID() { return pid; }
	std::string getPath() { return full_path; }
	std::string getName() { return base_name; }
private:
	uint32_t		pid;
	std::string		full_path;
	std::string		base_name;
	std::vector<uint32_t>	sockets;
};
/*TODO: 
 * alimenter les controlGroup avec /proc/[PID]/cgroup
 * faire un collecteur basé sur /proc/[PID]/status, /proc/[PID]/stat, /proc/[PID]/statm, /proc/[PID]/sched +/proc/<pid>/schedstat+
 */

/*********************************
 * Services
 */
class service {
public:
	service() {}
	void	setSocket(socket *p_sock);
	//socket *getSocket() {return sock; }
	void 	addMainProcess(process *p_p);
	bool	haveSocket(uint32_t p_socket_id);
	bool	havePID(uint32_t p_pid);
	void	getIndexHtml(std::stringstream& stream );
protected:
	std::string name;
private:
	std::vector<socket *> sockets;
	std::vector<process *> mainProcess;
	//std::vector<process *> clientProcess;
};

/*********************************
 * ServicesManager
 */
//TODO: add support for plugin detectors
//TODO: add support for enhancers (being plugin or not)
//TODO: plugins should be able to be written in lua
class serviceDetector;
class servicesManager {
public:
	servicesManager(HttpServer *p_server, Config* p_cfg);
	void	find();
	void	addService(service *p_serv);
	bool	haveSocket(uint32_t p_socket_id);
	bool	havePID(uint32_t p_pid);
	void	doGetJson(response_ptr response, request_ptr request);
	void	doGetRootPage(response_ptr response, request_ptr request);
	void	startThreads();
private:
	std::vector<service *>		services;
	std::vector<serviceDetector *>	detectors;
	CollectorsManager*		systemCollectors;
	HttpServer* server;
	Config *cfg;
};

/*********************************
 * Detectors
 */

class serviceDetector {
public:
	serviceDetector(servicesManager *p_sm):services(p_sm) {}
	virtual void find() =0;
	
protected:
	servicesManager *services;
};

class socketDetector: public serviceDetector {
public:
	socketDetector(servicesManager *p_sm):serviceDetector(p_sm) {}
	void find();
};

}
