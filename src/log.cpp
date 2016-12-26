#include "agent.h"
#include "config.h"

#include <list>
#include <fstream>
#include <ctime>
#include <iomanip>


namespace watcheD {

log::log(Json::Value* p_cfg) : cfg(p_cfg) {
	bool found = false;
	std::string lvl;
	if(! cfg->isMember("file") ) {
		(*cfg)["file"] = WATCHED_LOG_FILE;
		(*cfg)["file"].setComment(std::string("/*\t\tthe log file*/"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("level") ) {
		(*cfg)["level"] = "WARN";
		(*cfg)["level"].setComment(std::string("/*\t\tlevel to log at (NONE,ERROR,WARN,INFO,NOTICE,DEBUG,ALL)*/"), Json::commentAfterOnSameLine);
	}
	lvl = (*cfg)["level"].asString();
	std::transform(lvl.begin(), lvl.end(), lvl.begin(),  [](unsigned char c) { return std::toupper(c); });

	for(uint16_t i = 0;i<levels.size();i++) {
		if (levels[i] == lvl) {
			found = true;
			level = i;
		}
	}
	if(!found) level=3;
	(*cfg)["level"] = levels[level];
}

void log::write(uint16_t p_lvl, std::string p_message) {
	if (p_lvl>level) return;
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer,80,"%d-%m-%Y %I:%M:%S",timeinfo);

	std::ofstream outfile;
	std::unique_lock<std::mutex> locker(mutex); 
	outfile.open((*cfg)["file"].asString(), std::ios_base::app);
	if (outfile.is_open())
		outfile << "[ " << buffer << " - " << std::setw(6) << levels[p_lvl] << " ] "<< p_message << std::endl;
	else
		std::cout << "[ " << buffer << " - " << std::setw(6) << levels[p_lvl] << " ] "<< p_message << std::endl;
}

void log::write(std::string p_lvl, std::string p_message) {
	std::string lvl = p_lvl;
	uint16_t l=3;
	std::transform(lvl.begin(), lvl.end(), lvl.begin(),  [](unsigned char c) { return std::toupper(c); });
	for(uint16_t i = 0;i<levels.size();i++) {
		if (levels[i] == lvl) {
			l = i;
		}
	}
	write(l,p_message);
}

void log::error(std::string p_message) {write(1,p_message); }
void log::warning(std::string p_message) {write(2,p_message); }
void log::info(std::string p_message) {write(3,p_message); }
void log::notice(std::string p_message) {write(4,p_message); }
void log::debug(std::string p_message) {write(5,p_message); }

const std::vector<std::string> log :: levels ({"NONE","ERROR","WARN","INFO","NOTICE","DEBUG","ALL"});


}
