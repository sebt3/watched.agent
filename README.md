# watched.agent
[![Status](https://travis-ci.org/sebt3/watched.agent.svg?branch=master)](https://travis-ci.org/sebt3/watched.agent)
## Overview
[Watched](https://sebt3.github.io/watched/) aim to become a performance analysis and a monitoring tool.
This is the plugin-based agent written in C++. It provide a REST api to the data that the backend consume.
Plugins can be written in c++ or in lua

## Dependencies
This project is based on the following projects :
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [SimpleWebServer](https://github.com/eidheim/Simple-Web-Server) (itself based on Boost::asio)
* [Selene](https://github.com/jeremyong/Selene) to provide lua integration
* [Lua](https://www.lua.org) Version 5.2 or 5.3
* [Boost](http://www.boost.org) version >= 1.54
* C++11 compatible compiler (gcc >=4.9, clang >=3.3)
* Linux

## Other componants
* [watched.back](https://github.com/sebt3/watched.back) Centralize the agents metric and monitor them.
* [watched.front](https://github.com/sebt3/watched.front) Provide the interface to the centralized data

## build instruction
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
Complete instructions [here](https://sebt3.github.io/watched/doc/install/#the-agent).


## Current features
- Very lightweight
- Forward the collected data to the backend over a REST service
- Collect system metric based on C++ or lua plugins 
- Detect network services and monitor them
- (untested) C++ plugin framework for services management
- Can be secured using SSL (tls1.2)
- Offer a web micro frontend (might be used to monitor a single host)

## Planned
- Service performance data (with initial plugins : CPU, memory, IO and cgroup)
- Service log monitoring
- Service classification
- Lua plugins support for services detection, enhencement
- Plugins for :
  * systemd services detection, enhencement and log monitoring
  * docker services detection, enhencement
  * libvirt
  * pacemaker
  * and more
- More than one listen adress
