#include "agent.h"
#include <fstream>
#include <chrono>
using namespace watcheD;

#include <boost/filesystem.hpp>


const std::string SERVER_HEAD="watched.agent/0.1";
const std::string APPS_NAME="watched.agent";
const std::string APPS_DESC="Watch over wasted being washed up";
#include "config.h"


/*void default_resource_send(std::shared_ptr<HttpServer> server, std::shared_ptr<HttpServer::Response> response,
                           std::shared_ptr<std::ifstream> ifs, std::shared_ptr<std::vector<char> > buffer) {
	std::streamsize read_length;
	if((read_length=ifs->read(&(*buffer)[0], buffer->size()).gcount())>0) {
		response->write(&(*buffer)[0], read_length);
		if(read_length==static_cast<std::streamsize>(buffer->size())) {
			server->send(response, [&server, response, ifs, buffer](const boost::system::error_code &ec) {
				if(!ec)
					default_resource_send(server, response, ifs, buffer);
				else
					std::cerr << "Connection interrupted" << std::endl;
			});
		}
	}
}*/


/*********************************
 * Server
 */
typedef std::shared_ptr<SWHttpServer::Response> response_ptr_h;
typedef std::shared_ptr<SWHttpServer::Request> request_ptr_h;
typedef std::shared_ptr<SWHttpsServer::Response> response_ptr_s;
typedef std::shared_ptr<SWHttpsServer::Request> request_ptr_s;

HttpServer::HttpServer(Json::Value* p_cfg) : cfg(p_cfg) {
	struct stat buffer;
	int port_i		= (*cfg)["port"].asInt();
	std::string sslkey	= (*cfg)["SSL_key"].asString();
	std::string sslcert	= (*cfg)["SSL_cert"].asString();
	std::string sslvrf	= (*cfg)["SSL_verify"].asString();
	useSSL			= (	(*cfg)["useSSL"].asBool() &&
					(stat (sslcert.c_str(), &buffer) == 0) && // check if file exist
					(stat (sslvrf.c_str(), &buffer) == 0) &&
					(stat (sslkey.c_str(), &buffer) == 0) );

	if (useSSL) {
		https = std::make_shared<SWHttpsServer>(port_i, 1, sslcert, sslkey, 5, 300, sslvrf, true);
		https->config.address  = (*cfg)["host"].asString();
	} else {
		http = std::make_shared<SWHttpServer>(port_i, 1);
		http->config.address  = (*cfg)["host"].asString();
	}
}
void HttpServer::setRegex(std::string p_opt, std::string p_regex, std::function<void(response_ptr, request_ptr)> p_fnct) {
	if (useSSL && https != nullptr) {
		https->resource[p_regex][p_opt] = [p_fnct](response_ptr_s response, request_ptr_s request) {
			response_ptr resp	= std::make_shared<std::stringstream>();
			request_ptr args	= std::make_shared<std::vector<std::string>>();
			for(unsigned int i=1;i<=request->path_match.size();i++)
				args->push_back(request->path_match[i]);
			p_fnct(resp, args );
			*response << resp->str();
		};
	} else if (http != nullptr) {
		http->resource[p_regex][p_opt] = [p_fnct](response_ptr_h response, request_ptr_h request) {
			response_ptr resp	= std::make_shared<std::stringstream>();
			request_ptr args	= std::make_shared<std::vector<std::string>>();
			for(unsigned int i=1;i<=request->path_match.size();i++)
				args->push_back(request->path_match[i]);
			p_fnct(resp, args );
			*response << resp->str();
		};
	}
}
void HttpServer::setDefault(std::string p_opt, std::function<void(response_ptr, request_ptr)> p_fnct) {
	if (useSSL && https != nullptr) {
		https->default_resource[p_opt] = [p_fnct](response_ptr_s response, request_ptr_s request) {
			response_ptr resp	= std::make_shared<std::stringstream>();
			request_ptr args	= std::make_shared<std::vector<std::string>>();
			for(unsigned int i=1;i<=request->path_match.size();i++)
				args->push_back(request->path_match[i]);
			p_fnct(resp, args );
			*response << resp->str();
		};
	} else if (http != nullptr) {
		http->default_resource[p_opt] = [p_fnct](response_ptr_h response, request_ptr_h request) {
			response_ptr resp	= std::make_shared<std::stringstream>();
			request_ptr args	= std::make_shared<std::vector<std::string>>();
			for(unsigned int i=1;i<=request->path_match.size();i++)
				args->push_back(request->path_match[i]);
			p_fnct(resp, args );
			*response << resp->str();
		};
	}
}
void HttpServer::start() {
	if (useSSL)
		https->start();
	else
		http->start();
}


/*********************************
 * main
 */
int main(int argc, char *argv[]) {
	std::string cfgfile = WATCHED_CONFIG;
	if (argc>1) cfgfile = argv[1];
	std::shared_ptr<Config> cfg = std::make_shared<Config>(cfgfile);
	Json::Value*	servCfg = cfg->getServer();
	
	std::shared_ptr<HttpServer> server = std::make_shared<HttpServer>(servCfg);
	
	/*
	server->default_resource["GET"]=[server](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
		try {
			auto web_root_path=boost::filesystem::canonical("web");
			auto path=boost::filesystem::canonical(web_root_path/request->path);
			//Check if path is within web_root_path
			if(std::distance(web_root_path.begin(), web_root_path.end())>std::distance(path.begin(), path.end()) ||
			!std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
				throw std::invalid_argument("path must be within root path");
			if(boost::filesystem::is_directory(path))
				path/="index.html";
			if(!(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)))
				throw std::invalid_argument("file does not exist");
			
			auto ifs=std::make_shared<std::ifstream>();
			ifs->open(path.string(), std::ifstream::in | std::ios::binary);
			
			if(*ifs) {
				//read and send 128 KB at a time
				std::streamsize buffer_size=131072;
				auto buffer=std::make_shared<std::vector<char> >(buffer_size);
				
				ifs->seekg(0, std::ios::end);
				auto length=ifs->tellg();
				
				ifs->seekg(0, std::ios::beg);
				
				*response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";
				default_resource_send(server, response, ifs, buffer);
			}
			else
				throw std::invalid_argument("could not read file");
		}
		catch(const std::exception &e) {
			std::string content="Could not open path "+request->path+": "+e.what();
			*response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
		}
	};*/

	std::shared_ptr<servicesManager> sm = std::make_shared<servicesManager>(server,cfg);
	sm->init();
	cfg->save();

	sm->startThreads();
	server->start();

	return 0;
}
