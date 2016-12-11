#include "agent.h"
#include <fstream>
#include <chrono>
using namespace watcheD;

#include <boost/filesystem.hpp>
#include <openssl/sha.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>


const std::string SERVER_HEAD="watched.agent/0.1";
const std::string APPS_NAME="watched.agent";
const std::string APPS_DESC="Watch over wasted being washed up";
#include "config.h"

std::string sha256(const std::string p_fname) {
	std::string ret = "";
	unsigned char hash[SHA256_DIGEST_LENGTH];
	std::ifstream file(p_fname, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<char> buffer(size);
	if (!file.read(buffer.data(), size))
		return ret;

	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, reinterpret_cast<char*> (&buffer[0]), size);
	SHA256_Final(hash, &sha256);
	std::stringstream ss;
	for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	}
	return ss.str();
}

void default_resource_send(std::shared_ptr<SWHttpServer> server, std::shared_ptr<SWHttpServer::Response> response,
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
}

void defaults_resource_send(std::shared_ptr<SWHttpsServer> server, std::shared_ptr<SWHttpsServer::Response> response,
                           std::shared_ptr<std::ifstream> ifs, std::shared_ptr<std::vector<char> > buffer) {
	std::streamsize read_length;
	if((read_length=ifs->read(&(*buffer)[0], buffer->size()).gcount())>0) {
		response->write(&(*buffer)[0], read_length);
		if(read_length==static_cast<std::streamsize>(buffer->size())) {
			server->send(response, [&server, response, ifs, buffer](const boost::system::error_code &ec) {
				if(!ec)
					defaults_resource_send(server, response, ifs, buffer);
				else
					std::cerr << "Connection interrupted" << std::endl;
			});
		}
	}
}


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
					(stat (sslcert.c_str(), &buffer) == 0) && // check if files exist
					(stat (sslvrf.c_str(), &buffer) == 0) &&
					(stat (sslkey.c_str(), &buffer) == 0) );

	if (useSSL) {
		https = std::make_shared<SWHttpsServer>(port_i, 1, sslcert, sslkey, 5, 300, sslvrf, true);
		https->config.address  = (*cfg)["host"].asString();
		https->default_resource["GET"]=[this](std::shared_ptr<SWHttpsServer::Response> response, std::shared_ptr<SWHttpsServer::Request> request) {
			try {
				auto web_root_path=boost::filesystem::canonical((*cfg)["web_root"].asString());
				auto path=boost::filesystem::canonical(web_root_path/request->path);
				//Check if path is within web_root_path
				if(std::distance(web_root_path.begin(), web_root_path.end())>std::distance(path.begin(), path.end()) ||
				!std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
					throw std::invalid_argument("path must be within root path");
				if(boost::filesystem::is_directory(path))
					path/="index.html";
				if(!(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)))
					throw std::invalid_argument("file does not exist");

				std::string hash = sha256(path.string());
				auto it=request->header.find("If-None-Match");
				if(it!=request->header.end()) {
					if (it->second == "\""+hash+"\"") {
						*response << "HTTP/1.1 304 Not Modified\r\nCache-Control: max-age=86400\r\nETag: \""+hash+"\"\r\n\r\n";
						return;
					}
				}

				auto ifs=std::make_shared<std::ifstream>();
				ifs->open(path.string(), std::ifstream::in | std::ios::binary);
				
				if(*ifs) {
					//read and send 128 KB at a time
					std::streamsize buffer_size=131072;
					auto buffer=std::make_shared<std::vector<char> >(buffer_size);
					
					ifs->seekg(0, std::ios::end);
					auto length=ifs->tellg();
					
					ifs->seekg(0, std::ios::beg);
					
					*response << "HTTP/1.1 200 OK\r\nCache-Control: max-age=86400\r\nETag: \""+hash+"\"\r\nContent-Length: " << length << "\r\n\r\n";
					defaults_resource_send(https, response, ifs, buffer);
				}
				else
					throw std::invalid_argument("could not read file");
			}
			catch(const std::exception &e) {
				std::string content="Could not open path "+request->path+": "+e.what();
				*response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
			}
		};
	} else {
		http = std::make_shared<SWHttpServer>(port_i, 1);
		http->config.address  = (*cfg)["host"].asString();
		http->default_resource["GET"]=[this](std::shared_ptr<SWHttpServer::Response> response, std::shared_ptr<SWHttpServer::Request> request) {
			try {
				auto web_root_path=boost::filesystem::canonical((*cfg)["web_root"].asString());
				auto path=boost::filesystem::canonical(web_root_path/request->path);
				//Check if path is within web_root_path
				if(std::distance(web_root_path.begin(), web_root_path.end())>std::distance(path.begin(), path.end()) ||
				!std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
					throw std::invalid_argument("path must be within root path");
				if(boost::filesystem::is_directory(path))
					path/="index.html";
				if(!(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)))
					throw std::invalid_argument("file does not exist");

				std::string hash = sha256(path.string());
				auto it=request->header.find("If-None-Match");
				if(it!=request->header.end()) {
					if (it->second == "\""+hash+"\"") {
						*response << "HTTP/1.1 304 Not Modified\r\nCache-Control: max-age=86400\r\nETag: \""+hash+"\"\r\n\r\n";
						return;
					}
				}

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
					default_resource_send(http, response, ifs, buffer);
				}
				else
					throw std::invalid_argument("could not read file");
			}
			catch(const std::exception &e) {
				std::string content="Could not open path "+request->path+": "+e.what();
				*response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
			}
		};
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

std::string HttpServer::getHead(std::string p_title, std::string p_sub) {
	return "<!DOCTYPE html>\n<html><head>\n<meta charset=\"utf-8\">\n<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n<title>"+APPS_NAME+" - "+p_title+"</title>\n<meta content=\"width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no\" name=\"viewport\">\n<link rel=\"shortcut icon\" href=\"/favicon.ico\">\n<link rel=\"stylesheet\" href=\"/css/bootstrap.min.css\">\n<link rel=\"stylesheet\" href=\"/css/AdminLTE.min.css\">\n<link rel=\"stylesheet\" href=\"/css/skin-blue.css\">\n<link rel=\"stylesheet\" href=\"/css/font-awesome.min.css\">\n<link rel=\"stylesheet\" href=\"/css/ionicons.min.css\">\n<link rel=\"stylesheet\" href=\"/css/morris.css\">\n</head><body class=\"hold-transition skin-blue sidebar-collapse sidebar-mini\">\n<div class=\"wrapper\">\n<header class=\"main-header\"><a href=\"/\" class=\"logo\"> <span class=\"logo-mini\"><img src=\"/pic/watched32x32.png\" height=\"32\" width=\"32\" /></span></a><nav class=\"navbar navbar-static-top\"><a class=\"navbar-brand\" href=\"#\"><b>"+APPS_NAME+"</b></a><div class=\"navbar-custom-menu\"><ul class=\"nav navbar-nav\"></ul></div></nav></header>\n<aside class=\"main-sidebar\"><section class=\"sidebar\"><ul class=\"sidebar-menu\"></ul></section></aside>\n<div class=\"content-wrapper\"><section class=\"content-header\"><h1>"+p_title+"<small>"+p_sub+"</small></h1></section><section class=\"content\">\n";
}

std::string HttpServer::getFoot(std::string p_script) {
	return "</section></div><footer class=\"main-footer\"><div class=\"pull-right hidden-xs\"><b>Version</b> 0.1.0    </div><strong>Copyright &copy; 2016<a href=\"https://github.com/sebt3\">Sébastien Huss</a>.</strong> All rights reserved.</footer></div>\n<script src=\"/js/jquery-2.2.3.min.js\"></script>\n<script src=\"/js/bootstrap.min.js\"></script>\n<script src=\"/js/Chart.min.js\"></script>\n<script src=\"/js/adminlte.min.js\"></script>\n<script src=\"/js/raphael-min.js\"></script>\n<script src=\"/js/morris.min.js\"></script>\n<script>\n"+p_script+"</script>\n</body>\n</html>\n";
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

	std::shared_ptr<servicesManager> sm = std::make_shared<servicesManager>(server,cfg);
	sm->init();
	cfg->save();

	sm->startThreads();
	server->start();

	return 0;
}
