#pragma once

#include "common.h"

#include <mysql++/mysql++.h>

/*#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/client.h>*/

extern const std::string SERVER_HEAD;
extern const std::string APPS_NAME;
extern const std::string APPS_DESC;

namespace watcheD {

class dbPool : public mysqlpp::ConnectionPool {
public:
	dbPool(Json::Value* p_cfg) :cfg(p_cfg), usedCount(0) { }
	~dbPool() { clear(); }
	mysqlpp::Connection* grab() {
		unsigned int pool_size = (*cfg)["pool_size"].asInt();
		while (usedCount > pool_size)
			std::this_thread::sleep_for(std::chrono::seconds(1));
		++usedCount;
		return mysqlpp::ConnectionPool::grab();
	}
	void release(const mysqlpp::Connection* pc) {
		mysqlpp::ConnectionPool::release(pc);
		--usedCount;
	}
protected:
	mysqlpp::Connection* create() {
		return new mysqlpp::Connection((*cfg)["database_name"].asCString(), (*cfg)["connection_string"].asCString(), (*cfg)["login"].asCString(), (*cfg)["password"].asCString());
	}
	void destroy(mysqlpp::Connection* cp){ delete cp; }
	unsigned int max_idle_time() { return 10; }
private:
	Json::Value* cfg;
	unsigned int usedCount;

};
	
	
class dbTools {
public:
	dbTools(dbPool*	p_db) : dbp(p_db) { }
protected:
	bool	haveRessource(std::string p_name);
	bool	haveTable(std::string p_name);
	bool	tableHasColumn(std::string p_name, std::string p_col);
	dbPool	*dbp;
};

/*********************************
 * ressourceClient
 */
struct event {
	uint32_t	event_type;
	std::string	property;
	char		oper;
	double		value;
	inline bool operator==(const event& l) const {
		return ( (l.event_type == event_type) && (l.property == property) && (l.oper == oper) && (l.value == value) );
	}
};

class ressourceClient : public dbTools {
public:
	ressourceClient(uint32_t p_agtid, uint32_t p_resid, std::string p_url, std::string p_table, Json::Value *p_def, dbPool*	p_db, HttpClient* p_client) : dbTools(p_db), agt_id(p_agtid), res_id(p_resid), client(p_client), baseurl(p_url), table(p_table), def(p_def) { }
	void	init();
	void	collect();

private:
	double  getSince();

	uint32_t		agt_id;
	uint32_t		res_id;
	HttpClient		*client;
	std::string		baseurl;
	std::string		table;
	Json::Value		*def;
	std::string		baseInsert;
	std::vector<struct event*> event_factory;
	std::map<uint32_t, struct event*> current_events;
};

/*********************************
 * agentClient
 */
class agentClient : public dbTools {
public:
	agentClient(uint32_t p_id, dbPool* p_db) : dbTools(p_db), ready(false), active(false), id(p_id) { }
	~agentClient();
	void	init();
	void	startThread();

private:
	void	createTables();
	void	createRessources();
	uint32_t getRessourceId(std::string p_res);

	bool			ready;
	bool			active;
	uint32_t		id;
	HttpClient		*client;
	std::string		baseurl;
	Json::Value		api;
	std::vector<ressourceClient*>		ressources;
	std::thread		my_thread;
	uint32_t		pool_freq;
};

/*********************************
 * statAggregator
 */
class statAggregator : public dbTools {
public:
	statAggregator(dbPool*	p_db, Json::Value* p_aggregCfg);
	void	init();
	void	startThread();
private:
	std::map<std::string, std::string>	base_am;
	std::map<std::string, std::string>	base_ah;
	bool			active;
	std::thread		my_thread;
	Json::Value* 		cfg;
};

/*********************************
 * agentManager
 */
class agentManager : public dbTools {
public:
	agentManager(dbPool* p_db) : dbTools(p_db) { }
	void	init(Json::Value* p_aggregCfg);
	void	startThreads();
private:
	std::map<uint32_t, agentClient*>	agents;
	statAggregator*				aggreg;
};
}
