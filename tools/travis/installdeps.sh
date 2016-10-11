#!/bin/sh

mkdir deps
cd deps
git clone https://github.com/open-source-parsers/jsoncpp.git
mkdir jsoncpp/build
cd jsoncpp/build
cmake ..
make -j 4
sudo make install
