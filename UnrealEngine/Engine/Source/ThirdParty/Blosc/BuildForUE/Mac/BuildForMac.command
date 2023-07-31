#!/bin/bash

set -e

UsageAndExit()
{
    echo "Build Blosc for use with Unreal Engine on Mac"
    echo
    echo "Usage:"
    echo
    echo "    BuildForMac.command <Blosc Version>"
    echo
    echo "Usage examples:"
    echo
    echo "    BuildForMac.command 1.21.0"
    echo "      -- Installs Blosc version 1.21.0."
    exit 1
}

# Get version from arguments.
BLOSC_VERSION=$1
if [ -z "$BLOSC_VERSION" ]
then
    UsageAndExit
fi

BUILD_SCRIPT_LOCATION=`cd $(dirname "$0"); pwd`
UE_THIRD_PARTY_LOCATION=`cd $BUILD_SCRIPT_LOCATION/../../..; pwd`

ZLIB_LOCATION="$UE_THIRD_PARTY_LOCATION/zlib/v1.2.8"
ZLIB_INCLUDE_LOCATION="$ZLIB_LOCATION/include/Mac"
ZLIB_LIB_LOCATION="$ZLIB_LOCATION/lib/Mac/libz.a"

UE_MODULE_LOCATION=`cd $BUILD_SCRIPT_LOCATION/../..; pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/c-blosc-$BLOSC_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/c-blosc-$BLOSC_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/include"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

# Note that we patch the source for the version of LZ4 that is bundled with
# Blosc to add a prefix to all of its functions. This ensures that the symbol
# names do not collide with the version(s) of LZ4 that are embedded in the
# engine.

# Copy the source into the build directory so that we can apply patches.
BUILD_SOURCE_LOCATION="$BUILD_LOCATION/c-blosc-$BLOSC_VERSION"

cp -r $SOURCE_LOCATION $BUILD_SOURCE_LOCATION

pushd $BUILD_SOURCE_LOCATION > /dev/null
git apply $UE_MODULE_LOCATION/Blosc_v1.21.0_LZ4_PREFIX.patch
popd > /dev/null

C_FLAGS="-DLZ4_PREFIX=BLOSC_"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DPREFER_EXTERNAL_ZLIB=ON
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_LOCATION"
    -DZLIB_LIBRARY="$ZLIB_LIB_LOCATION"
    -DCMAKE_C_FLAGS="$C_FLAGS"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DDEACTIVATE_SSE2=ON
    -DDEACTIVATE_AVX2=ON
    -DBUILD_SHARED=OFF
    -DBUILD_TESTS=OFF
    -DBUILD_FUZZERS=OFF
    -DBUILD_BENCHMARKS=OFF
    -DCMAKE_DEBUG_POSTFIX=_d
)

echo Configuring build for Blosc version $BLOSC_VERSION...
cmake -G "Xcode" $BUILD_SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building Blosc for Debug...
cmake --build . --config Debug -j8

echo Installing Blosc for Debug...
cmake --install . --config Debug

echo Building Blosc for Release...
cmake --build . --config Release -j8

echo Installing Blosc for Release...
cmake --install . --config Release

popd > /dev/null

echo Removing pkgconfig files...
rm -rf "$INSTALL_LOCATION/lib/pkgconfig"

echo Moving lib directory into place...
INSTALL_LIB_LOCATION="$INSTALL_LOCATION/Mac"
mkdir $INSTALL_LIB_LOCATION
mv "$INSTALL_LOCATION/lib" "$INSTALL_LIB_LOCATION"

echo Done.
