#include "agent.h"

using namespace sel;
namespace watcheD {

LuaCollector::LuaCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, const std::string p_fname, std::shared_ptr<service> p_serv): Collector("lua", p_srv, p_cfg, 0, 0, p_serv) {
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
				"setType",	&service::setType,
				"setHost",	&service::setHost,
				"setName",	&service::setName
			);
		}
	}

	state["declare"]();
	addGetMetricRoute();
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
	state("pids = {}");
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
	state["collect"]();
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
LuaServiceEnhancer::LuaServiceEnhancer(std::shared_ptr<servicesManager> p_sm, const std::string p_fname): serviceEnhancer(p_sm) {
	state.Load(p_fname);
}

std::shared_ptr<service> LuaServiceEnhancer::enhance(std::shared_ptr<service> p_serv) {
	state["p_serv"].SetObj(*p_serv,
		"addCollector",	&service::addCollector,
		"havePID",	&service::havePID,
		"getType",	&service::getType,
		"getSubType",	&service::getSubType,
		"getName",	&service::getName,
		"getID",	&service::getID,
		"setType",	&service::setType,
		"setSubType",	&service::setSubType,
		"setUniqKey",	&service::setUniqKey,
		"setHost",	&service::setHost,
		"setName",	&service::setName
	);

	state("enhance(p_serv)");
	return nullptr;
}

}
