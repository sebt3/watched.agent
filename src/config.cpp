#include "common.h"

#include <list>
#include <fstream>

using namespace std;
using namespace watcheD;


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
	if (! data["server"].isMember("collectors_dir")) {
		data["server"]["collectors_dir"] = "plugins";
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

Json::Value* 	Config::getAggregate() { 
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("aggregate")) {
		data["aggregate"] = obj_value;
		data["aggregate"].setComment(std::string("/*\tConfigure the data aggregate process */"), Json::commentBefore);
	}
	return &(data["aggregate"]); 
}

Json::Value* 	Config::getCentral() {
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("central")) {
		data["central"] = obj_value;
		data["central"].setComment(std::string("/*\tCentral server speccific configuration */"), Json::commentBefore);
	}
	return &(data["central"]);
}

Json::Value* 	Config::getDB() {
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("db")) {
		data["db"] = obj_value;
		data["db"].setComment(std::string("/*\tMySQL database informations */"), Json::commentBefore);
	}
	if (!data["db"].isMember("connection_string")) {
		data["db"]["connection_string"]	= "localhost:3306";
		data["db"]["connection_string"].setComment(std::string("/*\tMySQL database connection string */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("database_name")) {
		data["db"]["database_name"]		= "sebtest";
		data["db"]["database_name"].setComment(std::string("/*\t\tMySQL database name */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("login")) {
		data["db"]["login"]			= "seb";
		data["db"]["login"].setComment(std::string("/*\t\t\tMySQL login */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("password")) {
		data["db"]["password"]		= "seb";
		data["db"]["password"].setComment(std::string("/*\t\t\tMySQL password */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("pool_size")) {
		data["db"]["pool_size"]		= 32;
		data["db"]["pool_size"].setComment(std::string("/*\t\t\tNumber of concurrent connections to the database */"), Json::commentAfterOnSameLine);
	}

	return &(data["db"]);
}
