#!/bin/bash

# yum install glibc-devel glibc-static clang cmake make gcc

set -e

SOURCE_VER="5.1.0"
SOURCE_NAME="metis-$SOURCE_VER"
SOURCE_PATH="http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/$SOURCE_NAME.tar.gz"

cd "../../"
if [ ! -f $SOURCE_NAME.tar.gz ]; then
	wget $SOURCE_PATH
	tar -xvf $SOURCE_NAME.tar.gz
fi

ARCH=x86_64-unknown-linux-gnu
#ARCH=aarch64-unknown-linux-gnueabi

UE_THIRD_PARTY_DIR=`cd ".."; pwd`
BASE_DIR=`cd "$SOURCE_NAME"; pwd`

cd $BASE_DIR

CXXFLAGS="-nostdlib -std=c++11 -ffunction-sections -fdata-sections -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/$ARCH/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/$ARCH/libc++abi.a -lm -lc -lgcc_s -lgcc -lpthread"

make config
make -j4
cp -v "build/Linux-x86_64/libmetis/libmetis.a" "../$SOURCE_VER/libmetis/Linux/$ARCH/Release"

make config debug=1
make -j4
cp -v "build/Linux-x86_64/libmetis/libmetis.a" "../$SOURCE_VER/libmetis/Linux/$ARCH/Debug"
