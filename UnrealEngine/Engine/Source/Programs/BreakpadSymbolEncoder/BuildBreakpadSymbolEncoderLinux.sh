#!/bin/bash

set -e

IFILE="BreakpadSymbolEncoder.cpp"
OFILE="BreakpadSymbolEncoder"
CXX=clang++
UE_THIRD_PARTY_DIR=`cd "../../ThirdParty"; pwd`

echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"

CXXFLAGS="-O3 -fPIC -std=c++11 -stdlib=libc++ -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/"
LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc -lpthread"

echo "$CXX $CXXFLAGS $LDFLAGS $IFILE -o $OFILE $LIBS"
exec $CXX $CXXFLAGS $LDFLAGS $IFILE -o $OFILE $LIBS
