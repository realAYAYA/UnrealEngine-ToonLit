#!/bin/bash

set -e

# Change this when compiling for arm
OPENVR_VERSION=1.0.16
ARCH=x86_64-unknown-linux-gnu
TEMP_DIR="/tmp/local-openvr-$BASHPID"
UE_THIRD_PARTY_DIR=`cd "../../../"; pwd`
SOURCE_DIR="`pwd`/openvr-$OPENVR_VERSION"
FINAL_DIR=`pwd`

echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"
echo "BINARY_INSTALL=$BINARY_INSTALL"
echo "SOURCE_DIR=$SOURCE_DIR"

if [ -d "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
fi

tar -xvf "v$OPENVR_VERSION.tar.gz"
cd "openvr-$OPENVR_VERSION"

echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"
mkdir $TEMP_DIR
cd $TEMP_DIR
CXXFLAGS="-g3 -O3 -fPIC -nostdlib -std=c++11 -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/"
LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc -lpthread"
cmake $SOURCE_DIR -DCMAKE_INSTALL_PREFIX="$TEMP_DIR" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_MODULE_LINKER_FLAGS="$LIBS" -DCMAKE_EXE_LINKER_FLAGS="$LIBS" -DCMAKE_SHARED_LINKER_FLAGS="$LIBS" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DBUILD_SHARED=ON -DCMAKE_CXX_STANDARD_LIBRARIES="$LIBS"
make install

echo "=========== Copy OpenVR DSOs ============"
cp -v $TEMP_DIR/lib/*.so "$FINAL_DIR"
