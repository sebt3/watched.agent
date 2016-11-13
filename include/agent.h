#pragma once
#include <memory>
#include "services.h"
#include "selene.h"

extern const std::string SERVER_HEAD;
extern const std::string APPS_NAME;
extern const std::string APPS_DESC;
namespace watcheD {

void setResponse404(response_ptr response, std::string content);
void setResponseHtml(response_ptr response, std::string content);
void setResponseJson(response_ptr response, std::string content);

/*********************************
 * Config
 */
class Config {
public:
	Config(std::string p_fname);
	void save();
	Json::Value* 	getCollector(std::string p_name);
	Json::Value* 	getServer() { return &(data["server"]); }
	Json::Value* 	getServices() { return &(data["services"]); }
	Json::Value* 	getPlugins() { return &(data["plugins"]); }
	Json::Value* 	getAgent();
private:
	Json::Value	data;
	std::string	fname;
};

/*********************************
 * CollectorsManager
 */
class CollectorsManager {
public:
	CollectorsManager(std::shared_ptr<HttpServer> p_server, std::shared_ptr<Config> p_cfg);
	~CollectorsManager();
	void startThreads();
	void getJson(Json::Value* p_defs);
	void getIndexHtml(std::stringstream& stream );
private:
	std::shared_ptr<HttpServer> server;
	std::shared_ptr<Config> cfg;
	std::map<std::string, std::shared_ptr<Collector> >	collectors;
}; 

/*********************************
 * LuaCollector
 */
class LuaCollector : public Collector {
public:
	LuaCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, const std::string p_fname);

	void collect();
	void addRes(std::string p_name, std::string p_desc, std::string p_typeName);
	void addProp(std::string p_res, std::string p_name, std::string p_desc, std::string p_type);
	void nextVal(std::string p_res);
	void setProp(std::string p_res, std::string p_name, int val);
	void setPropD(std::string p_res, std::string p_name, double val);
	std::string getName(){ return name; }
private:
	sel::State state{true};
};


/*********************************
 * ServicesManager
 */
//TODO: plugins should be able to be written in lua

class servicesManager : public std::enable_shared_from_this<servicesManager> {
public:
	servicesManager(std::shared_ptr<HttpServer> p_server, std::shared_ptr<Config> p_cfg);
	void	init();
	void	find();
	void	addService(std::shared_ptr<service> p_serv);
	bool	haveSocket(uint32_t p_socket_id);
	bool	havePID(uint32_t p_pid);
	void	doGetJson(response_ptr response, request_ptr request);
	void	doGetServiceStatus(response_ptr response, request_ptr request);
	void	doGetRootPage(response_ptr response, request_ptr request);
	void	startThreads();
	std::shared_ptr<service> enhanceFromFactory(std::string p_id, std::shared_ptr<service> p_serv);
private:
	std::vector< std::shared_ptr<service> >		services;
	std::vector< std::shared_ptr<serviceDetector> >	detectors;
	std::vector< std::shared_ptr<serviceEnhancer> >	enhancers;
	std::shared_ptr<CollectorsManager>		systemCollectors;
	std::thread 					my_thread;
	bool						active;
	std::shared_ptr<HttpServer> 			server;
	std::shared_ptr<Config> 			cfg;
};


/*********************************
 * SocketDetector
 */
class socketDetector: public serviceDetector {
public:
	socketDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server):serviceDetector(p_sm, p_server) {}
	void find();
};

/*********************************
 * ServiceCPUCollector
 */
class serviceCollector : public Collector {
public:
	serviceCollector(std::string p_name, std::shared_ptr<service> p_serv, std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector(p_name, p_srv, p_cfg), serv(p_serv) {
		basePath = "/service/"+p_serv->getID()+"/";
	}
protected:
	std::weak_ptr<service> serv;
};
class serviceCpuCollector : public serviceCollector {
public:
	serviceCpuCollector(std::shared_ptr<service> p_serv, std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : serviceCollector("serviceCPU", p_serv, p_srv, p_cfg) {	}
	void	collect() {
		
	}
};



}

/*********************************
 * Access-Control-Allow-Origin for pistache
 *

class acao : public Net::Http::Header::Header {
public:
	static constexpr uint64_t Hash = Net::Http::Header::detail::hash("Access-Control-Allow-Origin");
	uint64_t hash() const { return Hash; }

	static constexpr const char *Name = "Access-Control-Allow-Origin";
	const char *name() const { return Name; }

	acao() : allow_("*") { } 
	acao(std::string value) : allow_(value) { }
	void parse(const std::string& str) { }
	void write(std::ostream& os) const { os << allow_; }
	
private:
	std::string allow_;
};*/
