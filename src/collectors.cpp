#include "collectors.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h> 

using namespace std;
using namespace watcheD;

void setResponse404(std::shared_ptr<HttpServer::Response> response, std::string content) {
	*response << "HTTP/1.1 404 Not found\r\nServer: watched.agent\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}
void setResponseHtml(std::shared_ptr<HttpServer::Response> response, std::string content) {
	*response << "HTTP/1.1 200 OK\r\nServer: watched.agent\r\nAccess-Control-Allow-Origin:*\r\nContent-Type:text/html; charset=UTF-8\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}
void setResponseJson(std::shared_ptr<HttpServer::Response> response, std::string content) {
	*response << "HTTP/1.1 200 OK\r\nServer: watched.agent\r\nContent-Type:application/json\r\nAccess-Control-Allow-Origin:*\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}


/*********************************
 * Ressource
 */
void Ressource::nextValue() {
	Json::Value data(Json::objectValue);
	chrono::duration<double, std::milli> fp_ms = chrono::system_clock::now().time_since_epoch();
	if (v.empty())
		v.insert(v.begin(), data);
	else
		v.insert(v.begin(), v[0]);
	v[0]["timestamp"] = fp_ms.count();
	while(v.size()>=size)
		v.pop_back();
}

string  Ressource::getHistory(double since) {
	stringstream ss;
	bool splitted = true;
	ss << "[\n";
	if (v.size()>1) for(uint i=v.size()-1;i>0;i--) {
		if (v[i].isMember("timestamp") && v[i]["timestamp"].asDouble() > since) {
			if (!splitted) {
				ss << ",\n";
			}
			ss << v[i];
			splitted=false;
		}
	}
	ss << "\n]\n";
	return ss.str();
}

string  Ressource::getMorrisDesc() {
	string keys = "ykeys: [";
	string desc = "labels: [";
	string sep  = "";
	map<string,string>::iterator b=d.begin();
	for(map<string,string>::iterator i = b;i != d.end();i++) {
		keys+= sep+"'"+i->first+"'";
		desc+= sep+"'"+i->second+"'";
		sep=",";
	}
	return keys+"], "+desc+"]";
}

void Ressource::getDefinition(Json::Value* p_defs) {
	(*p_defs)["type"] = "object";
	(*p_defs)["required"][0] = "timestamp";
	(*p_defs)["properties"]["timestamp"]["description"]	= "timestamp";
	(*p_defs)["properties"]["timestamp"]["type"]		= "number";
	(*p_defs)["properties"]["timestamp"]["format"]		= "double";
	for(map<string,string>::iterator i = d.begin();i != d.end();i++) {
		(*p_defs)["properties"][i->first]["type"] 	= t[i->first];
		(*p_defs)["properties"][i->first]["description"]= i->second;
	}
}


/*********************************
 * Collector
 */

Collector::Collector(string p_name, HttpServer* p_server, Json::Value* p_cfg, uint p_history, uint p_freq_pool): cfg(p_cfg), morrisType("Line"), morrisOpts("behaveLikeLine:true,"), active(false), name(p_name), server(p_server) {
	if(! cfg->isMember("history")) {
		(*cfg)["history"] = p_history;
		(*cfg)["history"].setComment(std::string("/*\t\tNumber of elements to keep*/"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("poll-frequency")) {
		(*cfg)["poll-frequency"] = p_freq_pool;
		(*cfg)["poll-frequency"].setComment(std::string("/* \tNumber of seconds between snapshots*/"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("enable")) {
		(*cfg)["enable"] = true;
		(*cfg)["enable"].setComment(std::string("/*\t\tEnable this plugin ?*/"), Json::commentAfterOnSameLine);
	}
}
Collector::~Collector() {
	if (active) {
		active=false;
		my_thread.join();
	}
}
void Collector::startThread() {
	if (!active && (*cfg)["enable"].asBool()) {
		active=true;
		my_thread = std::thread ([this]() {
			int sec = ((*cfg)["poll-frequency"]).asInt();
			while(active) {
				collect();
				this_thread::sleep_for(chrono::seconds(sec));
			}
		});
	}
}
void Collector::addRessource(string p_name, string p_desc, std::string p_typeName) {
	map<string, Ressource*>::const_iterator it = ressources.find(p_name);
	if( it == ressources.end()) {
		ressources[p_name]	= new Ressource((*cfg)["history"].asUInt(), p_typeName);
		desc[p_name]		= p_desc;
	}
}

void Collector::addGetMetricRoute() {
	associate(server,"GET","^/"+name+"/(.*)/history$",doGetHistory);
	associate(server,"GET","^/"+name+"/(.*)/history.since=([0-9.]*)$",doGetHistory);
	associate(server,"GET","^/"+name+"/(.*)/graph$",doGetGraph);
}

void Collector::getDefinitions(Json::Value* p_defs) {
	Json::Value data(Json::objectValue);
	if ((*cfg)["enable"].asBool()) {
		for(map<string, Ressource*>::const_iterator i = ressources.begin();i!=ressources.end();i++) {
			if (! p_defs->isMember(i->second->typeName))
				(*p_defs)[i->second->typeName] = data;
			i->second->getDefinition( &((*p_defs)[i->second->typeName]) );
		}
	}
}

void Collector::getPaths(Json::Value* p_defs) {
	if ((*cfg)["enable"].asBool()) {
		for(map<string, Ressource*>::const_iterator i = ressources.begin();i!=ressources.end();i++) {
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["summary"] = desc[i->first];
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["parameters"][0]["name"] = "len";
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["parameters"][0]["in"] = "query";
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["parameters"][0]["type"] = "integer";
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["parameters"][0]["required"] = false;
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["parameters"][0]["description"] = "Length of the history to fetch";
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["responses"]["200"]["description"] = desc[i->first];
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["responses"]["200"]["schema"]["type"] = "array";
			(*p_defs)["/"+name+"/"+i->first+"/history"]["get"]["responses"]["200"]["schema"]["items"]["$ref"] = "#/definitions/"+i->second->typeName; //name+"-"+i->first;
		}
	}
}

void Collector::doGetGraph(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
	string id   = request->path_match[1];
	string str;
	const char* name_c  = name.c_str();

	map<string, Ressource*>::const_iterator it = ressources.find(id);
	if( it == ressources.end()) {
		setResponse404(response, "No such Metric");
		return;
	}
	std::stringstream stream;
        stream << "<html><head><title>" << APPS_NAME.c_str() << " - " << desc[id].c_str() << "</title>\n";
        stream << "<link rel=\"stylesheet\" href=\"http://cdnjs.cloudflare.com/ajax/libs/morris.js/0.5.1/morris.css\">";
	stream << "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.9.0/jquery.min.js\"></script>";
	stream << "<script src=\"http://cdnjs.cloudflare.com/ajax/libs/raphael/2.1.0/raphael-min.js\"></script>";
	stream << "<script src=\"http://cdnjs.cloudflare.com/ajax/libs/morris.js/0.5.1/morris.min.js\"></script>\n";
	stream << "</head><body><div id=\"" << name_c << "-graph\"></div>\n<script>\n";
	stream << "function updateLiveGraph(" << name_c << "Graph) {\n";
	stream << "  $.getJSON('/" << name_c << "/" << id.c_str() << "/history', function(results) { " << name_c;
	stream << "Graph.setData(results); });\n}\n" << name_c << "Graph = new Morris."<< morrisType.c_str() << "({element: '" << name_c;
	stream << "-graph',  data: [], xkey: 'timestamp', pointSize:0,fillOpacity:0.3," << morrisOpts.c_str();
	stream << ressources[id]->getMorrisDesc().c_str() << "});\n";
	stream << "updateLiveGraph(" << name_c << "Graph);setInterval(function() { updateLiveGraph(" << name_c;
	stream << "Graph); }, " << (*cfg)["history"].asString().c_str() << ");\n";
	stream << "</script></body></html>\n";
        setResponseHtml(response, stream.str());
}

void Collector::doGetHistory(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
	string name   = request->path_match[1];
	double since  = -1;
	if (request->path_match.size()>1) {
		try {
			since=stod(request->path_match[2]);
		} catch (std::exception &e) { }
	}
        /*if (request.query().has("since"))
		since= stod(request.query().get("since").get());*/

	map<string, Ressource*>::const_iterator it = ressources.find(name);
	if( it == ressources.end()) {
		setResponse404(response, "No such Metric");
		return;
	}
	setResponseJson(response, ressources[name]->getHistory(since));
}

void Collector::getIndexHtml(std::stringstream& stream ){
	if ((*cfg)["enable"].asBool()) {
		stream << "<h3>" << name.c_str() << "</h3><ul>\n";
		for(map<string, Ressource*>::const_iterator i = ressources.begin();i!=ressources.end();i++) {
			stream << "<li><a href=\"/" << name.c_str() << "/" << i->first.c_str() << "/graph\">" << desc[i->first].c_str() << "</a></li>\n";
		}
		stream << "</ul>\n";
	}
}


/*********************************
 * CollectorsManager
 */

namespace watcheD {
map<string, collector_maker_t *> factory;
}

void CollectorsManager::doGetRootPage(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
	std::stringstream stream;
        stream << "<html><head><title>" << APPS_NAME.c_str() << "</title>\n";
	stream << "</head><body>\n";
	for(std::map<std::string, Collector*>::iterator i = collectors.begin();i != collectors.end();i++) {
		i->second->getIndexHtml(stream);
	} 
	stream << "</body></html>\n";
        setResponseHtml(response, stream.str());
}
void CollectorsManager::doGetJson(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
	Json::Value res(Json::objectValue);
	Json::StreamWriterBuilder wbuilder;
	Json::Value obj(Json::objectValue);
	int cnt = 0;
	std::string document;
	
	res["swagger"]			= "2.0";
	res["info"]["version"]		= "1.0.0";
	res["info"]["title"]		= APPS_NAME;
	res["info"]["description"]	= APPS_DESC;
	//res["info"]["termsOfService"]	= "http://some.url/terms/";
	res["info"]["contact"]["name"]	= "Sebastien Huss";
	res["info"]["contact"]["email"]	= "sebastien.huss@gmail.com";
	//res["info"]["contact"]["url"]	= "http://sebastien.huss.free.fr/";
	res["info"]["license"]["name"]	= "AGPL";
	res["info"]["license"]["url"]	= "https://www.gnu.org/licenses/agpl-3.0.html";
	// add swaggerised docs
	//res["externalDocs"]["description"]	= "API documentation";
	//res["externalDocs"]["url"]		= "http://some.url/docs";
	
	res["host"]			= "localhost";
	res["basePath"]			= "/";
	res["schemes"][0]		= "http";
	res["consumes"][0]		= "application/json";
	res["produces"][0]		= "application/json";
	res["definitions"]		= obj;
	res["paths"]			= obj;


	for(std::map<std::string, Collector*>::iterator i = collectors.begin();i != collectors.end();i++,cnt++) {
		i->second->getDefinitions(&res["definitions"]);
		i->second->getPaths(&res["paths"]);
	}

	wbuilder["indentation"] = "\t";
	document = Json::writeString(wbuilder, res);

	setResponseJson(response, document);
}


CollectorsManager::CollectorsManager(HttpServer* p_server, Config* p_cfg) : server(p_server), cfg(p_cfg) {
	Json::Value*	servCfg = cfg->getServer();
	// 1st : find and load the modules
	map<string, collector_maker_t *, less<string> >::iterator factit;
	DIR *dir;
	const string directory = (*servCfg)["collectors_dir"].asString();
	class dirent *ent;
	class stat st;
	void *dlib;

			std::cout << "Loading from " << directory << "\n";
	dir = opendir(directory.c_str());
	while ((ent = readdir(dir)) != NULL) {
		const string file_name = ent->d_name;
		const string full_file_name = directory + "/" + file_name;
		const bool is_directory = (st.st_mode & S_IFDIR) != 0;

		if (file_name[0] == '.' || is_directory)
			continue;
		if (stat(full_file_name.c_str(), &st) == -1)
			continue;

		if (file_name.substr(file_name.rfind(".")) == ".so") {
			std::cout << "Loading " << file_name << "\n";
			dlib = dlopen(full_file_name.c_str(), RTLD_NOW);
			if(dlib == NULL){
				cerr << dlerror() << endl; 
				exit(-1);
			}
		}
	}
	closedir(dir);
	
	// Instanciate the plugins classes
	for(factit = factory.begin();factit != factory.end(); factit++) {
		collectors[factit->first]	= factit->second(server, cfg->getCollector(factit->first));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	associate(server,"GET","^/$",doGetRootPage);
	associate(server,"GET","^/api/swagger.json$",doGetJson);
}

void CollectorsManager::startThreads() {
	for(map<string, collector_maker_t *, less<string> >::iterator factit = factory.begin();factit != factory.end(); factit++)
		collectors[factit->first]->startThread();
}

CollectorsManager::~CollectorsManager() {
	// TODO free the ressources here
}
