#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace watcheD;

class CpuSpeedCollector : public Collector {
public:
	CpuSpeedCollector(HttpServer* p_srv, Json::Value* p_cfg) : Collector("cpuspeed", p_srv, p_cfg) {
		std::string	line;
		std::string	id;
		std::ifstream	infile("/proc/cpuinfo");
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, line.find("\t")) == "processor") {
				id = line.substr(line.find(" ")+1,line.length());
				addRessource(id, "CPU "+id+" speed (Mhz)", "cpuspeed");
				ressources[id]->addProperty("MHz", "CPU speed (Mhz)", "number");
			}
		}
		addGetMetricRoute();
		if (infile.good())
			infile.close();
	}

	void collect() {
		std::string		line;
		std::string		value;
		std::string		id = "";
		std::ifstream	infile("/proc/cpuinfo");
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, line.find("\t")) == "processor")
				id = line.substr(line.find(" ")+1,line.length());
			else if (line.substr(0, line.find("\t")) == "cpu MHz" && id != "") {
				value = line.substr(line.find(" ",6)+1,line.length());
				ressources[id]->nextValue();
				ressources[id]->setProperty("MHz", value);
			}
		}
		if (infile.good())
			infile.close();
	}
};

MAKE_PLUGIN_COLLECTOR(CpuSpeedCollector, "cpuspeed")
