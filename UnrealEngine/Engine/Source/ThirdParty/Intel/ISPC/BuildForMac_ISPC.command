#!/bin/bash

#
# This script is configured based on the following environment variables:
#    ANDROID_NDK     - If present, it needs to point to the root directory of the Android NDK.
#                      In such case, cross compilation for the Android platform will be enabled.
#                      Typically, the path will be like:
#                      /users/<user>/library/Android/sdk/ndk/<version>
#

set -e

ISPC_VERSION=1.21.0

BUILD_UNIVERSAL=true

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`

UE_MODULE_LOCATION=`pwd`

ISPC_INSTALL_LOCATION="$UE_MODULE_LOCATION/ispc/"
ISPC_BUILD_LOCATION="$ISPC_INSTALL_LOCATION/build/"
LLVM_INSTALL_LOCATION="$UE_MODULE_LOCATION/llvm/"

export PATH=$PATH:"$LLVM_INSTALL_LOCATION/bin":"$ISPC_INSTALL_LOCATION/bin"

ISPC_SOURCE_LOCATION="$UE_MODULE_LOCATION/ispc-$ISPC_VERSION/"

rm -rf $ISPC_INSTALL_LOCATION
rm -rf $ISPC_BUILD_LOCATION

mkdir $ISPC_INSTALL_LOCATION
mkdir $ISPC_BUILD_LOCATION

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$ISPC_INSTALL_LOCATION"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"
    -DCMAKE_BUILD_TYPE=Release
    -DARM_ENABLED=ON
    -DWASM_ENABLED=OFF
    -DGENX_ENABLED=OFF
    -DISPC_INCLUDE_EXAMPLES=OFF
    -DISPC_INCLUDE_DPCPP_EXAMPLES=OFF
    -DISPC_INCLUDE_TESTS=OFF
    -DISPC_INCLUDE_BENCHMARKS=OFF
    -DISPC_INCLUDE_UTILS=OFF
    -DISPC_PREPARE_PACKAGE=OFF
    -DISPC_CROSS=ON
    -DISPC_WINDOWS_TARGET=OFF
    -DISPC_LINUX_TARGET=OFF
    -DISPC_FREEBSD_TARGET=OFF
    -DISPC_MACOS_TARGET=ON
    -DISPC_IOS_TARGET=ON
    -DISPC_PS_TARGET=OFF
)

if [ "$ANDROID_NDK" != "" ] ; then
    CMAKE_ARGS+=(-DISPC_ANDROID_TARGET=ON)
    CMAKE_ARGS+=(-DISPC_ANDROID_NDK_PATH=$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-x86_64)
else
    CMAKE_ARGS+=(-DISPC_ANDROID_TARGET=OFF)
fi

if [ "$BUILD_UNIVERSAL" = true ] ; then
    CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
    CMAKE_ARGS+=(-DISPC_MACOS_ARM_TARGET=ON)
    CMAKE_ARGS+=(-DX86_ENABLED=ON)
fi

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

rm -rf $BUILD_LOCATION
mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

echo Configuring build for ISPC version $ISPC_VERSION...
cmake -G "Xcode" $ISPC_SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building ISPC for Release...
cmake --build . --config Release --parallel 8

echo Installing ISPC for Release...
cmake --install . --config Release

echo Copying ISPC into the bin directory...
cp "$ISPC_INSTALL_LOCATION/bin/ispc" "$UE_MODULE_LOCATION/bin/Mac/ispc"

popd

echo Done.
