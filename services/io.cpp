#include "services.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace watcheD;
namespace services {
//
// This is exactly the same as in the io.lua file. This is here for memory usage difference only
class ServioCollector : public Collector {
public:
	ServioCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_serv): Collector("io", p_srv, p_cfg, 150, 10, p_serv) {
		desc["io"] = "Service IO usage";
		ressources["io"]= std::make_shared<pidRessource>((*cfg)["history"].asUInt(),1.0,"service_io");
		ressources["io"]->addProperty("rd",  "Read bytes",	"number");
		ressources["io"]->addProperty("wt",  "Write bytes",	"number");
		addGetMetricRoute();
		morrisType="Area";morrisOpts="  ";
	}

	void collect() {
		std::shared_ptr<service> serv_p = onService.lock();
		if (!serv_p) return;
		std::shared_ptr< std::vector<uint32_t> > pids = serv_p->getPIDs();
		std::shared_ptr<pidRessource>	res = reinterpret_cast<std::shared_ptr<pidRessource>&>(ressources["io"]);
		res->nextValue();
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
					res->setPidValue("rd",*i,v);
				} else if (key == "write_bytes") {
					uint32_t v = atoi(value.c_str());
					res->setPidValue("wt",*i,v);
				}
			}
			infile.close();
		}
	}
};

MAKE_PLUGIN_SERVICE_COLLECTOR(ServioCollector, io)

}
