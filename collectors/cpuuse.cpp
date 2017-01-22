#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>

using namespace watcheD;
namespace collectors {

class cpuTicks : public Ressource {
public:
	cpuTicks(uint p_size) : Ressource(p_size, "cpu_usage") {
		addProperty("user",   "Normal processes usage(%)", "number");
		addProperty("nice",   "Nice processes usage(%)", "number");
		addProperty("system", "System usage(%)", "number");
		addProperty("iowait", "IO wait(%)", "number");
		addProperty("irq",    "Sevicing interupts(%)", "number");
		addProperty("softirq","Sevicing softirq(%)", "number");
	}
	void setRaw(std::vector<std::string> raw) {
		uint sum;
		std::string id;
		
		live["user"]	=atoi(raw[1].c_str());
		live["nice"]	=atoi(raw[2].c_str());
		live["system"]	=atoi(raw[3].c_str());
		live["idle"]	=atoi(raw[4].c_str());
		live["iowait"]	=atoi(raw[5].c_str());
		live["irq"]	=atoi(raw[6].c_str());
		live["softirq"]	=atoi(raw[7].c_str());
		if (live["iowait"]>10000000) live["iowait"] = 0; // very high iowait sound too bogus to report and it polute gfx viewes
		sum=0;
		for(std::map<std::string,uint>::iterator i=live.begin();i!=live.end();i++) {
			id = i->first;
			diff[id] = live[id] - old[id];
			sum+=diff[id];
			old[id] = live[id];
		}
		for(std::map<std::string,uint>::iterator i=live.begin();i!=live.end();i++) {
			if (i->first!="idle")
				setProperty(i->first, (double)diff[i->first]*100/sum);
		}
		
	}
private:
	std::map<std::string,uint>	live;
	std::map<std::string,uint>	old;
	std::map<std::string,uint>	diff;
};

class CpuUseCollector : public Collector {
public:
	CpuUseCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("cpuuse", p_srv, p_cfg) {
		std::string	line;
		std::string	id;
		std::ifstream	infile("/proc/stat");
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, 3) == "cpu") {
				if (line.substr(0, 4) == "cpu ")	id = "all";
				else					id = line.substr(3, line.find(" ")-3);
				ressources[id]	= std::make_shared<cpuTicks>((*cfg)["history"].asUInt());
				desc[id]	= "CPU "+id+" usage";
			} else if (line.substr(0, 4) == "intr") {
				ressources["intr"]	= std::make_shared<tickRessource>((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt(), "cpu_interrupts");
				desc["intr"]		= "Interrupts (#/s)";
				ressources["intr"]->addProperty("value",   "Interrupts", "number");
			} else if (line.substr(0, 4) == "ctxt") {
				ressources["ctxt"]	= std::make_shared<tickRessource>((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt(), "cpu_contexts");
				desc["ctxt"]		= "Context switches (#/s)";
				ressources["ctxt"]->addProperty("value",   "Context switches", "number");
			}
		}
		addGetMetricRoute();
		morrisType="Area";morrisOpts="  ";
		if (infile.good())
			infile.close();
	}

	void collect() {
		std::string	line;
		std::string	values;
		std::string	id = "";
		std::ifstream	infile("/proc/stat");
		std::shared_ptr<cpuTicks>	res;
		std::shared_ptr<tickRessource>	tres;
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, 3) == "cpu") {
				std::istringstream iss(line);
				std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
				if (line.substr(0, 4) == "cpu ")	id = "all";
				else					id = line.substr(3, line.find(" ")-3);
				ressources[id]->nextValue();
				res = reinterpret_cast<std::shared_ptr<cpuTicks>&>(ressources[id]);
				res->setRaw(tokens);
			} else if (line.substr(0, 4) == "intr") {
				tres = reinterpret_cast<std::shared_ptr<tickRessource>&>(ressources["intr"]);
				tres->nextValue();
				tres->setTickValue("value", atoi(line.substr(5, line.find(" ",5)-5).c_str()));
			} else if (line.substr(0, 4) == "ctxt") {
				tres = reinterpret_cast<std::shared_ptr<tickRessource>&>(ressources["ctxt"]);
				tres->nextValue();
				tres->setTickValue("value", atoi(line.substr(5, line.find(" ",5)-5).c_str()));
			}
		}
		if (infile.good())
			infile.close();
	}
};



MAKE_PLUGIN_COLLECTOR(CpuUseCollector, cpuuse)

}
