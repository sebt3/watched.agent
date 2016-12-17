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
	uint16_t	ip5;
	uint16_t	ip6;
	uint16_t	ip7;
	uint16_t	ip8;
};

class socket {
public:
	socket(std::string p_line, std::string p_type);
	socket(std::string p_source);
	uint32_t 	getID() { return id; }
	std::string 	getSource();
protected:
	uint32_t	 id;
	std::string	 type;
	struct sock_addr source;
	struct sock_addr dest;
private:
	void setSockFrom(std::string src, struct sock_addr *sa);
	void setSockFrom6(std::string src, struct sock_addr *sa);
};

/*********************************
 * Process
 */

class process {
	friend class service;
public:
	process(uint32_t p_pid);
	process(std::string p_fullpath);
	void 		setSockets();
	bool 		haveSocket(uint32_t p_socket_id);
	bool 		getStatus();
	uint32_t	getPID() { return pid; }
	std::string	getPath() { return full_path; }
	std::string	getName() { return base_name; }
	std::string	getCWD();
	std::string	getUsername();
private:
	uint32_t		pid;
	std::string		username;
	std::string		full_path;
	std::string		base_name;
	std::string		cwd;
	std::vector<uint32_t>	sockets;
};
/*TODO: 
 * alimenter les controlGroup avec /proc/[PID]/cgroup
 */

/*********************************
 * Services
 */
class serviceHandler;
class service: public std::enable_shared_from_this<service> {
public:
	service(std::shared_ptr<HttpServer> p_server);
	service(std::shared_ptr<HttpServer> p_server, std::string p_file_path);
	service(const service& p_src);
	void		updateFrom(std::shared_ptr<service> src);

	bool		operator==(const std::string rhs);
	bool		operator==(const service& rhs);
	inline bool	operator!=(const service& rhs){return !operator==(rhs);}
	bool		haveSocket(uint32_t p_socket_id);
	bool		haveSocket(std::string p_source) const;
	bool		havePID(uint32_t p_pid);
	bool		haveHandler() { return handler != NULL; }
	bool		needSocketFrom(std::shared_ptr<service> p_service);
	std::string	getType() { return type; }
	std::string	getSubType() { return subType; }
	std::string	getName() { return name; }
	std::string	getID() { return name+"-"+uniqName; }

	void		setSocket(std::shared_ptr<socket> p_sock);
	void 		addMainProcess(std::shared_ptr<process> p_p);
	void		setHandler(std::shared_ptr<serviceHandler> p_h) { handler = p_h; }
	void		setType(std::string p_type) { type=p_type; }
	void		setSubType(std::string p_type) { subType=p_type; }
	void		setHost(std::string p_host) { host=p_host; }
	void		setName(std::string p_name) { name=p_name; }
	void		setUniqKey(std::string p_key) { uniqName=p_key; }

	void		saveConfigTemplate(std::string p_cfg_dir);
	void		getIndexHtml(std::stringstream& stream );
	void		getJsonStatus(Json::Value* ref);
	void		getJson(Json::Value* p_defs);
	void		addCollector(const std::string p_name);
	std::shared_ptr<Collector>	getCollector(std::string p_name);
	Json::Value* 	getCollectorCfg(std::string p_name);
	void		getCollectorsHtml(std::stringstream& stream );
	std::shared_ptr< std::vector<uint32_t> >	getPIDs();
protected:
	void		setDefaultConfig();
	void		setDefaultHost();
	std::string					name;
	std::string					host;
	std::string					type;
	std::string					subType;
	std::string					uniqName;
	Json::Value					cfg;
	std::shared_ptr<serviceHandler>			handler;
private:
	std::string					cfgFile;
	std::vector< std::shared_ptr<socket> >		sockets;
	std::vector< std::shared_ptr<process> >		mainProcess;
	std::map< std::string, std::shared_ptr<Collector> >	collectors;
	//std::vector< std::shared_ptr<process> >	subProcess;
	std::shared_ptr<HttpServer>			server;
};



/*********************************
 * ServiceHandler
 */
class serviceHandler {
public:
	serviceHandler(std::shared_ptr<service> p_s): serv(p_s) {}
	virtual bool status() =0;
	virtual bool isBlackout() =0;
protected:
	std::weak_ptr<service>		serv;
private:
	
};

/*********************************
 * Enhancer
 */

class serviceEnhancer {
public:
	serviceEnhancer(std::shared_ptr<servicesManager> p_sm): services(p_sm) {}
	virtual std::shared_ptr<service> enhance(std::shared_ptr<service> p_serv) =0;
protected:
	std::weak_ptr<servicesManager>	services;
};

/*********************************
 * Detectors
 */
class serviceDetector {
public:
	serviceDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server):server(p_server), services(p_sm) {}
	virtual void find() =0;
	
protected:
	std::shared_ptr<HttpServer>	server;
	std::weak_ptr<servicesManager>	services;
};

/*********************************
 * Plugin management
 */

typedef std::shared_ptr<service>         service_maker_t(const service& p_src);
typedef std::shared_ptr<serviceHandler>  handler_maker_t(std::shared_ptr<service> p_s);
typedef std::shared_ptr<serviceDetector> detector_maker_t(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server);
typedef std::shared_ptr<serviceEnhancer> enhancer_maker_t(std::shared_ptr<servicesManager> p_sm);
typedef std::shared_ptr<Collector> 	 service_collector_maker_t(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_s, const std::string p_filename);
extern std::map<std::string, service_maker_t* >  serviceFactory;
extern std::map<std::string, handler_maker_t* >  handlerFactory;
extern std::map<std::string, detector_maker_t* > detectorFactory;
extern std::map<std::string, enhancer_maker_t* > enhancerFactory;
extern std::map<std::string, std::pair<service_collector_maker_t*,std::string> > serviceCollectorFactory;
}

#define MAKE_PLUGIN_SERVICE_COLLECTOR(className,id)				\
extern "C" {									\
std::shared_ptr<Collector> makerSCol_##id(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_s){	\
   return std::make_shared<className>(p_srv, p_cfg, p_s);			\
}										\
}										\
class proxySCol_##id { public:							\
   proxySCol_##id(){ serviceCollectorFactory[#id] = std::make_pair(makerSCol_##id,""); }		\
};										\
proxySCol_##id p;

#define MAKE_PLUGIN_SERVICE(className,id)					\
extern "C" {									\
std::shared_ptr<service> makeService_##id(const service& p_src) {		\
   return std::make_shared<className>(p_src);					\
}										\
}										\
class proxyService_##id { public:						\
   proxyService_##id(){ serviceFactory[#id] = makeService_##id; }		\
};										\
proxyService_##id ps_##id;


#define MAKE_PLUGIN_HANDLER(className,id)					\
extern "C" {									\
std::shared_ptr<serviceHandler> makeHandler_##id(std::shared_ptr<service> p_s) {\
   return std::make_shared<className>(p_s);					\
}										\
}										\
class proxyHandler_##id { public:						\
   proxyHandler_##id(){ handlerFactory[#id] = makeHandler_##id; }		\
};										\
proxyHandler_##id ph_##id;


#define MAKE_PLUGIN_ENHANCER(className,id)					\
extern "C" {									\
std::shared_ptr<serviceEnhancer> makeEnhancer_##id(std::shared_ptr<servicesManager> p_sm) {	\
   return std::make_shared<className>(p_sm);					\
}										\
}										\
class proxyEnhancer_##id { public:						\
   proxyEnhancer_##id(){ enhancerFactory[#id] = makeEnhancer_##id; }		\
};										\
proxyEnhancer_##id pe_##id;


#define MAKE_PLUGIN_DETECTOR(className,id)					\
extern "C" {									\
std::shared_ptr<serviceDetector> makeDetector_##id(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server) {	\
   return std::make_shared<className>(p_sm, p_server);				\
}										\
}										\
class proxyDetector_##id { public:						\
   proxyDetector_##id(){ detectorFactory[#id] = makeDetector_##id; }		\
};										\
proxyDetector_##id pd_##id;
