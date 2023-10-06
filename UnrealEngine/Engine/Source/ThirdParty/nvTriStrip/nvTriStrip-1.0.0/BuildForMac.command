#!/bin/bash

set -e

NVTRISTRIP_VERSION=1.0.0

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/"
INSTALL_LOCATION="$UE_MODULE_LOCATION/Lib/Mac/"
BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

rm -rf $BUILD_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF
)

if [ "$BUILD_UNIVERSAL" = true ] ; then
    CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
fi

echo Configuring build for nvTriStrip version $NVTRISTRIP_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building nvTriStrip for Debug...
cmake --build . --config Debug

echo Installing nvTriStrip for Debug...
cmake --install . --config Debug

echo Building nvTriStrip for Release...
cmake --build . --config Release

echo Installing nvTriStrip for Release...
cmake --install . --config Release

cp "$BUILD_LOCATION/Release/libnvtristrip.a" "$INSTALL_LOCATION/libnvtristrip.a"
cp "$BUILD_LOCATION/Debug/libnvtristrip.a" "$INSTALL_LOCATION/libnvtristripd.a"

popd

echo Done.
