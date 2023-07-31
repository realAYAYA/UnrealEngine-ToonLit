#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

PATH=$PATH:/Applications/CMake.app/Contents/bin/:/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin

CUR_PATH=`dirname $0`
PATH_TO_CMAKE_FILE="$CUR_PATH/../.."
PATH_TO_CMAKE_FILE=`cd "$PATH_TO_CMAKE_FILE"; pwd`
echo $PATH_TO_CMAKE_FILE

OUTPUT_DIR=$CUR_PATH/../../lib/IOS
mkdir -p $OUTPUT_DIR
OUTPUT_DIR=`cd "$OUTPUT_DIR"; pwd`
echo $OUTPUT_DIR

UE_THIRD_PARTY_DIR=`cd "$CUR_PATH/../../../.."; pwd`
LIB_PNG_PATH="$UE_THIRD_PARTY_DIR/libPNG/libPNG-1.5.2"
ZLIB_PATH="$UE_THIRD_PARTY_DIR/zlib/zlib-1.2.5/Inc"

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
MAKE_PATH_SIM=$MAKE_PATH/Simulator
#rm -rf $MAKE_PATH_DEBUG
mkdir -p $MAKE_PATH_DEBUG
#rm -rf $MAKE_PATH_RELEASE
mkdir -p $MAKE_PATH_RELEASE
#rm -rf $MAKE_PATH_SIM
mkdir -p $MAKE_PATH_SIM 
MAKE_PATH_DEBUG=`cd "$MAKE_PATH_DEBUG"; pwd`
MAKE_PATH_RELEASE=`cd "$MAKE_PATH_RELEASE"; pwd`
MAKE_PATH_SIM=`cd "$MAKE_PATH_SIM"; pwd`

CXXFLAGS="-std=c++11"


echo "Generating FreeType2(Debug) makefile..."
cd $MAKE_PATH_DEBUG
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$PATH_IOS_TOOLCHAIN/iOS.cmake -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_PROJECT_INCLUDE=$PATH_IOS_TOOLCHAIN/TPS.cmake -DTPS_INCLUDE_PATHS="$LIB_PNG_PATH;$ZLIB_PATH" -DCMAKE_C_COMPILER="/usr/bin/clang" -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_IOS_DEPLOYMENT_TARGET=10.0 -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE $PATH_TO_CMAKE_FILE

echo "Building FreeType2(Debug)..."
make -j4

#Moving file to expected lib directory
DEBUG_OUTPUT_PATH=$OUTPUT_DIR/Debug
rm -rf "$DEBUG_OUTPUT_PATH"
mkdir -p "$DEBUG_OUTPUT_PATH"
mv -v "$MAKE_PATH_DEBUG/libfreetyped.a" "$DEBUG_OUTPUT_PATH/libfreetyped.a"


echo "Generating FreeType2(Release) makefile..."
cd $MAKE_PATH_RELEASE
cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$PATH_IOS_TOOLCHAIN/iOS.cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_PROJECT_INCLUDE=$PATH_IOS_TOOLCHAIN/TPS.cmake -DTPS_INCLUDE_PATHS="$LIB_PNG_PATH;$ZLIB_PATH" -DCMAKE_C_COMPILER="/usr/bin/clang" -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_IOS_DEPLOYMENT_TARGET=10.0 -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE $PATH_TO_CMAKE_FILE

echo "Building FreeType2(Release)..."
make -j4

#Moving file to expected lib directory
RELEASE_OUTPUT_PATH=$OUTPUT_DIR/Release
rm -rf "$RELEASE_OUTPUT_PATH"
mkdir -p "$RELEASE_OUTPUT_PATH"
mv -v "$MAKE_PATH_RELEASE/libfreetype.a" "$RELEASE_OUTPUT_PATH/libfreetype.a"


#echo "Generating FreeType2(Simulator) makefile..."
#cd $MAKE_PATH_SIM
#cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$PATH_IOS_TOOLCHAIN/iOS.cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_C_COMPILER="/usr/bin/clang" -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCMAKE_IOS_DEPLOYMENT_TARGET=10.0 -DIOS_PLATFORM=SIMULATOR -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE $PATH_TO_CMAKE_FILE

#echo "Building FreeType2(Simulator)..."
#make -j4

#Moving file to expected lib directory
#SIM_OUTPUT_PATH=$OUTPUT_DIR/Simulator
#rm -rf "$SIM_OUTPUT_PATH"
#mkdir -p "$SIM_OUTPUT_PATH"
#mv -v "$MAKE_PATH_SIM/libfreetype.a" "$SIM_OUTPUT_PATH/libfreetype.a"
