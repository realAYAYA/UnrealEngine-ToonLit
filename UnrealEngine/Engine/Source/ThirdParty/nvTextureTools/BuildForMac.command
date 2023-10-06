#!/bin/bash

set -e

NVTT_VERSION=2.0.8

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/nvTextureTools-$NVTT_VERSION/src"
SQUISH_SOURCE_LIB_LOCATION="$UE_MODULE_LOCATION/nvTextureTools-$NVTT_VERSION/lib/Mac"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

# Specify all of the include/bin/lib directory variables so that CMake can
# compute relative paths correctly for the imported targets.
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR="Mac/"
INSTALL_LIB_DIR="Mac/"

INSTALL_LOCATION="$UE_THIRD_PARTY_LOCATION/../../Binaries/ThirdParty/nvTextureTools/Mac"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_MACOSX_RPATH=TRUE
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DNVTT_SHARED=TRUE
)

if [ "$BUILD_UNIVERSAL" = true ] ; then
    CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
fi

SQUISH_SOURCE_LOCATION="$SOURCE_LOCATION/src/nvtt/squish"

echo Building Squish for Release...
xcodebuild clean build -scheme=squish -project $SQUISH_SOURCE_LOCATION/squish.xcodeproj -configuration Release
cp $SQUISH_SOURCE_LOCATION/build/Release/libsquish.a $SQUISH_SOURCE_LIB_LOCATION/

echo Configuring build for NVTT version $NVTT_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building NVTT for Debug...
cmake --build . --config Debug

echo Installing NVTT for Debug...
cmake --install . --config Debug

echo Building NVTT for Release...
cmake --build . --config Release

echo Installing NVTT for Release...
cmake --install . --config Release

cp -a $INSTALL_LOCATION"/lib/." $INSTALL_LOCATION

rm -rf $INSTALL_LOCATION"/lib"
rm -rf $INSTALL_LOCATION"/bin"
rm -rf $INSTALL_LOCATION"/include"

popd

echo Done.
