#include "services.h"

using namespace watcheD;


class dummyDetector: public serviceDetector {
public:
	dummyDetector(servicesManager *p_sm, HttpServer* p_server):serviceDetector(p_sm, p_server) {}
	void find() {
		std::cout << "dummyDetector::find()\n";
	}
};


/*
class systemService : public service {
public:
	systemService(HttpServer* p_srv) : service(p_srv) {
	}
};


MAKE_PLUGIN_SERVICE(systemService, "system")
*/

MAKE_PLUGIN_DETECTOR(dummyDetector, "dummy")
