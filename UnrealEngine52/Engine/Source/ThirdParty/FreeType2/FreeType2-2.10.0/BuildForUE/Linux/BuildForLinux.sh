#!/bin/bash

set -e

# Set this for other arch if not on x86_64
ARCH=x86_64-unknown-linux-gnu
TEMP_DIR_RELEASE="/tmp/local-freetype-release-$BASHPID"
TEMP_DIR_DEBUG="/tmp/local-freetype-debug-$BASHPID"
BASE_DIR=`cd "../../"; pwd`
UE_THIRD_PARTY_DIR=`cd "../../../.."; pwd`

LIB_ARCH_PATH="lib/Linux/x86_64-unknown-linux-gnu"
LIB_PNG_PATH="$UE_THIRD_PARTY_DIR/libPNG/libPNG-1.5.2/$LIB_ARCH_PATH;$UE_THIRD_PARTY_DIR/libPNG/libPNG-1.5.2"
ZLIB_PATH="$UE_THIRD_PARTY_DIR/zlib/v1.2.8/$LIB_ARCH_PATH"

mkdir $TEMP_DIR_RELEASE
mkdir $TEMP_DIR_DEBUG

CFLAGS="-ffunction-sections -fdata-sections"

cd $TEMP_DIR_RELEASE
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$LIB_PNG_PATH;$ZLIB_PATH" -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="$CFLAGS" "$BASE_DIR"
make -j4
cp -v libfreetype.a "$BASE_DIR/lib/Linux/$ARCH/libfreetype_fPIC.a"

cd $TEMP_DIR_DEBUG
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$LIB_PNG_PATH;$ZLIB_PATH" -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="$CFLAGS" "$BASE_DIR"
make -j4
cp -v libfreetyped.a "$BASE_DIR/lib/Linux/$ARCH/libfreetyped_fPIC.a"
