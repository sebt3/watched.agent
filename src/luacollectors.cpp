#include "agent.h"

using namespace watcheD;
using namespace sel;

LuaCollector::LuaCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, const std::string p_fname): Collector("lua", p_srv, p_cfg, 0, 0) {
	state.Load(p_fname);
	if(! cfg->isMember("history")) (*cfg)["history"] = (int)state["cfg"]["history"];
	if(! cfg->isMember("poll-frequency")) (*cfg)["poll-frequency"] = (int)state["cfg"]["poolfreq"];
	std::string n = state["cfg"]["name"];name = n;

	state["this"].SetObj(*this,
		"addRessource", &LuaCollector::addRes,
		"addProperty",  &LuaCollector::addProp,
		"nextValue",    &LuaCollector::nextVal,
		"setProperty",  &LuaCollector::setProp,
		"setDProperty", &LuaCollector::setPropD
	);

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

void LuaCollector::collect() {
	state["collect"]();
}
