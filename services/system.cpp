#include "services.h"

using namespace watcheD;


//*
class dummyDetector: public serviceDetector {
public:
	dummyDetector(servicesManager *p_sm, HttpServer* p_server):serviceDetector(p_sm, p_server) {}
	void find() {
		std::cout << "dummyDetector::find()\n";
	}
};
MAKE_PLUGIN_DETECTOR(dummyDetector, dummy)
//*/

class systemService : public service {
public:
	systemService(const service& p_src) : service(p_src) {
		type = "system";
	}
};
class printService : public service {
public:
	printService(const service& p_src) : service(p_src) {
		type = "print";
	}
};

class systemEnhancer: public serviceEnhancer {
public:
	systemEnhancer(servicesManager *p_sm): serviceEnhancer(p_sm) {}
	service *enhance(service *p_serv) {
		service *ret	  = NULL;
		std::string name  = p_serv->getName();
		if(name == "rpcbind")
			ret = new systemService(*p_serv);
		else if(name == "dhclient")
			ret = new systemService(*p_serv);
		else if(name == "exim4")
			ret = new systemService(*p_serv);
		else if(name == "cups-browsed")
			ret = new printService(*p_serv);
		else if(name == "cupsd")
			ret = new printService(*p_serv);
		if (ret != NULL)
			delete p_serv;
		return ret;
		
	}
};

MAKE_PLUGIN_SERVICE(printService, print)
MAKE_PLUGIN_SERVICE(systemService, system)
MAKE_PLUGIN_ENHANCER(systemEnhancer, system)
