#include "collectors.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <chrono>

using namespace watcheD;
namespace collectors {

class CpuSpeedCollector : public Collector {
public:
	CpuSpeedCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("cpuspeed", p_srv, p_cfg) {
		/*std::string	line;
		std::string	id;
		std::ifstream	infile("/proc/cpuinfo");
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, line.find("\t")) == "processor") {
				id = line.substr(line.find(" ")+1,line.length());
				addRessource(id, "CPU "+id+" speed (Mhz)", "cpuspeed");
				ressources[id]->addProperty("MHz", "CPU speed (Mhz)", "number");
			}
		}
		if (infile.good())
			infile.close();*/
		addGetMetricRoute();
	}

	void collect() {
		const std::string directory = "/sys/devices/system/cpu/cpufreq";
		DIR *dir;
		struct dirent *ent;
		struct stat st;
		dir = opendir(directory.c_str());
		if (dir != NULL) {
			while ((ent = readdir(dir)) != NULL) {
				const std::string file_name = ent->d_name;
				const std::string full_dir_name = directory + "/" + file_name;
				const std::string full_file_name = full_dir_name+"/scaling_cur_freq";

				int cpuid = -1;
				sscanf( file_name.c_str(), "%*[^0-9]%d", &cpuid);
				if (cpuid == -1)
					continue;

				if (file_name[0] == '.')
					continue;
				if (stat(full_dir_name.c_str(), &st) == -1)
					continue;
				const bool is_directory = (st.st_mode & S_IFDIR) != 0;
				if (!is_directory)
					continue;
				if (stat(full_file_name.c_str(), &st) == -1)
					continue;
				std::ifstream t(full_file_name.c_str());
				std::string str;

				// reading cpu speed value
				t.seekg(0, std::ios::end);   
				str.reserve(t.tellg());
				t.seekg(0, std::ios::beg);
				str.assign((std::istreambuf_iterator<char>(t)),
					std::istreambuf_iterator<char>());
				if (ressources.find(file_name) == ressources.end()) {
					addRessource(file_name, "CPU "+std::to_string(cpuid)+" speed (Mhz)", "cpuspeed");
					ressources[file_name]->addProperty("MHz", "CPU speed (Mhz)", "number");
				}
				ressources[file_name]->nextValue();
				ressources[file_name]->setProperty("MHz", atoi(str.c_str())/1000);

				
			}
			closedir(dir);
		}
		/*std::string		line;
		std::string		value;
		std::string		id = "";
		std::ifstream	infile("/proc/cpuinfo");
		while(infile.good() && getline(infile, line)) {
			if (line.substr(0, line.find("\t")) == "processor")
				id = line.substr(line.find(" ")+1,line.length());
			else if (line.substr(0, line.find("\t")) == "cpu MHz" && id != "") {
				value = line.substr(line.find(" ",6)+1,line.length());
				ressources[id]->nextValue();
				ressources[id]->setProperty("MHz", atoi(value.c_str()));
			}
		}
		if (infile.good())
			infile.close();
		*/
	}
};

MAKE_PLUGIN_COLLECTOR(CpuSpeedCollector, cpuspeed)

}
