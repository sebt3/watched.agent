#include "services.h"
#include <fstream>
#include <iostream>
#include <chrono>

// This is exactly the same as in the mem.lua file. This is here for memory usage difference only
using namespace watcheD;
namespace services {

class ServMemCollector : public Collector {
public:
	ServMemCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_serv): Collector("mem", p_srv, p_cfg, 25, 60, p_serv) {
		addRessource("mem", "Service memory usage", "serviceMemory");
		ressources["mem"]->addProperty("total",  "Total Memory usage (kB)",	"number");
		ressources["mem"]->addProperty("swap",   "Swapped (kB)",		"number");
		ressources["mem"]->addProperty("shared", "Shared Libraries (kB)",	"number");
		addGetMetricRoute();
		morrisType="Area";morrisOpts="  ";
	}

	void collect() {
		std::shared_ptr<service> serv_p = onService.lock();
		if (!serv_p) return;
		std::shared_ptr< std::vector<uint32_t> > pids = serv_p->getPIDs();
		uint32_t total  = 0;
		uint32_t shared = 0;
		uint32_t swap   = 0;
		for (std::vector<uint32_t>::iterator i=pids->begin();i!=pids->end();i++) {
			std::ifstream	infile("/proc/"+std::to_string(*i)+"/smaps");
			std::string	line;
			if (!infile.good()) continue;
			while(std::getline(infile, line)) {
				if (line.find(" kB") == std::string::npos) continue;
				std::string key   = line.substr(0,line.find(":"));
				std::string value = line.substr(key.length()+1, line.find(" kB"));
				if (key == "Rss")		total  += atoi(value.c_str());
				else if (key == "Swap")		swap   += atoi(value.c_str());
				else if (key == "Shared_Clean")	shared += atoi(value.c_str());
			}
			infile.close();
		}
		ressources["mem"]->nextValue();
		ressources["mem"]->setProperty("total",		total);
		ressources["mem"]->setProperty("swap",		swap);
		ressources["mem"]->setProperty("shared",	shared);
	}
};

MAKE_PLUGIN_SERVICE_COLLECTOR(ServMemCollector, mem)


}
