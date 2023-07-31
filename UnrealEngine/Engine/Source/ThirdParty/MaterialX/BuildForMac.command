#!/bin/bash

set -e

MATERIALX_VERSION=1.38.5

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/MaterialX-$MATERIALX_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

INSTALL_INCLUDEDIR=include
INSTALL_LIB_DIR="Mac/lib"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/MaterialX-$MATERIALX_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DMATERIALX_INSTALL_INCLUDE_PATH="$INSTALL_INCLUDEDIR"
    -DMATERIALX_INSTALL_LIB_PATH="$INSTALL_LIB_DIR"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DMATERIALX_BUILD_TESTS=OFF
    -DMATERIALX_TEST_RENDER=OFF
    -DCMAKE_DEBUG_POSTFIX=_d
)

echo Configuring build for MaterialX version $MATERIALX_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building MaterialX for Debug...
cmake --build . --config Debug

echo Installing MaterialX for Debug...
cmake --install . --config Debug

echo Building MaterialX for Release...
cmake --build . --config Release

echo Installing MaterialX for Release...
cmake --install . --config Release

popd > /dev/null

echo Done.
