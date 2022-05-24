#!/bin/bash

cd bin
ln -s ../result result
ln -s ../config config
ln -s ../log log

cd ../build
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Debug ..