#include "services.h"

using namespace watcheD;

/* TODO:
 * Use D-Bus to query system service list and status (aptitude install libdbus-c++-dev et voir http://blog.emilienkia.net/post/2012/11/07/Journey-into-the-world-of-DBus-for-C-(part-2)-attempt-around-libdbus-c)
 * for s in $(qdbus --system org.freedesktop.systemd1|grep service);do R=$(qdbus --system org.freedesktop.systemd1 $s org.freedesktop.systemd1.Unit.SubState);if [[ "$R" != "exited" ]] && [[ "$R" != "dead" ]];then echo $R $s;fi;done
 * qdbus --system org.freedesktop.systemd1 /org/freedesktop/systemd1/unit/watched_2eback_2eservice org.freedesktop.systemd1.Service.MainPID
 * qdbus --system org.freedesktop.systemd1 /org/freedesktop/systemd1/unit/watched_2eback_2eservice org.freedesktop.systemd1.Unit.ActiveState (active/inactive/failed)
 */

class systemdDetector: public serviceDetector {
public:
	systemdDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server):serviceDetector(p_sm, p_server) {}
	void find() {
		//std::cout << "systemdDetector::find()\n";
	}
};
MAKE_PLUGIN_DETECTOR(systemdDetector, systemd)


class systemdHandler: public serviceHandler {
public:
	systemdHandler(std::shared_ptr<service> p_s):serviceHandler(p_s) {}
	bool status() { return true; }
	bool isBlackout() { return false; }
};
MAKE_PLUGIN_HANDLER(systemdHandler,systemd)

/*
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
*/
