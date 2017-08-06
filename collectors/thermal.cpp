#include "collectors.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <chrono>

using namespace watcheD;
namespace collectors {

class ThermalCollector : public Collector {
public:
	ThermalCollector(std::shared_ptr<HttpServer> p_srv, Json::Value* p_cfg) : Collector("thermal", p_srv, p_cfg) {
		addGetMetricRoute();
		addRessource("all", "all thermal informations", "thermal");
	}
	
	std::string getContent(std::string path) {
		std::string str = "";
		struct stat st;
		if (stat(path.c_str(), &st) == -1)
			return str;

		std::ifstream t(path.c_str());
		t.seekg(0, std::ios::end);   
		str.reserve(t.tellg());
		t.seekg(0, std::ios::beg);
		str.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

		return str.substr(0,str.length()-1);
	}

	void collect() {
		const std::string directory = "/sys/class/thermal";
		DIR *dir;
		struct dirent *ent;
		struct stat st;
		dir = opendir(directory.c_str());
		if (dir != NULL) {
			ressources["all"]->nextValue();
			while ((ent = readdir(dir)) != NULL) {
				const std::string prefix = "thermal_zone";
				const std::string file_name = ent->d_name;
				const std::string full_dir_name = directory + "/" + file_name;

				if (! std::equal(prefix.begin(), prefix.end(), file_name.begin()))
					continue;

				if (file_name[0] == '.')
					continue;
				if (stat(full_dir_name.c_str(), &st) == -1)
					continue;
				const bool is_directory = (st.st_mode & S_IFDIR) != 0;
				if (!is_directory)
					continue;

				std::string value = getContent(full_dir_name+"/temp");
				std::string name = getContent(full_dir_name+"/type");
				std::string sub = getContent(full_dir_name+"/device/path");
				if (sub != "") {
					std::string::size_type pos = 0;
					if ((pos = sub.find('.', pos)) != std::string::npos)
						name += sub.substr(pos);
					else
						name += sub;
				}
				ressources["all"]->addProperty(name,name+" temperature","number");
				ressources["all"]->setProperty(name, (double)(atoi(value.c_str()))/1000);
			}
			closedir(dir);
		}
	}
};

MAKE_PLUGIN_COLLECTOR(ThermalCollector, thermal)

}
