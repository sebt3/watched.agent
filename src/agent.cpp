#include "agent.h"
#include "services.h"
#include <fstream>
using namespace watcheD;

#include <boost/filesystem.hpp>


const std::string SERVER_HEAD="watched.agent/0.1";
const std::string APPS_NAME="watched.agent";
const std::string APPS_DESC="Watch over wasted being washed up";
#include "config.h"


void default_resource_send(const HttpServer &server, std::shared_ptr<HttpServer::Response> response,
                           std::shared_ptr<std::ifstream> ifs, std::shared_ptr<std::vector<char> > buffer) {
	std::streamsize read_length;
	if((read_length=ifs->read(&(*buffer)[0], buffer->size()).gcount())>0) {
		response->write(&(*buffer)[0], read_length);
		if(read_length==static_cast<std::streamsize>(buffer->size())) {
			server.send(response, [&server, response, ifs, buffer](const boost::system::error_code &ec) {
				if(!ec)
					default_resource_send(server, response, ifs, buffer);
				else
					std::cerr << "Connection interrupted" << std::endl;
			});
		}
	}
}

int main(int argc, char *argv[]) {
	std::string cfgfile = WATCHED_CONFIG;
	if (argc>1) cfgfile = argv[1];
	Config cfg(cfgfile);
	Json::Value*	servCfg = cfg.getServer();
	int port_i	= (*servCfg)["port"].asInt();

	HttpServer server(port_i, 1);
	server.config.address  = (*servCfg)["host"].asString();
	
	server.default_resource["GET"]=[&server](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
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
	};

	//CollectorsManager cm(&server,&cfg);
	servicesManager *sm = new servicesManager(&server,&cfg);
	cfg.save();

	sm->startThreads();
	//cm.startThreads();

	sm->find();

	server.start();

	return 0;
}
