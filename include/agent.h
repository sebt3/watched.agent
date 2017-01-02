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
	Json::Value* 	getLog();
private:
	Json::Value	data;
	std::string	fname;
};

class log {
public:
	log(Json::Value* p_cfg);
	void write(uint16_t p_lvl, const std::string p_src, std::string p_message);
	void write(std::string p_lvl, const std::string p_src, std::string p_message);
	void error(const std::string p_src, std::string p_message);
	void warning(const std::string p_src, std::string p_message);
	void info(const std::string p_src, std::string p_message);
	void notice(const std::string p_src, std::string p_message);
	void debug(const std::string p_src, std::string p_message);
private:
	Json::Value*	cfg;
	uint16_t	level;
	static const std::vector<std::string> levels;
	std::mutex	mutex;
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
	LuaCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, const std::string p_fname, std::shared_ptr<service> p_serv = nullptr);
	~LuaCollector();

	void collect();
	void addRes(std::string p_name, std::string p_desc, std::string p_typeName);
	void addProp(std::string p_res, std::string p_name, std::string p_desc, std::string p_type);
	void nextVal(std::string p_res);
	void setProp(std::string p_res, std::string p_name, int val);
	void setPropD(std::string p_res, std::string p_name, double val);
	void getPIDList();
	std::string getName(){ return name; }
private:
	sel::State	state{true};
	bool		have_state;
	std::mutex	lua;
};

/*********************************
 * LuaSorter
 * find what type of service management plugin a lua file contain
 */
class LuaSorter {
public:
	LuaSorter(const std::string p_fname);
	bool isType(const std::string p_typename);
private:
	sel::State state{true};
};

/*********************************
 * LuaServiceEnhancer
 */
class LuaServiceEnhancer: public serviceEnhancer {
public:
	LuaServiceEnhancer(std::shared_ptr<servicesManager> p_sm, const std::string p_fname);
	~LuaServiceEnhancer();
	std::shared_ptr<service> enhance(std::shared_ptr<service> p_serv);
private:
	sel::State state{true};
	bool		have_state;
	std::mutex	lua;
};


/*********************************
 * SocketDetector
 */
class socketDetector: public serviceDetector {
public:
	socketDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server):serviceDetector(p_sm, p_server) {}
	void find();
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
