#!/bin/sh
# Prerequisites:
#  clang++
#  cmake 3.5

sh build-linux.sh MinSizeRel
sh build-linux.sh MinSizeRel fPIC 
