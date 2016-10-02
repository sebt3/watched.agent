# watched.agent
## Overview
Watched aim to become a performance analysis and a monitoring tool.
This is the plugin-based agent written in c++. It provide a REST api to the data that the backend consume.

## Dependencies
This project is based on the following projects :
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [SimpleWebServer](https://github.com/eidheim/Simple-Web-Server) (itself based on Boost::asio)

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
