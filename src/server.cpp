#include "agent.h"
#include "config.h"
#include <fstream>
#include <chrono>
//#include <boost/lexical_cast.hpp>
//#include <boost/filesystem.hpp>
#include <openssl/sha.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>


namespace watcheD {

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
			server->send(response, [server, response, ifs, buffer](const std::error_code &ec) {
				if(!ec)
					default_resource_send(server, response, ifs, buffer);
				/*else
					logInfo("Connection interrupted while sending a static file over HTTP");*/
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
			server->send(response, [server, response, ifs, buffer](const std::error_code &ec) {
				if(!ec)
					defaults_resource_send(server, response, ifs, buffer);
				/*else
					logInfo("Connection interrupted while sending a static file over HTTPS");*/
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

HttpServer::HttpServer(Json::Value* p_cfg, Json::Value* p_logcfg) : cfg(p_cfg) {
	struct stat buffer;
	int port_i		= (*cfg)["port"].asInt();
	std::string sslkey	= (*cfg)["SSL_key"].asString();
	std::string sslcert	= (*cfg)["SSL_cert"].asString();
	std::string sslvrf	= (*cfg)["SSL_verify"].asString();
	useSSL			= (	(*cfg)["useSSL"].asBool() &&
					(stat (sslcert.c_str(), &buffer) == 0) && // check if files exist
					(stat (sslvrf.c_str(), &buffer) == 0) &&
					(stat (sslkey.c_str(), &buffer) == 0) );
	
	l = std::make_shared<log>(p_logcfg);

	if (useSSL) {
		https = std::make_shared<SWHttpsServer>(port_i, 1, sslcert, sslkey, 5, 300, sslvrf);
		https->config.address  = (*cfg)["host"].asString();
		https->on_error = [this](request_ptr_s req, const std::error_code& ec)  {
			std::string err = ec.message();
			if (ec.category() == asio::error::get_ssl_category()) {
				err = std::string(" (")
					+std::to_string(ERR_GET_LIB(ec.value()))+","
					+std::to_string(ERR_GET_FUNC(ec.value()))+","
					+std::to_string(ERR_GET_REASON(ec.value()))+") ";
				char buf[128];
				::ERR_error_string_n(ec.value(), buf, sizeof(buf));
				err += buf;
			}
			logNotice("HttpServer::httpsOnError", err);
		};

		https->default_resource["GET"]=[this](std::shared_ptr<SWHttpsServer::Response> response, std::shared_ptr<SWHttpsServer::Request> request) {
			struct stat st;
			try {
				const std::string web_root_path=(*cfg)["web_root"].asString();
				std::string path=web_root_path+"/"+request->path;
				if (stat(path.c_str(), &st) == -1)
					throw std::invalid_argument("file does not exist");
				const bool is_directory = (st.st_mode & S_IFDIR) != 0;
				if (is_directory)
					path += "/index.html";
				if (stat(path.c_str(), &st) == -1)
					throw std::invalid_argument("file does not exist");

				std::string hash = sha256(path);
				auto it=request->header.find("If-None-Match");
				if(it!=request->header.end()) {
					if (it->second == "\""+hash+"\"") {
						*response << "HTTP/1.1 304 Not Modified\r\nCache-Control: max-age=86400\r\nETag: \""+hash+"\"\r\n\r\n";
						return;
					}
				}

				auto ifs=std::make_shared<std::ifstream>();
				ifs->open(path, std::ifstream::in | std::ios::binary);
				
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
		http->on_error = [this](request_ptr_h req, const std::error_code& ec)  {
			std::string err = ec.message();
			if (err!="Operation canceled")
				logNotice("HttpServer::httpOnError", err);
		};

		http->default_resource["GET"]=[this](std::shared_ptr<SWHttpServer::Response> response, std::shared_ptr<SWHttpServer::Request> request) {
			struct stat st;
			try {
				const std::string web_root_path=(*cfg)["web_root"].asString();
				std::string path=web_root_path+"/"+request->path;
				if (stat(path.c_str(), &st) == -1)
					throw std::invalid_argument("file does not exist");
				const bool is_directory = (st.st_mode & S_IFDIR) != 0;
				if (is_directory)
					path += "/index.html";
				if (stat(path.c_str(), &st) == -1)
					throw std::invalid_argument("file does not exist");

				std::string hash = sha256(path);
				auto it=request->header.find("If-None-Match");
				if(it!=request->header.end()) {
					if (it->second == "\""+hash+"\"") {
						*response << "HTTP/1.1 304 Not Modified\r\nCache-Control: max-age=86400\r\nETag: \""+hash+"\"\r\n\r\n";
						return;
					}
				}

				auto ifs=std::make_shared<std::ifstream>();
				ifs->open(path, std::ifstream::in | std::ios::binary);
				
				if(*ifs) {
					//read and send 128 KB at a time
					std::streamsize buffer_size=131072;
					auto buffer=std::make_shared<std::vector<char> >(buffer_size);
					
					ifs->seekg(0, std::ios::end);
					auto length=ifs->tellg();
					
					ifs->seekg(0, std::ios::beg);
					
					*response << "HTTP/1.1 200 OK\r\nCache-Control: max-age=86400\r\nETag: \""+hash+"\"\r\nContent-Length: " << length << "\r\n\r\n";
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

void HttpServer::logError(  const std::string p_src, std::string p_message) { l->error(  p_src, p_message); }
void HttpServer::logWarning(const std::string p_src, std::string p_message) { l->warning(p_src, p_message); }
void HttpServer::logInfo(   const std::string p_src, std::string p_message) { l->info(   p_src, p_message); }
void HttpServer::logNotice( const std::string p_src, std::string p_message) { l->notice( p_src, p_message); }
void HttpServer::logDebug(  const std::string p_src, std::string p_message) { l->debug(  p_src, p_message); }


std::string HttpServer::getHead(std::string p_title, std::string p_sub) {
	return "<!DOCTYPE html>\n<html><head>\n<meta charset=\"utf-8\">\n<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n<title>"+APPS_NAME+" - "+p_title+"</title>\n<meta content=\"width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no\" name=\"viewport\">\n<link rel=\"shortcut icon\" href=\"/favicon.ico\">\n<link rel=\"stylesheet\" href=\"/css/bootstrap.min.css\">\n<link rel=\"stylesheet\" href=\"/css/AdminLTE.min.css\">\n<link rel=\"stylesheet\" href=\"/css/skin-blue.css\">\n<link rel=\"stylesheet\" href=\"/css/font-awesome.min.css\">\n<link rel=\"stylesheet\" href=\"/css/ionicons.min.css\">\n<link rel=\"stylesheet\" href=\"/css/watched.css\">\n</head><body class=\"hold-transition skin-blue sidebar-collapse sidebar-mini\">\n<div class=\"wrapper\">\n<header class=\"main-header\"><a href=\"/\" class=\"logo\"> <span class=\"logo-mini\"><img src=\"/pic/watched32x32.png\" height=\"32\" width=\"32\" /></span></a><nav class=\"navbar navbar-static-top\"><a class=\"navbar-brand\" href=\"#\"><b>"+APPS_NAME+"</b></a><div class=\"navbar-custom-menu\"><ul class=\"nav navbar-nav\"></ul></div></nav></header>\n<aside class=\"main-sidebar\"><section class=\"sidebar\"><ul class=\"sidebar-menu\"></ul></section></aside>\n<div class=\"content-wrapper\"><section class=\"content-header\"><h1>"+p_title+"<small>"+p_sub+"</small></h1></section><section class=\"content\">\n";
}

std::string HttpServer::getFoot(std::string p_script) {
	return "</section></div><footer class=\"main-footer\"><div class=\"pull-right hidden-xs\"><b>Version</b> 0.1.0    </div><strong>Copyright &copy; 2016<a href=\"https://github.com/sebt3\">Sébastien Huss</a>.</strong> All rights reserved.</footer></div>\n<script src=\"/js/jquery-2.2.3.min.js\"></script>\n<script src=\"/js/bootstrap.min.js\"></script>\n<script src=\"/js/adminlte.min.js\"></script>\n<script src=\"/js/d3.v4.min.js\"></script>\n<script src=\"/js/watched.js\"></script>\n<script src=\"/js/watched.line.js\"></script>\n<script>\n"+p_script+"</script>\n</body>\n</html>\n";
}

}
