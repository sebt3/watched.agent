#pragma once
#include "collectors.h"
#include <string>
#include <vector>

namespace watcheD {

class servicesManager;

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

class socket {
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
	void setSockets();
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
 */

/*********************************
 * Services
 */
class service {
public:
	service(HttpServer* p_server): name(""), type("unknown"), cfg(NULL), server(p_server) {}
	service(const service& p_src);
	void		setSocket(socket *p_sock);
	void 		addMainProcess(process *p_p);
	bool		haveSocket(uint32_t p_socket_id);
	bool		havePID(uint32_t p_pid);
	void		getIndexHtml(std::stringstream& stream );
	std::string	getType() { return type; }
	void		setType(std::string p_type) { type=p_type; }
	std::string	getName() { return name; }
	void		setName(std::string p_name) { name=p_name; }
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

/*TODO: 
 * faire un collecteur basé sur /proc/[PID]/status ou /proc/[PID]/stat + /proc/[PID]/statm
 * faire un collecteur basé sur /proc/[PID]/sched ou +/proc/<pid>/schedstat+
 */

/*********************************
 * Enhancer
 */

class serviceEnhancer {
public:
	serviceEnhancer(servicesManager *p_sm): services(p_sm) {}
	virtual service *enhance(service *p_serv) =0;
protected:
	servicesManager*	services;
};

/*********************************
 * Detectors
 */
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

typedef service *service_maker_t(const service& p_src);
extern std::map<std::string, service_maker_t *> serviceFactory;
typedef serviceDetector *detector_maker_t(servicesManager *p_sm, HttpServer* p_server);
extern std::map<std::string, detector_maker_t *> detectorFactory;
typedef serviceEnhancer *enhancer_maker_t(servicesManager *p_sm);
extern std::map<std::string, enhancer_maker_t *> enhancerFactory;

}

#define MAKE_PLUGIN_SERVICE(className,id)			\
extern "C" {							\
service *makeService_##id(const service& p_src) {			\
   return new className(p_src);					\
}								\
}								\
class proxyService_##id { public:					\
   proxyService_##id(){ serviceFactory[#id] = makeService_##id; }	\
};								\
proxyService_##id ps_##id;


#define MAKE_PLUGIN_ENHANCER(className,id)				\
extern "C" {								\
serviceEnhancer *makeEnhancer_##id(servicesManager *p_sm) {		\
   return new className(p_sm);						\
}									\
}									\
class proxyEnhancer_##id { public:					\
   proxyEnhancer_##id(){ enhancerFactory[#id] = makeEnhancer_##id; }	\
};									\
proxyEnhancer_##id pe_##id;


#define MAKE_PLUGIN_DETECTOR(className,id)					\
extern "C" {									\
serviceDetector *makeDetector_##id(servicesManager *p_sm, HttpServer* p_server) {	\
   return new className(p_sm, p_server);					\
}										\
}										\
class proxyDetector_##id { public:						\
   proxyDetector_##id(){ detectorFactory[#id] = makeDetector_##id; }		\
};										\
proxyDetector_##id pd_##id;
