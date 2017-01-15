#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>

using namespace watcheD;

class diskStatsRess : public tickRessource {
public:
	diskStatsRess(uint p_size, uint p_freq) : tickRessource(p_size, p_freq, "disk_stats") {
		addProperty("rcount",  "Read count/s", "number");
		addProperty("rmerge",  "Read merge/s", "number");
		addProperty("rsector", "Sectors read/s", "number");
		addProperty("rtime",   "Read time ms/s", "number");
		addProperty("wcount",  "Write count/s", "number");
		addProperty("wmerge",  "Write merge/s", "number");
		addProperty("wsector", "Sectors write/s", "number");
		addProperty("wtime",   "Write time ms/s", "number");
		addProperty("iotime",  "IO time ms/s", "number");
		addProperty("iowtime", "Weighted IO time ms/s", "number");
	}
	void setRaw(std::vector<std::string> raw) {
		setTickValue("rcount",	atoi(raw[3].c_str()));
		setTickValue("rmerge",	atoi(raw[4].c_str()));
		setTickValue("rsector",	atoi(raw[5].c_str()));
		setTickValue("rtime",	atoi(raw[6].c_str()));
		setTickValue("wcount",	atoi(raw[7].c_str()));
		setTickValue("wmerge",	atoi(raw[8].c_str()));
		setTickValue("wsector",	atoi(raw[9].c_str()));
		setTickValue("wtime",	atoi(raw[10].c_str()));
		setTickValue("iotime",	atoi(raw[12].c_str()));
		setTickValue("iowtime",	atoi(raw[13].c_str()));
	}
};

class diskStatsCollector : public Collector {
public:
	diskStatsCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("diskstats", p_srv, p_cfg) {
		addGetMetricRoute();
	}

	void collect() {
		std::string	line;
		std::string	values;
		std::ifstream	infile("/proc/diskstats");
		std::shared_ptr<diskStatsRess>	res;
		while(infile.good() && getline(infile, line)) {
			std::istringstream iss(line);
			std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
			if(tokens[2].substr(0,3) == "dm-") continue;
			if (ressources.find(tokens[2]) == ressources.end()) {
				res = std::make_shared<diskStatsRess>((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt());
				ressources[tokens[2]] = res;
				desc[tokens[2]] = "Disk "+tokens[2]+" statistics";
			}else
				res = reinterpret_cast<std::shared_ptr<diskStatsRess>&>(ressources[tokens[2]]);

			ressources[tokens[2]]->nextValue();
			res->setRaw(tokens);
		}
		if (infile.good())
			infile.close();
	}
};


MAKE_PLUGIN_COLLECTOR(diskStatsCollector, diskstats)
