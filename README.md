# watched.agent
[![Status](https://travis-ci.org/sebt3/watched.agent.svg?branch=master)](https://travis-ci.org/sebt3/watched.agent)
## Overview
[Watched](https://sebt3.github.io/watched/) aim to become a performance analysis and a monitoring tool.
This is the plugin-based agent written in c++. It provide a REST api to the data that the backend consume.
Plugins can be written in c++ or in lua

## Dependencies
This project is based on the following projects :
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [SimpleWebServer](https://github.com/eidheim/Simple-Web-Server) (itself based on Boost::asio)
* [Selene](https://github.com/jeremyong/Selene) to provide lua integration

## Other componants
* [watched.back](https://github.com/sebt3/watched.back) the backend. Centralize the agents metric and monitor them.
* [watched.front](https://github.com/sebt3/watched.front) the frontend. 

## build instruction
    mkdir build
    cd build
    cmake ..
    make

## Current status
This is the very early days.
Collect the base system metrics.
