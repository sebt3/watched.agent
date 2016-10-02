#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>


using namespace std;
using namespace watcheD;

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
	void setRaw(vector<string> raw) {
		uint sum;
		string id;
		
		live["user"]	=atoi(raw[1].c_str());
		live["nice"]	=atoi(raw[2].c_str());
		live["system"]	=atoi(raw[3].c_str());
		live["idle"]	=atoi(raw[4].c_str());
		live["iowait"]	=atoi(raw[5].c_str());
		live["irq"]	=atoi(raw[6].c_str());
		live["softirq"]	=atoi(raw[7].c_str());
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
	CpuUseCollector(HttpServer* p_srv, Json::Value* p_cfg) : Collector("cpuuse", p_srv, p_cfg) {
		string		line;
		string		id;
		ifstream	infile("/proc/stat");
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, 3) == "cpu") {
				if (line.substr(0, 4) == "cpu ")	id = "all";
				else					id = line.substr(3, line.find(" ")-3);
				ressources[id]	= new cpuTicks((*cfg)["history"].asUInt());
				desc[id]	= "CPU "+id+" usage";
			} else if (line.substr(0, 4) == "intr") {
				ressources["intr"]	= new tickRessource((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt(), "cpu_interrupts");
				desc["intr"]		= "Interrupts (#/s)";
				ressources["intr"]->addProperty("value",   "Interrupts", "number");
			} else if (line.substr(0, 4) == "ctxt") {
				ressources["ctxt"]	= new tickRessource((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt(), "cpu_contexts");
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
		string		line;
		string		values;
		string		id = "";
		ifstream	infile("/proc/stat");
		cpuTicks*	res;
		tickRessource*	tres;
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, 3) == "cpu") {
				istringstream iss(line);
				vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
				if (line.substr(0, 4) == "cpu ")	id = "all";
				else					id = line.substr(3, line.find(" ")-3);
				ressources[id]->nextValue();
				res = reinterpret_cast<cpuTicks*>(ressources[id]);
				res->setRaw(tokens);
			} else if (line.substr(0, 4) == "intr") {
				tres = reinterpret_cast<tickRessource*>(ressources["intr"]);
				tres->nextValue();
				tres->setTickValue("value", atoi(line.substr(5, line.find(" ",5)-5).c_str()));
			} else if (line.substr(0, 4) == "ctxt") {
				tres = reinterpret_cast<tickRessource*>(ressources["ctxt"]);
				tres->nextValue();
				tres->setTickValue("value", atoi(line.substr(5, line.find(" ",5)-5).c_str()));
			}
		}
		if (infile.good())
			infile.close();
	}
};


MAKE_PLUGIN_COLLECTOR(CpuUseCollector, "cpuuse")
