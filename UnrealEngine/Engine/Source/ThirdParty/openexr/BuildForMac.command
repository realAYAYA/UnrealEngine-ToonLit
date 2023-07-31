#!/bin/bash

set -e

OPENEXR_VERSION=3.1.5

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`
IMATH_CMAKE_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.3/Mac/lib/cmake/Imath"
ZLIB_LOCATION="$UE_THIRD_PARTY_LOCATION/zlib/v1.2.8"
ZLIB_INCLUDE_LOCATION="$ZLIB_LOCATION/include/Mac"
ZLIB_LIB_LOCATION="$ZLIB_LOCATION/lib/Mac"

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/openexr-$OPENEXR_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

# Specify all of the include/bin/lib directory variables so that CMake can
# compute relative paths correctly for the imported targets.
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR="Mac/bin"
INSTALL_LIB_DIR="Mac/lib"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/openexr-$OPENEXR_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

CXX_FLAGS="-fvisibility-ms-compat -fvisibility-inlines-hidden"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION"
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_LOCATION"
    -DZLIB_ROOT="$ZLIB_LIB_LOCATION"
    -DCMAKE_INSTALL_INCLUDEDIR="$INSTALL_INCLUDEDIR"
    -DCMAKE_INSTALL_BINDIR="$INSTALL_BIN_DIR"
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DBUILD_SHARED_LIBS=OFF
    -DOPENEXR_BUILD_TOOLS=OFF
    -DBUILD_TESTING=OFF
    -DOPENEXR_INSTALL_EXAMPLES=OFF
    -DOPENEXR_INSTALL_PKG_CONFIG=OFF
)

echo Configuring build for OpenEXR version $OPENEXR_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building OpenEXR for Debug...
cmake --build . --config Debug

echo Installing OpenEXR for Debug...
cmake --install . --config Debug

echo Building OpenEXR for Release...
cmake --build . --config Release

echo Installing OpenEXR for Release...
cmake --install . --config Release

popd

echo Done.
