#!/bin/bash

set -e

# Change this when compiling for arm
ARCH=x86_64-unknown-linux-gnu
TEMP_DIR="/tmp/local-nvcore-$BASHPID"
UE_THIRD_PARTY_DIR=`cd "../../../../"; pwd`
BASE_DIR=`cd "../../"; pwd`
FINAL_DIR=`pwd`

echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"
echo "BINARY_INSTALL=$BINARY_INSTALL"

if [ -d "$TEMP_DIR" ]; then
	rm -rf "$TEMP_DIR"
fi

mkdir $TEMP_DIR
cd $TEMP_DIR
CXXFLAGS="-g3 -O3 -fPIC -nostdlib -std=c++11 -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/"
LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc -lpthread"
cmake $BASE_DIR/src -DCMAKE_INSTALL_PREFIX="$TEMP_DIR" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_MODULE_LINKER_FLAGS="$LIBS" -DCMAKE_EXE_LINKER_FLAGS="$LIBS" -DCMAKE_SHARED_LINKER_FLAGS="$LIBS" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DNVTT_SHARED=1 -DCMAKE_CXX_STANDARD_LIBRARIES="$LIBS"
make install

echo "=========== Copy nvTextureTools DSOs ============"
cp -v $TEMP_DIR/lib/*.so "$FINAL_DIR"
