#include "agent.h"

#include <list>
#include <fstream>

using namespace std;
namespace watcheD {


Config::Config(std::string p_fname) : fname(p_fname) {
	// reading the file
	ifstream cfgif (fname);
	if (cfgif.good()) {
		cfgif >> data;
		cfgif.close();
	}

	// Server configuration
	if (! data.isMember("server") || ! data["server"].isMember("thread")) {
		data["server"]["threads"] = 8;
		data["server"]["threads"].setComment(std::string("/*\t\tNumber of concurrent threads use to reply on network interface */"), Json::commentAfterOnSameLine);
	}
	if (! data["server"].isMember("host")) {
		data["server"]["host"] = "";
		data["server"]["host"].setComment(std::string("/*\t\tHost string to listen on (default: all interfaces) */"), Json::commentAfterOnSameLine);
	}
	if (! data["server"].isMember("port")) {
		data["server"]["port"] = 9080;
		data["server"]["port"].setComment(std::string("/*\t\tTCP port number */"), Json::commentAfterOnSameLine);
	}

	// services
	if (! data.isMember("services") || ! data["services"].isMember("config_dir")) {
		data["services"]["config_dir"] = "cfgs";
		data["services"]["config_dir"].setComment(std::string("/*\t\tThe directory where the *.json service configuration files are stored*/"), Json::commentAfterOnSameLine);
	}
	if (! data["services"].isMember("find_frequency")) {
		data["services"]["find_frequency"] = 120;
		data["services"]["find_frequency"].setComment(std::string("/*\t\t\t\tThe frequency (in sec)  to pool for new (or restarted) services*/"), Json::commentAfterOnSameLine);
	}

	// plugins
	if (! data.isMember("plugins") || ! data["plugins"].isMember("services_cpp")) {
		data["plugins"]["services_cpp"] = "services";
		data["plugins"]["services_cpp"].setComment(std::string("/*\t\tThe directory where the *.so services plugins are stored*/"), Json::commentAfterOnSameLine);
	}
	if (! data["plugins"].isMember("collectors_cpp")) {
		data["plugins"]["collectors_cpp"] = "collectors";
		data["plugins"]["collectors_cpp"].setComment(std::string("/*\t\tThe directory where the *.so collector plugins are stored*/"), Json::commentAfterOnSameLine);
	}
	if (! data["plugins"].isMember("collectors_lua")) {
		data["plugins"]["collectors_lua"] = "../luaCollectors";
		data["plugins"]["collectors_lua"].setComment(std::string("/*\t\tThe directory where the *.lua collector plugins are stored*/"), Json::commentAfterOnSameLine);
	}
}

void 		Config::save() {
	std::ofstream cfgof (fname, std::ifstream::out);
	cfgof<<data;
	cfgof.close();
}

Json::Value* 	Config::getCollector(std::string p_name) {
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("collectors")) {
		data["collectors"][p_name] = obj_value;
		data["collectors"].setComment(std::string("/*\tConfigure the collectors plugins */"), Json::commentBefore);
	}else if(! data["collectors"].isMember(p_name) ) {
		data["collectors"][p_name] = obj_value;
	}
	return &(data["collectors"][p_name]); 
}

Json::Value* 	Config::getAgent() { 
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("agent")) {
		data["agent"] = obj_value;
		data["agent"].setComment(std::string("/*\tRemote agent specific configuration */"), Json::commentBefore);
	}
	return &(data["agent"]); 
}

}
