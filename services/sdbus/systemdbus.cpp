#include "services.h"
using namespace watcheD;

// might use this version once libsystemd actually stop leaking memory
#include <systemd/sd-bus.h>
static uint32_t getPID(sd_bus *bus, std::string path) {
	const char *contents = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	char type;
	union {
		uint8_t u8;
		uint16_t u16;
		int16_t s16;
		uint32_t u32;
		int32_t s32;
		uint64_t u64;
		int64_t s64;
		double d64;
		const char *string;
		int i;
	} basic;

	int r = sd_bus_call_method(bus,
		"org.freedesktop.systemd1",            /* service to contact */
		path.c_str(),      /* object path */
		"org.freedesktop.DBus.Properties", "Get",
		&error, &reply, "ss", "org.freedesktop.systemd1.Service", "ExecMainPID");
	if (r < 0) {
		std::cerr << "Failed to issue method call: " << error.message << std::endl;
		sd_bus_error_free(&error);
		sd_bus_message_unref(reply);
		return 0;
	}
	sd_bus_error_free(&error);
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return 0;
	}
	r = sd_bus_message_enter_container(reply, 'v', contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_enter_container: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return 0;
	}
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		sd_bus_message_unref(reply);
		std::cerr << "Failed to sd_bus_mesSage_peek_type: " << strerror(-r)  << std::endl;
		return 0;
	}
	r = sd_bus_message_read_basic(reply, type, &basic);
	if (r < 0) {
		sd_bus_message_unref(reply);
		std::cerr << "Failed to sd_bus_message_read_basic: " << strerror(-r)  << std::endl;
		return 0;
	}
	r = sd_bus_message_exit_container(reply);
	if (r < 0) {
		sd_bus_message_unref(reply);
		std::cerr << "Failed to sd_bus_message_exit_container: " << strerror(-r)  << std::endl;
		return 0;
	}
	sd_bus_message_unref(reply);

	return basic.u32;
}

static std::string getStatus(sd_bus *bus, std::string path) {
	const char *contents = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	char type;
	union {
		uint8_t u8;
		uint16_t u16;
		int16_t s16;
		uint32_t u32;
		int32_t s32;
		uint64_t u64;
		int64_t s64;
		double d64;
		const char *string;
		int i;
	} basic;
	std::string ret = "";

	int r = sd_bus_call_method(bus,
		"org.freedesktop.systemd1",            /* service to contact */
		path.c_str(),      /* object path */
		"org.freedesktop.DBus.Properties", "Get",
		&error, &reply, "ss", "org.freedesktop.systemd1.Unit", "ActiveState");
	if (r < 0) {
		std::cerr << "Failed to issue method call: " << error.message << std::endl;
		sd_bus_error_free(&error);
		sd_bus_message_unref(reply);
		return ret;
	}
	sd_bus_error_free(&error);
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_enter_container(reply, 'v', contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_enter_container: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_read_basic(reply, type, &basic);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_read_basic: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_exit_container(reply);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_exit_container: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	ret = basic.string;
	sd_bus_message_unref(reply);
	return ret;
}

static std::string getSubStatus(sd_bus *bus, std::string path) {
	const char *contents = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	char type;
	union {
		uint8_t u8;
		uint16_t u16;
		int16_t s16;
		uint32_t u32;
		int32_t s32;
		uint64_t u64;
		int64_t s64;
		double d64;
		const char *string;
		int i;
	} basic;
	std::string ret = "";

	int r = sd_bus_call_method(bus,
		"org.freedesktop.systemd1",            /* service to contact */
		path.c_str(),      /* object path */
		"org.freedesktop.DBus.Properties", "Get",
		&error, &reply, "ss", "org.freedesktop.systemd1.Unit", "SubState");
	if (r < 0) {
		std::cerr << "Failed to issue method call: " << error.message << std::endl;
		sd_bus_error_free(&error);
		sd_bus_message_unref(reply);
		return ret;
	}
	sd_bus_error_free(&error);
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_enter_container(reply, 'v', contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_enter_container: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_read_basic(reply, type, &basic);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_read_basic: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	r = sd_bus_message_exit_container(reply);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_exit_container: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return ret;
	}
	ret = basic.string;
	sd_bus_message_unref(reply);
	return ret;
}


static sd_bus *bus = NULL;
static bool have_bus = false;

/*********************************
 * Handler
 */

class systemdHandler: public serviceHandler {
public:
	systemdHandler(std::shared_ptr<service> p_s):serviceHandler(p_s) {}
	bool isBlackout() { 
	/*	int r = sd_bus_open_system(&bus);
		if (r < 0) {
			std::cerr << "Failed to connect to system bus: " << strerror(-r) << std::endl;
			sd_bus_unref(bus);
			return false;
		}*/
		if (!have_bus) return false;
		std::string res = getStatus(bus, id);
		//sd_bus_unref(bus);
		if (res == "active") { // restarted not yet known
			// TODO: update mainprocess
			std::shared_ptr<service> s = serv.lock();
			if(!s) return true;
			uint32_t PID = getPID(bus, id);
			if (PID==0 || s->havePID(PID))
				return true;
			std::shared_ptr<process> p = std::make_shared<process>(PID);
			if (p->getName() == "")
				return true; // probably running non-root
			s->updateMainProcess(p);
			return true;
		}
		return res == "inactive";
	}
	void setID(std::string p_id) { id = p_id; }
private:
	std::string id;
};
MAKE_PLUGIN_HANDLER(systemdHandler,systemd)

/*********************************
 * Detector
 */

class systemdDetector: public serviceDetector {
public:
	systemdDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server):serviceDetector(p_sm, p_server) {
		have_bus = false;
		int r = sd_bus_open_system(&bus);
		if (r < 0) {
			std::string me = "Failed to connect to system bus: ";
			me+=strerror(-r);me+=". disabling systemD integration.";
			server->logError("systemdDetector::", me);
		} else
			have_bus = true;
	}
	~systemdDetector() {
		have_bus = false;
		sd_bus_flush_close_unref(bus);
	}
	void find() {
		sd_bus_error error = SD_BUS_ERROR_NULL;
		sd_bus_message *m = NULL;
		if (!have_bus) return;
		//sd_bus *bus = NULL;
		const char *path;
		/*int r = sd_bus_open_system(&bus);
		if (r < 0) {
			std::cerr << "Failed to connect to system bus: " << strerror(-r) << std::endl;
			sd_bus_unref(bus);
			return;
		}*/

		int r = sd_bus_call_method(bus,
			"org.freedesktop.systemd1",            /* service to contact */
			"/org/freedesktop/systemd1/unit",      /* object path */
			"org.freedesktop.DBus.Introspectable", /* interface name */
			"Introspect",                          /* method name */
			&error, &m, "");
		if (r < 0) {
			std::string me = "Failed to issue method call: ";me+=error.message;
			server->logWarning("systemdDetector::find", me);
			sd_bus_error_free(&error);
			sd_bus_message_unref(m);
			//sd_bus_unref(bus);
			return;
		}
		sd_bus_error_free(&error);

		/* Parse the response message */
		r = sd_bus_message_read(m, "s", &path);
		if (r < 0) {
			std::string me = "Failed to parse response message: ";me+=strerror(-r);
			server->logWarning("systemdDetector::find", me);
			sd_bus_message_unref(m);
			//sd_bus_unref(bus);
			return;
		}

		std::stringstream ss(path);
		std::string line;
		std::shared_ptr<servicesManager> mgr = services.lock();
		uint16_t count = 0;
		uint16_t coun2 = 0;
		sd_bus_message_unref(m);
		if (!mgr) return;
		while (std::getline(ss, line)) {
			if (line.substr(1,5) == "<node" && line.find("service") != std::string::npos) {
				std::string small = line.substr(line.find("\"")+1,line.rfind("\"")-line.find("\"")-1);
				std::string name = "/org/freedesktop/systemd1/unit/"+small;
				uint32_t PID = getPID(bus, name);
				if (PID == 0)
					continue;	// this service have no PID
				std::string status = getSubStatus(bus, name);
				if (status == "exited" || status == "dead" || status == "")
					continue; // ignore exited service

				if (mgr->havePID(PID)) {
					std::shared_ptr<service> serv = mgr->getService(PID);
					if(!serv) continue;
					if (serv->haveHandler()) continue;
					std::shared_ptr<systemdHandler> hand = std::make_shared<systemdHandler>(serv);
					hand->setID(name);
					serv->setHandlerObj(hand);
					coun2++;
				} else {
					std::shared_ptr<process> p = std::make_shared<process>(PID);
					if (p->getName() == "")
						continue; // probably running non-root
					std::shared_ptr<service> serv = std::make_shared<service>(server);
					std::shared_ptr<systemdHandler> hand = std::make_shared<systemdHandler>(serv);
					hand->setID(name);
					serv->setUniqKey(small);
					serv->addMainProcess(p);
					serv->setHandlerObj(hand);
					mgr->addService(serv);
					count++;
				}

			}
		}
		server->logInfo("systemdDetector::find", "found "+std::to_string(count)+" services. updated "+std::to_string(coun2));
	}
/*private:
	sd_bus *bus = NULL;*/
};
MAKE_PLUGIN_DETECTOR(systemdDetector, systemd)

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
