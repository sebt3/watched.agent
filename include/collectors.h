#pragma once
#include <json/json.h>
#include <map>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include "server_http.hpp"
#include "server_https.hpp"

namespace watcheD {
typedef std::shared_ptr<std::stringstream> response_ptr;
typedef std::shared_ptr<std::vector<std::string>> request_ptr;

/*********************************
 * Server
 */
typedef SimpleWeb::Server<SimpleWeb::HTTP> SWHttpServer;
typedef SimpleWeb::Server<SimpleWeb::HTTPS> SWHttpsServer;
class log;
class HttpServer {
public:
	HttpServer(Json::Value* p_cfg, Json::Value* p_logcfg);
	void setRegex(std::string p_opt, std::string p_regex, std::function<void(response_ptr, request_ptr)> p_fnct);
	void setDefault(std::string p_opt, std::function<void(response_ptr, request_ptr)> p_fnct);
	void start();
	std::string getHead(std::string p_title, std::string p_sub="");
	std::string getFoot(std::string p_script);
	void logError(  const std::string p_src, std::string p_message);
	void logWarning(const std::string p_src, std::string p_message);
	void logInfo(   const std::string p_src, std::string p_message);
	void logNotice( const std::string p_src, std::string p_message);
	void logDebug(  const std::string p_src, std::string p_message);
private:
	std::shared_ptr<SWHttpServer>   http;
	std::shared_ptr<SWHttpsServer>  https;
	std::shared_ptr<log>		l;
	Json::Value*			cfg;
	bool				useSSL;
};
/*********************************
 * Ressource
 */
class Ressource {
public:
	Ressource(uint p_size, std::string p_typeName): typeName(p_typeName), size(p_size) { }
	void nextValue();
	bool haveProperty(std::string p_name) { return v[0].isMember(p_name); }
	void setProperty(std::string p_name, Json::Value p_val) { v[0][p_name] = p_val;}
	void addProperty(std::string p_name, std::string p_desc, std::string p_typename) { d[p_name] = p_desc;t[p_name] = p_typename;}
	Json::Value* getValue(std::string p_name) { return &(v[0][p_name]); }
	std::string  getHistory(double since);
	std::string  getMorrisDesc();
	void getDefinition(Json::Value* p_defs);
	std::string  typeName;

private:
	std::vector<Json::Value>	  v;
	std::map<std::string,std::string> d;
	std::map<std::string,std::string> t;
	uint				  size;
};

class tickRessource : public Ressource {
public:
	tickRessource(uint p_size, uint p_freq, std::string p_typeName) : Ressource(p_size, p_typeName), freq(p_freq) { }
	void setTickValue(std::string p_name, uint p_value) {
		std::map<std::string,uint>::iterator it = old.find(p_name);
		if (it != old.end())
			setProperty(p_name, (p_value - old[p_name])/freq);
		else	setProperty(p_name, 0);
		old[p_name] = p_value;
	}
private:
	std::map<std::string,uint>	old;
	uint				freq;
};


/*********************************
 * Collector
 */
class service;
struct timer_killer {
	template<class R, class P>
	bool wait_for( std::chrono::duration<R,P> const& time ) {
		std::unique_lock<std::mutex> lock(m);
		return !cv.wait_for(lock, time, [&]{return terminate;});
	}
	void kill() {
		std::unique_lock<std::mutex> lock(m);
		terminate=true;
		cv.notify_all();
	}
	std::mutex m;
private:
	std::condition_variable cv;
	bool terminate = false;
};

class Collector {
public:
	Collector(std::string p_name, std::shared_ptr<HttpServer> p_server, Json::Value* p_cfg, uint p_history = 300, uint p_freq_pool = 5, std::shared_ptr<service> p_serv = nullptr);
	~Collector();
	Collector(const Collector& that) = delete;
	void startThread();
	
	virtual void collect() =0;

	void doGetHistory(response_ptr response, request_ptr request);
	void doGetGraph(response_ptr response, request_ptr request);
	void getDefinitions(Json::Value* p_defs);
	void getPaths(Json::Value* p_defs);
	void setService(std::shared_ptr<service> p_serv);
	std::string getHost();
	void getIndexHtml(std::stringstream& stream );
protected:
	void addGetMetricRoute();
	void addRessource(std::string p_name, std::string p_desc, std::string p_typeName);
	std::map<std::string, std::shared_ptr<Ressource> >	ressources;
	std::map<std::string, std::string>			desc;
	Json::Value*			cfg;
	std::string			morrisType;
	std::string			morrisOpts;
	std::string			name;
	std::string			basePath;
	std::string			host;
	std::weak_ptr<service>		onService;
	bool				haveService;
private:
	bool				active;
	timer_killer			timer;
	std::thread			my_thread;
	std::shared_ptr<HttpServer>	server;
	std::mutex			lock;
};


/*********************************
 * Plugin management
 */
typedef std::shared_ptr<Collector> collector_maker_t(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg);
extern std::map<std::string, collector_maker_t* > collectorFactory;

}

#define associate(s,type,regex,method)				\
server->setRegex(type,regex,[this](response_ptr response, request_ptr request) { this->method(response, request); });
#define MAKE_PLUGIN_COLLECTOR(className,id)			\
extern "C" {									\
std::shared_ptr<Collector> maker_##id(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg){	\
   return std::make_shared<className>(p_srv, p_cfg);				\
}								\
}								\
class proxy_##id { public:					\
   proxy_##id(){ collectorFactory[#id] = maker_##id; }		\
};								\
proxy_##id p;


