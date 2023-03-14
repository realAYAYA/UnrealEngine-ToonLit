#!/bin/bash

set -e

OPENEXR_VERSION=3.1.5

ARCH_NAME=x86_64-unknown-linux-gnu

UE_ENGINE_LOCATION=`cd $(pwd)/../../..; pwd`

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`
IMATH_CMAKE_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.3/Unix/$ARCH_NAME/lib/cmake/Imath"
ZLIB_LOCATION="$UE_THIRD_PARTY_LOCATION/zlib/v1.2.8"
ZLIB_INCLUDE_LOCATION="$ZLIB_LOCATION/include/Unix/$ARCH_NAME"
ZLIB_LIB_LOCATION="$ZLIB_LOCATION/lib/Unix/$ARCH_NAME"

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/openexr-$OPENEXR_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

# Specify all of the include/bin/lib directory variables so that CMake can
# compute relative paths correctly for the imported targets.
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR="Unix/$ARCH_NAME/bin"
INSTALL_LIB_DIR="Unix/$ARCH_NAME/lib"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/openexr-$OPENEXR_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_LINUX_LOCATION="$INSTALL_LOCATION/Unix"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_LINUX_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

# Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
# that the toolchain is setup and verify that this name matches.
TOOLCHAIN_NAME=v20_clang-13.0.1-centos7

UE_TOOLCHAIN_LOCATION="$UE_ENGINE_LOCATION/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME/$ARCH_NAME"

C_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang"
CXX_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang++"

CXX_FLAGS="-fvisibility=hidden -fPIC -I$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/include/c++/v1"
CXX_LINKER="-nodefaultlibs -L$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/ -lc++ -lc++abi -lm -lc -lgcc_s -lgcc"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION"
    -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_LOCATION"
    -DZLIB_ROOT="$ZLIB_LIB_LOCATION"
    -DCMAKE_INSTALL_INCLUDEDIR="$INSTALL_INCLUDEDIR"
    -DCMAKE_INSTALL_BINDIR="$INSTALL_BIN_DIR"
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_EXE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_MODULE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_SHARED_LINKER_FLAGS="$CXX_LINKER"
    -DBUILD_SHARED_LIBS=OFF
    -DOPENEXR_BUILD_TOOLS=OFF
    -DBUILD_TESTING=OFF
    -DOPENEXR_INSTALL_EXAMPLES=OFF
    -DOPENEXR_INSTALL_PKG_CONFIG=OFF
)

NUM_CPU=`grep -c ^processor /proc/cpuinfo`

echo Configuring Debug build for OpenEXR version $OPENEXR_VERSION...
cmake -G "Unix Makefiles" $SOURCE_LOCATION -DCMAKE_BUILD_TYPE=Debug "${CMAKE_ARGS[@]}"

echo Building OpenEXR for Debug...
cmake --build . -j$NUM_CPU

echo Installing OpenEXR for Debug...
cmake --install .

# The Unix Makefiles generator does not support multiple configurations, so we
# need to re-configure for Release.
echo Configuring Release build for OpenEXR version $OPENEXR_VERSION...
cmake -G "Unix Makefiles" $SOURCE_LOCATION -DCMAKE_BUILD_TYPE=Release "${CMAKE_ARGS[@]}"

echo Building OpenEXR for Release...
cmake --build . -j$NUM_CPU

echo Installing OpenEXR for Release...
cmake --install .

popd

echo Done.
