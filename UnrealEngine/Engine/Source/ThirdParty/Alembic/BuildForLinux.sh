#!/bin/bash

set -e

ALEMBIC_VERSION=1.8.2

ARCH_NAME=x86_64-unknown-linux-gnu

UE_ENGINE_LOCATION=`cd $(pwd)/../../..; pwd`

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`
IMATH_CMAKE_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.3/Unix/$ARCH_NAME/lib/cmake/Imath"

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/alembic-$ALEMBIC_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/alembic-$ALEMBIC_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/include"
INSTALL_LINUX_LOCATION="$INSTALL_LOCATION/Unix"
INSTALL_LIB_DIR="Unix/$ARCH_NAME/lib"
# The Alembic build is setup incorrectly such that relative install paths
# land in the build tree rather than the install tree. To make sure the
# library is installed in the correct location, we use a full path. Doing so
# prevents CMake from computing the correct import prefix though, so the
# resulting config files include absolute paths that we don't want. We won't
# really miss having these CMake files since we are unlikely to build
# anything on top of Alembic using CMake, so we use a relative path for those
# and let them disappear when the build tree in "Intermediate" is cleaned.
INSTALL_LIB_LOCATION="$INSTALL_LOCATION/$INSTALL_LIB_DIR"
INSTALL_CMAKE_DIR="$INSTALL_LIB_DIR/cmake/Alembic"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_LINUX_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

# Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
# that the toolchain is setup and verify that this name matches.
TOOLCHAIN_NAME=v19_clang-11.0.1-centos7

UE_TOOLCHAIN_LOCATION="$UE_ENGINE_LOCATION/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME/$ARCH_NAME"

C_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang"
CXX_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang++"

CXX_FLAGS="-fvisibility=hidden -fPIC -I$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/include/c++/v1"
CXX_LINKER="-nodefaultlibs -L$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/ -lc++ -lc++abi -lm -lc -lgcc_s -lgcc"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION"
    -DALEMBIC_LIB_INSTALL_DIR="$INSTALL_LIB_LOCATION"
    -DConfigPackageLocation="$INSTALL_CMAKE_DIR"
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_EXE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_MODULE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_SHARED_LINKER_FLAGS="$CXX_LINKER"
    -DUSE_BINARIES=OFF
    -DUSE_TESTS=OFF
    -DALEMBIC_ILMBASE_LINK_STATIC=ON
    -DALEMBIC_SHARED_LIBS=OFF
    -DCMAKE_DEBUG_POSTFIX=_d
)

NUM_CPU=`grep -c ^processor /proc/cpuinfo`

echo Configuring Debug build for Alembic version $ALEMBIC_VERSION...
cmake -G "Unix Makefiles" $SOURCE_LOCATION -DCMAKE_BUILD_TYPE=Debug "${CMAKE_ARGS[@]}"

echo Building Alembic for Debug...
cmake --build . -j$NUM_CPU

echo Installing Alembic for Debug...
cmake --install .

# The Unix Makefiles generator does not support multiple configurations, so we
# need to re-configure for Release.
echo Configuring Release build for Alembic version $ALEMBIC_VERSION...
cmake -G "Unix Makefiles" $SOURCE_LOCATION -DCMAKE_BUILD_TYPE=Release "${CMAKE_ARGS[@]}"

echo Building Alembic for Release...
cmake --build . -j$NUM_CPU

echo Installing Alembic for Release...
cmake --install .

popd

echo Done.
