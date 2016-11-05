#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>


using namespace std;
using namespace watcheD;

class vmStatCollector : public Collector {
public:
	vmStatCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("vmstat", p_srv, p_cfg) {
		string		line;
		string		id;
		ifstream	infile("/proc/vmstat");
		std::shared_ptr<tickRessource> 	res	= std::make_shared<tickRessource>((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt(), "memory_stats");
		ressources["stat"]	= res;
		desc["stat"]		= "Memory usage statistics";
		while(infile.good() && getline(infile, line)) {
			id = line.substr(0,line.find(" "));
			if (id == "pgpgin")
				res->addProperty(id, "Page in /s", "number");
			else if (id == "pgpgout")
				res->addProperty(id, "Page out /s", "number");
			else if (id == "pswpin")
				res->addProperty(id, "Swap in /s", "number");
			else if (id == "pswpout")
				res->addProperty(id, "Swap out /s", "number");
			else if (id == "pgfree")
				res->addProperty(id, "Page free /s", "number");
			else if (id == "pgactivate")
				res->addProperty(id, "Page Activations /s", "number");
			else if (id == "pgdeactivate")
				res->addProperty(id, "Page Desactivations /s", "number");
			else if (id == "pgfault")
				res->addProperty(id, "Page faults /s", "number");
		}
		addGetMetricRoute();
		//morrisType="Area";morrisOpts="  ";
		if (infile.good())
			infile.close();
	}

	void collect() {
		string		line;
		string		id = "";
		ifstream	infile("/proc/vmstat");
		std::shared_ptr<tickRessource>	res = reinterpret_cast<std::shared_ptr<tickRessource>&>(ressources["stat"]);
		res->nextValue();
		while(infile.good() && getline(infile, line)) {
			id = line.substr(0,line.find(" "));
			if (id == "pgpgin" || id == "pgpgout" || id == "pswpin" || id == "pswpout" || id == "pgfree" || id == "pgactivate" || id == "pgdeactivate"|| id == "pgfault") {
				res->setTickValue(id, atoi( line.substr(line.find(" ")+1).c_str() ));
			}
		}
		if (infile.good())
			infile.close();
	}
};


MAKE_PLUGIN_COLLECTOR(vmStatCollector, vmstat)
