#!/bin/bash

set -e

ARCH=x86_64-unknown-linux-gnu
UE_THIRD_PARTY_DIR=`cd "../../../.."; pwd`
TEMP_DIR_RELEASE="/tmp/local-harfbuzz-release-$BASHPID"
TEMP_DIR_DEBUG="/tmp/local-harfbuzz-debug-$BASHPID"
BASE_DIR=`cd "../../BuildForUE"; pwd`

mkdir $TEMP_DIR_RELEASE
mkdir $TEMP_DIR_DEBUG

CXXFLAGS="-std=c++11 -ffunction-sections -fdata-sections -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc -lpthread"

cd $TEMP_DIR_RELEASE
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_MODULE_LINKER_FLAGS="$LIBS" -DCMAKE_EXE_LINKER_FLAGS="$LIBS" -DCMAKE_SHARED_LINKER_FLAGS="$LIBS" "$BASE_DIR"
make -j4
cp -v ../libharfbuzz.a "$BASE_DIR/../lib/Linux/$ARCH/libharfbuzz_fPIC.a"

cd $TEMP_DIR_DEBUG
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_MODULE_LINKER_FLAGS="$LIBS" -DCMAKE_EXE_LINKER_FLAGS="$LIBS" -DCMAKE_SHARED_LINKER_FLAGS="$LIBS" "$BASE_DIR"
make -j4
cp -v ../libharfbuzz.a "$BASE_DIR/../lib/Linux/$ARCH/libharfbuzzd_fPIC.a"
