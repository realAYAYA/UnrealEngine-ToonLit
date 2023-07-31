#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

PATH=$PATH:/Applications/CMake.app/Contents/bin/:/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin

CUR_PATH=`dirname $0`
PATH_TO_CMAKE_FILE="$CUR_PATH/.."
PATH_TO_CMAKE_FILE=`cd "$PATH_TO_CMAKE_FILE"; pwd`
echo $PATH_TO_CMAKE_FILE

OUTPUT_DIR=$CUR_PATH/../../lib/IOS
mkdir -p $OUTPUT_DIR
OUTPUT_DIR=`cd "$OUTPUT_DIR"; pwd`
echo $OUTPUT_DIR

# Temporary build directories (used as working directories when running CMake)
MAKE_PATH="$CUR_PATH/Build"
rm -rf $MAKE_PATH
mkdir -p $MAKE_PATH
MAKE_PATH=`cd "$MAKE_PATH"; pwd`
echo $MAKE_PATH

PATH_IOS_TOOLCHAIN=$CUR_PATH
PATH_IOS_TOOLCHAIN=`cd "$PATH_IOS_TOOLCHAIN"; pwd`
echo PATH_IOS_TOOLCHAIN=$PATH_IOS_TOOLCHAIN

# Setup paths for debug and release
MAKE_PATH_DEBUG=$MAKE_PATH/Debug
MAKE_PATH_RELEASE=$MAKE_PATH/Release
rm -rf $MAKE_PATH_DEBUG
mkdir -p $MAKE_PATH_DEBUG
rm -rf $MAKE_PATH_RELEASE
mkdir -p $MAKE_PATH_RELEASE
MAKE_PATH_DEBUG=`cd "$MAKE_PATH_DEBUG"; pwd`
MAKE_PATH_RELEASE=`cd "$MAKE_PATH_RELEASE"; pwd`
echo $MAKE_PATH_DEBUG
echo $MAKE_PATH_RELEASE  

CXXFLAGS="-std=c++11"

echo "Generating ICU(Debug) makefile..."
cd $MAKE_PATH_DEBUG
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$PATH_IOS_TOOLCHAIN/iOS.cmake -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_C_COMPILER="/usr/bin/clang" -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_IOS_DEPLOYMENT_TARGET=10.0 $PATH_TO_CMAKE_FILE 

echo "Building ICU(Debug)..."
make -j4

#Moving file to expected lib directory
DEBUG_OUTPUT_PATH=$OUTPUT_DIR/Debug
rm -rf "$DEBUG_OUTPUT_PATH"
mkdir -p "$DEBUG_OUTPUT_PATH"
mv -v "$MAKE_PATH_DEBUG/../libicu.a" "$DEBUG_OUTPUT_PATH/libicu.a"


echo "Generating ICU(Release) makefile..."
cd $MAKE_PATH_RELEASE
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$PATH_IOS_TOOLCHAIN/iOS.cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_C_COMPILER="/usr/bin/clang" -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_IOS_DEPLOYMENT_TARGET=10.0 $PATH_TO_CMAKE_FILE

echo "Building ICU(Release)..."
make -j4

#Moving file to expected lib directory
RELEASE_OUTPUT_PATH=$OUTPUT_DIR/Release
rm -rf "$RELEASE_OUTPUT_PATH"
mkdir -p "$RELEASE_OUTPUT_PATH"
Mv -v "$MAKE_PATH_RELEASE/../libicu.a" "$RELEASE_OUTPUT_PATH/libicu.a"



#rm -rf $MAKE_PATH
