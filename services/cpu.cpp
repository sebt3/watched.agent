#include "services.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace watcheD;
namespace services {

// This is exactly the same as in the cpu.lua file. This is here for memory usage difference only
class ServCpuCollector : public Collector {
public:
	ServCpuCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg, std::shared_ptr<service> p_serv): Collector("cpu", p_srv, p_cfg, 150, 10, p_serv) {
		addRessource("shed", "Service scheduler information", "service_cpu_shed");
		ressources["shed"]->addProperty("exec_runtime", "exec_runtime stat from the sheduler",	"number");
		ressources["shed"]->addProperty("process",	"Number of process",				"number");
		ressources["shed"]->addProperty("thread",	"total number of threads",			"number");
		addRessource("stat", "Service CPU usage", "service_cpu_stat");
		ressources["stat"]->addProperty("user",		"user %",		"number");
		ressources["stat"]->addProperty("system",	"system %",		"number");
		ressources["stat"]->addProperty("pct",		"total % used",		"number");
		ressources["stat"]->addProperty("cuser",	"user % cumulated",	"number");
		ressources["stat"]->addProperty("csystem",	"system % cumulated",	"number");
		addGetMetricRoute();
		prev_total_time=0;
	}

	void collect() {
		std::shared_ptr<service> serv_p = onService.lock();
		if (!serv_p) return;
		double sum		= 0;
		uint32_t process	= 0;
		uint32_t thread		= 0;
		uint32_t total_time	= 0;
		uint32_t cpu_count	= 0;
		uint32_t res_users	= 0;
		uint32_t res_sys	= 0;
		uint32_t res_cusers	= 0;
		uint32_t res_csys	= 0;
		std::string line;

		std::ifstream	f("/proc/stat");
		if (!f.good()) return;
		while(std::getline(f, line)) {
			std::istringstream iss(line);
			std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
			if (tokens[0] == "cpu") {
				for(uint i=1;i<tokens.size();i++)
					total_time += atoi(tokens[i].c_str());
			} else if (tokens[0].substr(0,3) == "cpu")
				cpu_count++;
			
		}
		if (f.good()) f.close();
		if (cpu_count==0) cpu_count=1;

		std::shared_ptr< std::vector<uint32_t> > pids = serv_p->getPIDs();
		for (std::vector<uint32_t>::iterator i=pids->begin();i!=pids->end();i++) {
			// collect the shed information
			std::ifstream	infile("/proc/"+std::to_string(*i)+"/sched");
			if (!infile.good()) continue;
			while(std::getline(infile, line)) {
				std::istringstream iss(line);
				std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
				if (tokens[0] == "se.sum_exec_runtime" ) {
					double v = stod(tokens[2]);
					if (values.find(*i)!=values.end() && values[*i]<v)
						sum += v - values[*i];
					values[*i] = v;
				} else if (tokens.size()>2 && tokens[2] == "#threads:")
					thread+=atoi(tokens[3].substr(0,tokens[3].length()-1).c_str());
			}
			infile.close();

			// collect cpu used data
			std::ifstream	statfile("/proc/"+std::to_string(*i)+"/stat");
			if (!statfile.good()) continue;
			if(std::getline(statfile, line)) { // there's should be only one line but...
				std::istringstream iss(line);
				std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

				if (tokens.size() < 17) continue;
				uint32_t v = atoi(tokens[13].c_str());
				if (users.find(*i)!=users.end() && users[*i]<v)
					res_users += v - users[*i];
				users[*i] = v;
				v = atoi(tokens[14].c_str());
				if (sys.find(*i)!=sys.end() && sys[*i]<v)
					res_sys += v - sys[*i];
				sys[*i] = v;
				v = atoi(tokens[15].c_str());
				if (cusers.find(*i)!=cusers.end() && cusers[*i]<v)
					res_cusers += v - cusers[*i];
				cusers[*i] = v;
				v = atoi(tokens[16].c_str());
				if (csys.find(*i)!=csys.end() && csys[*i]<v)
					res_csys += v - csys[*i];
				csys[*i] = v;
			}
			statfile.close();
			process++;
		}
		if (prev_total_time!=0) {
			ressources["shed"]->nextValue();
			ressources["shed"]->setProperty("exec_runtime",	sum);
			ressources["shed"]->setProperty("process",	process);
			ressources["shed"]->setProperty("thread",	thread);
			double p = 100*cpu_count;p = p/(total_time-prev_total_time);
			ressources["stat"]->nextValue();
			ressources["stat"]->setProperty("user",		res_users*p);
			ressources["stat"]->setProperty("system",	res_sys*p);
			ressources["stat"]->setProperty("pct",		(res_sys+res_users)*p);
			ressources["stat"]->setProperty("cuser",	res_cusers*p);
			ressources["stat"]->setProperty("csystem",	res_csys*p);
		}
		prev_total_time = total_time;
	}
private:
	std::map<uint64_t, double> values;
	std::map<uint64_t, uint32_t> users;
	std::map<uint64_t, uint32_t> sys;
	std::map<uint64_t, uint32_t> cusers;
	std::map<uint64_t, uint32_t> csys;
	uint64_t prev_total_time;
};

MAKE_PLUGIN_SERVICE_COLLECTOR(ServCpuCollector, cpu)

}
