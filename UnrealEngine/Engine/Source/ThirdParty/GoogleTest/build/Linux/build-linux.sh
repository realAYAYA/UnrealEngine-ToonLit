#!/bin/sh
# Prerequisites:
#  clang++
#  cmake 3.5

#####################
# configuration
CONFIG=MinSizeRel
if [ -n "$1" ]
 then
	CONFIG=$1
fi

if [ -z $TARGET_ARCH ]; then
	TARGET_ARCH=x86_64-unknown-linux-gnu
fi

GTEST_SDK=$(pwd)/../google-test-source
OUTPUT_LIBS=$(pwd)/../../lib/Linux/$CONFIG/$TARGET_ARCH
OUTPUT_DIR=$(pwd)/Artifacts_$CONFIG
CXX_LIBS=$(pwd)/../../../Linux/LibCxx/lib/Linux/$TARGET_ARCH
CXX_INCLUDES=$(pwd)/../../../Linux/LibCxx/include

fPIC=""
if [ "$2" = "fPIC" ]; then
	fPIC="-fPIC"
	OUTPUT_LIBS=$(pwd)/../../lib/Linux/${CONFIG}_fPIC/$TARGET_ARCH
	OUTPUT_DIR=$(pwd)/Artifacts_${CONFIG}_fPIC
fi

echo "$OUTPUT_LIBS"

#####################
# create output directories
mkdir -p $OUTPUT_DIR
mkdir -p $OUTPUT_LIBS


#####################
# unpack source if needed
if [ ! -d "$GTEST_SDK" ]; then
	pushd $(pwd)/../
	bash uncompress_and_patch.sh googletest-release-*.zip
	popd
fi


#####################
# config cmake project
pushd $OUTPUT_DIR
cmake -DCMAKE_CXX_COMPILER="/usr/bin/clang++" -DCMAKE_CXX_FLAGS="-Qunused-arguments -std=c++11 -L$CXX_LIBS -I$CXX_INCLUDES -I$CXX_INCLUDES/c++/v1/ -lc++abi -stdlib=libc++ $fPIC" -DCMAKE_BUILD_TYPE=$CONFIG -D BUILD_SHARED_LIBS:BOOL=OFF $GTEST_SDK
popd


#####################
# compile project
pushd $OUTPUT_DIR 
make
popd


#####################
# remove existing binaries
rm -rfv $OUTPUT_LIBS/*


#####################
# copy new binaries
cp $OUTPUT_DIR/googlemock/gtest/*.a $OUTPUT_LIBS
cp $OUTPUT_DIR/googlemock/*.a $OUTPUT_LIBS
