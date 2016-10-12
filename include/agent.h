#pragma once
#include "collectors.h"

extern const std::string SERVER_HEAD;
extern const std::string APPS_NAME;
extern const std::string APPS_DESC;
namespace watcheD {

/*********************************
 * Config
 */
class Config {
public:
	Config(std::string p_fname);
	void save();
	Json::Value* 	getCollector(std::string p_name);
	Json::Value* 	getServer() { return &(data["server"]); }
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
	CollectorsManager(HttpServer *p_server, Config* p_cfg);
	~CollectorsManager();
	void startThreads();
	void doGetJson(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request);
	void doGetRootPage(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request);
	HttpServer* server;
	Config *cfg;
	std::map<std::string, Collector*>	collectors;
}; 

}


/*********************************
 * Access-Control-Allow-Origin
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
