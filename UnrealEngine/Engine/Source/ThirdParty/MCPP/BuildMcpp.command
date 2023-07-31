#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.
#
# Simple shell file to build mcpp on macOS systems

cd "`dirname "$0"`"
cd ./mcpp-2.7.2

export CFLAGS='-mmacosx-version-min=10.9 -arch x86_64 -arch arm64 -gdwarf-2'

./configure --enable-mcpplib --enable-static

make

cp -f ./src/.libs/libmcpp.a ./lib/Mac

unset CFLAGS
