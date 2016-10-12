#pragma once

#include <json/json.h>
#include <map>
#include <string>
#include <thread>
#include "server_http.hpp"
#include "client_http.hpp"
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;

namespace watcheD {

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

class Collector {
public:
	Collector(std::string p_name, HttpServer* p_server, Json::Value* p_cfg, uint p_history = 300, uint p_freq_pool = 5);
	~Collector();
	void startThread();
	
	virtual void collect() =0;

	//void thread();
	void doGetHistory(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request);
	void doGetGraph(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request);
	void getDefinitions(Json::Value* p_defs);
	void getPaths(Json::Value* p_defs);
	void getIndexHtml(std::stringstream& stream );
protected:
	void addGetMetricRoute();
	void addRessource(std::string p_name, std::string p_desc, std::string p_typeName);
	std::map<std::string, Ressource*>	ressources;
	std::map<std::string, std::string>	desc;
	Json::Value* cfg;
	std::string morrisType;
	std::string morrisOpts;
private:
	bool active;
	std::string name;
	std::thread my_thread;
	HttpServer* server;
};


/*********************************
 * Plugin management
 */
typedef Collector *collector_maker_t(HttpServer* p_srv, Json::Value* p_cfg);
extern std::map<std::string, collector_maker_t *> factory;

}

#define associate(s,type,regex,method)				\
server->resource[regex][type]=[this](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) { this->method(response, request); }
#define MAKE_PLUGIN_COLLECTOR(className,id)			\
extern "C" {							\
Collector *maker(HttpServer* p_srv, Json::Value* p_cfg){	\
   return new className(p_srv, p_cfg);				\
}								\
}								\
class proxy { public:						\
   proxy(){ factory[id] = maker; }				\
};								\
proxy p;


