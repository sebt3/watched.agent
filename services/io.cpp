#include "services.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace watcheD;

// This is exactly the same as in the io.lua file. This is here for memory usage difference only
class ServioCollector : public Collector {
public:
	ServioCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_serv): Collector("io", p_srv, p_cfg, 150, 10, p_serv) {
		addRessource("io", "Service IO usage", "service_io");
		ressources["io"]->addProperty("rd",  "Read bytes",	"number");
		ressources["io"]->addProperty("wt",  "Write bytes",	"number");
		addGetMetricRoute();
		morrisType="Area";morrisOpts="  ";
	}

	void collect() {
		std::shared_ptr<service> serv_p = onService.lock();
		if (!serv_p) return;
		std::shared_ptr< std::vector<uint32_t> > pids = serv_p->getPIDs();
		uint32_t rd  = 0;
		uint32_t wt = 0;
		for (std::vector<uint32_t>::iterator i=pids->begin();i!=pids->end();i++) {
			std::ifstream	infile("/proc/"+std::to_string(*i)+"/io");
			std::string	line;
			if (!infile.good()) continue;
			while(std::getline(infile, line)) {
				if (line.find(":") == std::string::npos) continue;
				std::string key   = line.substr(0,line.find(":"));
				std::string value = line.substr(key.length()+1);
				if (key == "read_bytes") {
					uint32_t v = atoi(value.c_str());
					if (reads.find(*i) != reads.end() && reads[*i] < v)
						rd += v - reads[*i];
					reads[*i] = v;
				} else if (key == "write_bytes") {
					uint32_t v = atoi(value.c_str());
					if (writes.find(*i) != writes.end() && writes[*i] < v) 
						wt += v - writes[*i];
					writes[*i] = v;
				}
			}
			infile.close();
		}
		ressources["io"]->nextValue();
		ressources["io"]->setProperty("rd",		rd);
		ressources["io"]->setProperty("wt",		wt);
	}
private:
	std::map<uint64_t, uint32_t> reads;
	std::map<uint64_t, uint32_t> writes;
};

MAKE_PLUGIN_SERVICE_COLLECTOR(ServioCollector, io)
