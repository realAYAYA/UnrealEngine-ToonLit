#!/bin/bash

set -e

SOURCE_VER="5.1.0"
SOURCE_NAME="metis-$SOURCE_VER"
SOURCE_PATH="http://glaros.dtc.umn.edu/gkhome/fetch/sw/metis/$SOURCE_NAME.tar.gz"

cd "../../"
if [ ! -f $SOURCE_NAME.tar.gz ]; then
	curl $SOURCE_PATH -o $SOURCE_NAME.tar.gz
	tar -xvf $SOURCE_NAME.tar.gz
fi

UE_THIRD_PARTY_DIR=`cd ".."; pwd`
BASE_DIR=`cd "$SOURCE_NAME"; pwd`

cd $BASE_DIR

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="../$SOURCE_VER/libmetis/Mac"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DGKLIB_PATH="./GKlib/"
)

cmake . "${CMAKE_ARGS[@]}"

echo Building libmetis for Debug...
cmake --build . --config Debug

echo Installing libmetis for Debug...
cmake --install . --config Debug

cp -v "../$SOURCE_VER/libmetis/Mac/lib/libmetis.a" "../$SOURCE_VER/libmetis/Mac/Debug/libmetis.a"

echo Building libmetis for Release...
cmake --build . --config Release

echo Installing libmetis for Release...
cmake --install . --config Release

cp -v "../$SOURCE_VER/libmetis/Mac/lib/libmetis.a" "../$SOURCE_VER/libmetis/Mac/Release/libmetis.a"

rm -rf "../$SOURCE_VER/libmetis/Mac/bin/"
rm -rf "../$SOURCE_VER/libmetis/Mac/include/"
rm -rf "../$SOURCE_VER/libmetis/Mac/lib/"

echo Done.
