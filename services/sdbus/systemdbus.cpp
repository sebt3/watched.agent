#include "services.h"
using namespace watcheD;

#include <systemd/sd-bus.h>
namespace services {

#define MAKE_DBUS_READ(ETYPE, RTYPE, FNAME, IFACE, PROP)						\
static ETYPE FNAME(sd_bus *bus, std::string path) {							\
	const char *contents = NULL;									\
	sd_bus_error error = SD_BUS_ERROR_NULL;								\
	sd_bus_message *reply = NULL;									\
	char type;											\
	union {												\
		uint8_t u8;										\
		uint16_t u16;										\
		int16_t s16;										\
		uint32_t u32;										\
		int32_t s32;										\
		uint64_t u64;										\
		int64_t s64;										\
		double d64;										\
		const char *string;									\
		int i;											\
	} basic;											\
													\
	int r = sd_bus_call_method(bus,									\
		"org.freedesktop.systemd1",								\
		path.c_str(),										\
		"org.freedesktop.DBus.Properties", "Get",						\
		&error, &reply, "ss", IFACE, PROP);							\
	if (r < 0) {											\
		if(0==1) std::cerr << "Failed to issue method call: " << error.message << std::endl;	\
		sd_bus_error_free(&error);								\
		sd_bus_message_unref(reply);								\
		return 0;										\
	}												\
	sd_bus_error_free(&error);									\
	r = sd_bus_message_peek_type(reply, &type, &contents);						\
	if (r < 0) {											\
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;	\
		sd_bus_message_unref(reply);								\
		return 0;										\
	}												\
	r = sd_bus_message_enter_container(reply, 'v', contents);					\
	if (r < 0) {											\
		std::cerr << "Failed to sd_bus_message_enter_container: " << strerror(-r)  << std::endl; \
		sd_bus_message_unref(reply);								\
		return 0;										\
	}												\
	r = sd_bus_message_peek_type(reply, &type, &contents);						\
	if (r < 0) {											\
		sd_bus_message_unref(reply);								\
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;	\
		return 0;										\
	}												\
	r = sd_bus_message_read_basic(reply, type, &basic);						\
	if (r < 0) {											\
		sd_bus_message_unref(reply);								\
		std::cerr << "Failed to sd_bus_message_read_basic: " << strerror(-r)  << std::endl;	\
		return 0;										\
	}												\
	r = sd_bus_message_exit_container(reply);							\
	if (r < 0) {											\
		sd_bus_message_unref(reply);								\
		std::cerr << "Failed to sd_bus_message_exit_container: " << strerror(-r)  << std::endl;	\
		return 0;										\
	}												\
	sd_bus_message_unref(reply);									\
													\
	return basic.RTYPE;										\
}

/* see :
 * intro() { gdbus introspect --system --dest org.freedesktop.systemd1 --object-path /org/freedesktop/systemd1/unit/$1|awk '$1=="readonly"||$1=="interface"'; }
 * intro cups_2eservice|more
 */

MAKE_DBUS_READ(uint32_t, u32, getPID, "org.freedesktop.systemd1.Service", "ExecMainPID")
MAKE_DBUS_READ(std::string, string, getStatus, "org.freedesktop.systemd1.Unit", "ActiveState")
MAKE_DBUS_READ(std::string, string, getSubStatus, "org.freedesktop.systemd1.Unit", "SubState")
MAKE_DBUS_READ(std::string, string, getType, "org.freedesktop.systemd1.Service", "Type")
MAKE_DBUS_READ(std::string, string, getRestart, "org.freedesktop.systemd1.Service", "Restart")



static std::string findByPID(sd_bus *bus, uint32_t p_pid) {
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
		"org.freedesktop.systemd1",
		"/org/freedesktop/systemd1",
		"org.freedesktop.systemd1.Manager", "GetUnitByPID",
		&error, &reply, "u", p_pid);
	if (r < 0) {
		std::cerr << "Failed to issue method call: " << error.message << std::endl;
		sd_bus_error_free(&error);
		sd_bus_message_unref(reply);
		return "";
	}
	sd_bus_error_free(&error);
	r = sd_bus_message_peek_type(reply, &type, &contents);
	if (r < 0) {
		std::cerr << "Failed to sd_bus_message_peek_type: " << strerror(-r)  << std::endl;
		sd_bus_message_unref(reply);
		return "";
	}
	r = sd_bus_message_read_basic(reply, type, &basic);
	if (r < 0) {
		sd_bus_message_unref(reply);
		std::cerr << "Failed to sd_bus_message_read_basic: " << strerror(-r)  << std::endl;
		return "";
	}
	std::string res = basic.string;
	sd_bus_message_unref(reply);
	res = res.substr(res.rfind('/')+1);
	return res;
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
			// update mainprocess
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
	systemdDetector(std::shared_ptr<servicesManager> p_sm, std::shared_ptr<HttpServer> p_server, Json::Value* p_cfg):serviceDetector(p_sm, p_server, p_cfg) {
		Json::Value	arr_value(Json::arrayValue);
		if (! cfg->isMember("whiteList")) {
			(*cfg)["whiteList"]	= arr_value;
			(*cfg)["whiteList"].setComment(std::string("/*\tConfigure the services that could be detected */"), Json::commentBefore);
			(*cfg)["whiteList"][0]["unit-name"]	= "rsyslog_2eservice";
			(*cfg)["whiteList"][1]["unit-name"]	= "watched_2eback_2eservice";
			(*cfg)["whiteList"][2]["type"]		= "forking";
			(*cfg)["whiteList"][3]["type"]		= "simple";
			(*cfg)["whiteList"][4]["type"]		= "dbus";
			(*cfg)["whiteList"][5]["type"]		= "notify";
			(*cfg)["whiteList"][5]["restart"]	= "on-failure";
		}
		if (! cfg->isMember("blackList")) {
			(*cfg)["blackList"]	= arr_value;
			(*cfg)["blackList"].setComment(std::string("/*\tConfigure the services that should not be detected */"), Json::commentBefore);
			(*cfg)["blackList"][0]["type"]		= "dbus";
			(*cfg)["blackList"][0]["restart"]	= "no";
			(*cfg)["blackList"][1]["unit-name"]	= "systemd_2d.*_2eservice";
		}
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

protected:
	bool matchList(const std::string p_small, Json::Value* p_list) {
		std::string name = "/org/freedesktop/systemd1/unit/"+p_small;
		std::string s_type	= getType(bus, name);
		std::string s_restart	= getRestart(bus, name);
		for (Json::Value::iterator j = p_list->begin();j!=p_list->end();j++) {
			bool lineMatched = false;
			if (j->isMember("unit-name")) {
				std::regex base_regex((*j)["unit-name"].asString());
				std::smatch base_match;
				if (std::regex_match(p_small, base_match, base_regex))
					lineMatched = true;
				else
					lineMatched = false;
			}
			if (j->isMember("type")) {
				if (s_type == (*j)["type"].asString())
					lineMatched = true;
				else
					lineMatched = false;
			}
			if (j->isMember("restart")) {
				if (s_restart == (*j)["restart"].asString())
					lineMatched = true;
				else
					lineMatched = false;
			}
			if (lineMatched)
				return true;
		}

		return false;
	}
public:
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

		// see : echo -e $(gdbus call -y -d org.freedesktop.systemd1 -o /org/freedesktop/systemd1/unit -m org.freedesktop.DBus.Introspectable.Introspect)|grep " <node "|grep service
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
				if (!matchList(small, &((*cfg)["whiteList"])) || matchList(small, &((*cfg)["blackList"])))
					continue;

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


class systemdEnhancer: public serviceEnhancer {
public:
	systemdEnhancer(std::shared_ptr<servicesManager> p_sm): serviceEnhancer(p_sm) {}
	std::shared_ptr<service> enhance(std::shared_ptr<service> p_serv) {
		std::string name;
		if (!have_bus) return nullptr;
		std::shared_ptr<process> p = p_serv->getMainProcess();
		if (p!=nullptr  && ! p_serv->haveHandler()) {
			name = findByPID(bus,p->getPID());
			if (name.length()>7 && name.substr(name.length()-7) == "service" ) {
				p_serv->setUniqKey(name);
				std::shared_ptr<systemdHandler> hand = std::make_shared<systemdHandler>(p_serv);
				hand->setID(name);
				p_serv->setHandlerObj(hand);
			}
			
		}
		return nullptr;
	}
};
MAKE_PLUGIN_ENHANCER(systemdEnhancer, systemd)


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


MAKE_PLUGIN_SERVICE(printService, print)
MAKE_PLUGIN_SERVICE(systemService, system)
*/

}
