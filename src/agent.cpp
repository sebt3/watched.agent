#include "agent.h"
#include <fstream>
#include <chrono>
using namespace watcheD;

#include <openssl/sha.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>


const std::string SERVER_HEAD="watched.agent/0.1";
const std::string APPS_NAME="watched.agent";
const std::string APPS_DESC="Watch over wasted being washed up";
#include "config.h"

/*********************************
 * main
 */
int main(int argc, char *argv[]) {
	std::string cfgfile = WATCHED_CONFIG;
	if (argc>1) cfgfile = argv[1];
	std::shared_ptr<Config>		cfg	= std::make_shared<Config>(cfgfile);
	std::shared_ptr<HttpServer>	server	= std::make_shared<HttpServer>(cfg->getServer(), cfg->getLog());
	std::shared_ptr<servicesManager> sm	= std::make_shared<servicesManager>(server,cfg);
	sm->init();
	cfg->save();

	sm->startThreads();
	server->start();

	return 0;
}


