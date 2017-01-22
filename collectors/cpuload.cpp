#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace watcheD;
namespace collectors {

class CpuLoadCollector : public Collector {
public:

	CpuLoadCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("cpuload", p_srv, p_cfg, 150, 10) {
		addRessource("avg", "Load average", "loadaverage");
		ressources["avg"]->addProperty("avg1",  "Load average (1mn)", "number");
		ressources["avg"]->addProperty("avg5",  "Load average (5mn)", "number");
		ressources["avg"]->addProperty("avg15", "Load average (15mn)", "number");
		addGetMetricRoute();
	}

	void collect() {
		std::string	line;
		std::ifstream	infile("/proc/loadavg");
		while(infile.good() && getline(infile, line)) {
			ressources["avg"]->nextValue();
			int pos = line.find(" ",0);
			ressources["avg"]->setProperty("avg1", stod(line.substr(0, pos)));
			ressources["avg"]->setProperty("avg5", stod(line.substr(pos, line.find(" ",pos+1)-pos)));
			pos = line.find(" ",pos+1);
			ressources["avg"]->setProperty("avg15", stod(line.substr(pos, line.find(" ",pos+1)-pos)));
		}
		if (infile.good())
			infile.close();
	}
};

MAKE_PLUGIN_COLLECTOR(CpuLoadCollector, cpuload)

}
