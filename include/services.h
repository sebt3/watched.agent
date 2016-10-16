#pragma once
#include "collectors.h"
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
	service(HttpServer* p_server): name(""), type(""), cfg(NULL), server(p_server) {}
	void	setSocket(socket *p_sock);
	//socket *getSocket() {return sock; }
	void 	addMainProcess(process *p_p);
	bool	haveSocket(uint32_t p_socket_id);
	bool	havePID(uint32_t p_pid);
	void	getIndexHtml(std::stringstream& stream );
protected:
	std::string		name;
	std::string		type;
	Json::Value*		cfg;
private:
	HttpServer*		server;
	std::vector<socket *>	sockets;
	std::vector<process *>	mainProcess;
	//std::vector<process *> subProcess;
};


/*********************************
 * Detectors
 */
class servicesManager;
class serviceDetector {
public:
	serviceDetector(servicesManager *p_sm, HttpServer* p_server):server(p_server), services(p_sm) {}
	virtual void find() =0;
	
protected:
	HttpServer*		server;
	servicesManager*	services;
};

/*********************************
 * Plugin management
 */
typedef service *service_maker_t(HttpServer* p_srv);
extern std::map<std::string, service_maker_t *> serviceFactory;
typedef serviceDetector *detector_maker_t(servicesManager *p_sm, HttpServer* p_server);
extern std::map<std::string, detector_maker_t *> detectorFactory;

}

#define MAKE_PLUGIN_SERVICE(className,id)		\
extern "C" {						\
service *makeService(HttpServer* p_srv) {		\
   return new className(p_srv);				\
}							\
}							\
class proxyService { public:				\
   proxyService(){ serviceFactory[id] = makeService; }	\
};							\
proxyService p;

#define MAKE_PLUGIN_DETECTOR(className,id)				\
extern "C" {								\
serviceDetector *makeDetector(servicesManager *p_sm, HttpServer* p_server) {	\
   return new className(p_sm, p_server);				\
}									\
}									\
class proxyDetector { public:						\
   proxyDetector(){ detectorFactory[id] = makeDetector; }		\
};									\
proxyDetector p;
