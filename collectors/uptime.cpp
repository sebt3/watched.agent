#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace std;
using namespace watcheD;

class Uptime : public Collector {
public:
	Uptime(HttpServer* p_srv, Json::Value* p_cfg) : Collector("uptime", p_srv, p_cfg, 100, 300) {
		string		line;
		addRessource("uptime", "Load average", "uptime");
		ressources["uptime"]->addProperty("uptime",  "System uptime", "number");
		addGetMetricRoute();
	}

	void collect() {
		string		line;
		ifstream	infile("/proc/uptime");
		while(infile.good() && getline(infile, line)) {
			ressources["uptime"]->nextValue();
			ressources["uptime"]->setProperty("uptime", stod(line.substr(0, line.find(" ",0))));
		}
		if (infile.good())
			infile.close();
	}
};

MAKE_PLUGIN_COLLECTOR(Uptime, "uptime")
