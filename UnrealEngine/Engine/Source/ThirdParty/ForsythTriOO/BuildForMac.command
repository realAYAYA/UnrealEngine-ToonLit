#!/bin/bash

set -e

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

echo Configuring build for ForsythTriOO...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building ForsythTriOO for Debug...
cmake --build . --config Debug

echo Installing ForsythTriOO for Debug...
cmake --install . --config Debug

echo Building ForsythTriOO for Release...
cmake --build . --config Release

echo Installing ForsythTriOO for Release...
cmake --install . --config Release

cp "$BUILD_LOCATION/Release/libForsythTriOptimizer.a" "$INSTALL_LOCATION/libForsythTriOptimizer.a"
cp "$BUILD_LOCATION/Debug/libForsythTriOptimizer.a" "$INSTALL_LOCATION/libForsythTriOptimizerd.a"

popd

echo Done.
