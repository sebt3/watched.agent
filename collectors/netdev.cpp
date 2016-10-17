#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iterator>


using namespace std;
using namespace watcheD;

class netDevRess : public tickRessource {
public:
	netDevRess(uint p_size, uint p_freq) : tickRessource(p_size, p_freq, "network_stat") {
		addProperty("rbytes",   "Bytes/s (receive)", "number");
		addProperty("rpackets", "Packets/s (receive)", "number");
		addProperty("rerrs",    "Errors/s (receive)", "number");
		addProperty("rdrop",    "Drops/s (receive)", "number");
		addProperty("rmulti",   "Multicast/s (receive)", "number");
		addProperty("tbytes",   "Bytes/s (transmit)", "number");
		addProperty("tpackets", "Packets/s (transmit)", "number");
		addProperty("terrs",    "Errors/s (transmit)", "number");
		addProperty("tdrop",    "Drop/s (transmit)", "number");
	}
	void setRaw(vector<string> raw) {
		setTickValue("rbytes",	atoi(raw[0].c_str()));
		setTickValue("rpackets",atoi(raw[1].c_str()));
		setTickValue("rerrs",	atoi(raw[2].c_str()));
		setTickValue("rdrop",	atoi(raw[3].c_str()));
		setTickValue("rmulti",	atoi(raw[7].c_str()));
		setTickValue("tbytes",	atoi(raw[8].c_str()));
		setTickValue("tpackets",atoi(raw[9].c_str()));
		setTickValue("terrs",	atoi(raw[10].c_str()));
		setTickValue("tdrop",	atoi(raw[11].c_str()));
	}
};

class NetDevCollector : public Collector {
public:
	NetDevCollector(HttpServer* p_srv, Json::Value* p_cfg) : Collector("netdev", p_srv, p_cfg) {
		addGetMetricRoute();
	}

	void collect() {
		string		line;
		string		values;
		string		id = "";
		ifstream	infile("/proc/net/dev");
		netDevRess*	res;
		while(infile.good() && getline(infile, line)) {
			if (line.find(":") >= line.size()) continue;
			int endid = line.find(":");
			int start = line.rfind(" ",endid)+1;
			id = line.substr(start,endid-start);
			istringstream iss(line.substr(endid+1, line.length()));
			vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
			if (ressources.find(id) == ressources.end()) {
				res = new netDevRess((*cfg)["history"].asUInt(), (*cfg)["poll-frequency"].asUInt());;
				ressources[id]	= res;
				desc[id]	= "Network interface "+id+" usage";
			}else
				res = reinterpret_cast<netDevRess*>(ressources[id]);
			ressources[id]->nextValue();
			res->setRaw(tokens);
		}
		if (infile.good())
			infile.close();
	}
};


MAKE_PLUGIN_COLLECTOR(NetDevCollector, netdev)
