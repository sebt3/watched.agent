#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace std;
using namespace watcheD;

class MemoryCollector : public Collector {
public:
	MemoryCollector(HttpServer* p_srv, Json::Value* p_cfg) : Collector("memory", p_srv, p_cfg, 150, 10) {
		string		line;
		string		id;
		ifstream	infile("/proc/meminfo");
		if (infile.good()) {
			infile.close();
			addRessource("swap", "Swap usage", "swap_usage");
			ressources["swap"]->addProperty("free", "Free swap (Kb)", "integer");
			ressources["swap"]->addProperty("used", "Used swap (Kb)", "integer");
			ressources["swap"]->addProperty("pct", "Free swap (%)", "number");
			addRessource("memory", "Memory usage", "memory_usage");
			ressources["memory"]->addProperty("free",   "Free memory (Kb)", "integer");
			ressources["memory"]->addProperty("buffer", "Buffer memory (kb)", "integer");
			ressources["memory"]->addProperty("apps",   "Application memory (Kb)", "integer");
			ressources["memory"]->addProperty("cached", "Cached memory (kb)", "integer");
			ressources["memory"]->addProperty("slab",   "Kernel memory (kb)", "integer");
			ressources["memory"]->addProperty("pct",   "Free memory (%)", "number");
			addGetMetricRoute();
		}
		morrisType="Area";morrisOpts="  ";
	}

	void collect() {
		string		line;
		uint32_t	value;
		string		id = "";
		ifstream	infile("/proc/meminfo");
		int		start=0;
		bool		found=false;
		uint32_t	MemTotal = 0;
		uint32_t	MemFree = 0;
		uint32_t	Slab = 0;
		uint32_t	Buffers = 0;
		uint32_t	Cached = 0;
		uint32_t	SwapTotal = 0;
		uint32_t	SwapFree = 0;
		bool ok= false;

		while(infile.good() && getline(infile, line)) {
			id = line.substr(0, line.find(":"));
			found=false;start=line.find(":")+1;
			for(uint i=start;!found && i<line.length();i++) {
				if (line[i] !=' ') {
					found=true;
					start=i;
				}
			}
			value = atoi(line.substr(start, line.find(" ",start+1)-start).c_str());
			if (id == "MemTotal")		MemTotal = value;
			else if (id == "MemFree")	MemFree = value;
			else if (id == "Buffers")	Buffers = value;
			else if (id == "Cached")	Cached = value;
			else if (id == "Slab")		Slab = value;
			else if (id == "SwapTotal")	SwapTotal = value;
			else if (id == "SwapFree")	SwapFree = value;
			ok=true;
		}
		if (ok) {
			infile.close();
			ressources["swap"]->nextValue();
			ressources["swap"]->setProperty("free",		SwapFree);
			ressources["swap"]->setProperty("used",		SwapTotal - SwapFree);
			ressources["swap"]->setProperty("pct", 		(double)(100*SwapFree)/SwapTotal);
			ressources["memory"]->nextValue();
			ressources["memory"]->setProperty("free",	MemFree);
			ressources["memory"]->setProperty("buffer",	Buffers);
			ressources["memory"]->setProperty("apps",	MemTotal - (MemFree + Cached + Slab + Buffers));
			ressources["memory"]->setProperty("cached",	Cached);
			ressources["memory"]->setProperty("slab",	Slab);
			ressources["memory"]->setProperty("pct",	(double)(100*MemFree)/MemTotal);
			
		}
		
	}
};

MAKE_PLUGIN_COLLECTOR(MemoryCollector, "memory")
