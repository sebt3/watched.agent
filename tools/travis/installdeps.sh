#!/bin/sh

mkdir deps
cd deps
#git clone https://github.com/open-source-parsers/jsoncpp.git
#mkdir jsoncpp/build
#cd jsoncpp/build
#cmake ..
#make -j 4
#sudo make install
git clone https://github.com/chriskohlhoff/asio.git
cd asio/asio
sed -i 's/SUBDIRS = include src/SUBDIRS = include/g' Makefile.am
sh autogen.sh
./configure
make install
