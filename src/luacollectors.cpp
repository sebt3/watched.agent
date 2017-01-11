#include "agent.h"

using namespace sel;
namespace watcheD {

LuaCollector::LuaCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, const std::string p_fname, std::shared_ptr<service> p_serv): Collector("lua", p_srv, p_cfg, 0, 0, p_serv), have_state(true) {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	state.Load(p_fname);
	if(! cfg->isMember("history")) (*cfg)["history"] = (int)state["cfg"]["history"];
	if(! cfg->isMember("poll-frequency")) (*cfg)["poll-frequency"] = (int)state["cfg"]["poolfreq"];
	std::string n = state["cfg"]["name"];name = n;

	state["this"].SetObj(*this,
		"addRessource", &LuaCollector::addRes,
		"addProperty",  &LuaCollector::addProp,
		"nextValue",    &LuaCollector::nextVal,
		"setProperty",  &LuaCollector::setProp,
		"setDProperty", &LuaCollector::setPropD,
		"getPIDList",	&LuaCollector::getPIDList
	);
	if (haveService) {
		std::shared_ptr<service> serv_p = onService.lock();
		if (serv_p) {
			state["service"].SetObj(*serv_p,
				"havePID",		&service::havePID,
				"getType",		&service::getType,
				"getSubType",		&service::getSubType,
				"getName",		&service::getName,
				"getID",		&service::getID
			);
		}
	}

	state["declare"]();
	addGetMetricRoute();
}

LuaCollector::~LuaCollector() {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	have_state = false;
}

void LuaCollector::addRes(std::string p_name, std::string p_desc, std::string p_typeName)
{ addRessource(p_name, p_desc, p_typeName); }
void LuaCollector::addProp(std::string p_res, std::string p_name, std::string p_desc, std::string p_type)	{ ressources[p_res]->addProperty(p_name, p_desc, p_type); }
void LuaCollector::nextVal(std::string p_res)
{ ressources[p_res]->nextValue(); }
void LuaCollector::setProp(std::string p_res, std::string p_name, int val)
{ ressources[p_res]->setProperty(p_name, val); }
void LuaCollector::setPropD(std::string p_res, std::string p_name, double val)
{ ressources[p_res]->setProperty(p_name, val); }

void LuaCollector::getPIDList() {
	state("if type(pids) == \"table\" then while not ( type(pids[1]) == \"nil\" ) do table.remove(pids); end else pids={} end");
	if (haveService) {
		std::shared_ptr<service> serv_p = onService.lock();
		if (serv_p) {
			std::shared_ptr< std::vector<uint32_t> > pids = serv_p->getPIDs();
			for(std::vector<uint32_t>::iterator i = pids->begin();i!= pids->end();i++) {
				std::string cmd = "table.insert(pids,"+std::to_string(*i)+")";
				state(cmd.c_str());
			}
		}
	}
}

void LuaCollector::collect() {
	std::unique_lock<std::mutex> locker(lua);  // Lua isnt exactly thread safe
	if (have_state) 
		state["collect"]();
}


/*********************************
 * LuaParser
 */

LuaParser::LuaParser(const std::string p_logname, Json::Value* p_cfg, const std::string p_luafile) : logParser(p_logname, p_cfg,0,0), have_state(true)  {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	state.Load(p_luafile);
	std::string cmd="ok="+std::to_string(ok)+";notice="+std::to_string(notice)+";info="+std::to_string(info)+";warning="+std::to_string(warning)+";error="+std::to_string(error)+";critical="+std::to_string(critical);
	state(cmd.c_str());
	if(! cfg->isMember("history")) (*cfg)["history"] = (int)state["cfg"]["history"];
	if(! cfg->isMember("poll-frequency")) (*cfg)["poll-frequency"] = (int)state["cfg"]["poolfreq"];
}

LuaParser::~LuaParser() {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	have_state = false;
}

logParser::levels	LuaParser::getLevel(std::string p_line) {
	std::unique_lock<std::mutex> locker(lua);  // Lua isnt exactly thread safe
	if (have_state) {
		int ret = state["getLevel"](p_line);
		return (logParser::levels)ret;
	}
	return ok;
}

std::string		LuaParser::getDate (std::string p_line) {
	std::unique_lock<std::mutex> locker(lua);  // Lua isnt exactly thread safe
	if (have_state) {
		return state["getDate"](p_line);
	}
	return "";
}

/*********************************
 * LuaSorter
 */
LuaSorter::LuaSorter(const std::string p_fname) {
	state.Load(p_fname);
}

bool LuaSorter::isType(const std::string p_typename) {
	sel::Selector types = state["types"];
	std::string x="x";
	for(int i=1;x!="";i++) {
		std::string y = types[i];
		x = y;
		if (y== p_typename)
			return true;
	}
	return false;
}

/*********************************
 * LuaServiceEnhancer
 */
LuaServiceEnhancer::LuaServiceEnhancer(std::shared_ptr<servicesManager> p_sm, const std::string p_fname): serviceEnhancer(p_sm), have_state(true) {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	state.Load(p_fname);
}

LuaServiceEnhancer::~LuaServiceEnhancer() {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	have_state = false;
}

std::shared_ptr<service> LuaServiceEnhancer::enhance(std::shared_ptr<service> p_serv) {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	if (!have_state) 
		return nullptr;
	state["p_serv"].SetObj(*p_serv,
		"addCollector",		&service::addCollector,
		"havePID",		&service::havePID,
		"getType",		&service::getType,
		"getSubType",		&service::getSubType,
		"getName",		&service::getName,
		"getID",		&service::getID,
		"setType",		&service::setType,
		"setSubType",		&service::setSubType,
		"setUniqKey",		&service::setUniqKey,
		"setHost",		&service::setHost,
		"setHandler",		&service::setHandler,
		"addLogMonitor",	&service::addLogMonitor,
		"updateBasePaths",	&service::updateBasePaths,
		"setName",		&service::setName
	);

	state("enhance(p_serv)");
	return nullptr;
}


/*********************************
 * LuaServiceEnhancer
 */
LuaServiceHandler::LuaServiceHandler(std::shared_ptr<service> p_s, const std::string p_fname): serviceHandler(p_s),have_state(true) {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	state.Load(p_fname);
}

bool	LuaServiceHandler::isBlackout() {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	if (!have_state) 
		return false;
	std::shared_ptr<service> s = serv.lock();
	if(!s) return false;

	state["p_serv"].SetObj(*s,
		"havePID",		&service::havePID,
		"getType",		&service::getType,
		"getSubType",		&service::getSubType,
		"getName",		&service::getName,
		"getID",		&service::getID
	);

	state("p_ret = isBlackout(p_serv)");
	return state["p_ret"];
}

/*********************************
 * LuaDetector
 */
LuaDetector::LuaDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server, const std::string p_fname):serviceDetector(p_sm, p_server) {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	state.Load(p_fname);
}

void	LuaDetector::find() {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	if (!have_state) 
		return;
	std::shared_ptr<servicesManager> mgr = services.lock();
	if(!mgr) return;
	state["mgr"].SetObj(*mgr,
//		"addService",		&servicesManager::addService,
		"haveSocket",		&servicesManager::haveSocket,
		"havePID",		&servicesManager::havePID
	);
	//TODO: add the ability to create service and add them to the manager working around the shared_ptr issue
/*	state["service"].SetClass<service>(
		"addCollector",		&service::addCollector,
		"havePID",		&service::havePID,
		"getType",		&service::getType,
		"getSubType",		&service::getSubType,
		"getName",		&service::getName,
		"getID",		&service::getID,
		"setType",		&service::setType,
		"setSubType",		&service::setSubType,
		"setUniqKey",		&service::setUniqKey,
		"setHost",		&service::setHost,
		"setHandler",		&service::setHandler,
		"addLogMonitor",	&service::addLogMonitor,
		"updateBasePaths",	&service::updateBasePaths,
		"setName",		&service::setName
	);*/
	
	state["find"]();
}

}
