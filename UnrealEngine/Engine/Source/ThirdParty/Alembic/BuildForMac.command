#!/bin/bash

set -e

ALEMBIC_VERSION=1.8.2

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`
IMATH_CMAKE_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.3/Mac/lib/cmake/Imath"

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/alembic-$ALEMBIC_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/alembic-$ALEMBIC_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/include"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"
INSTALL_LIB_DIR="Mac/lib"
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
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

CXX_FLAGS="-fvisibility-ms-compat -fvisibility-inlines-hidden"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION"
    -DALEMBIC_LIB_INSTALL_DIR="$INSTALL_LIB_LOCATION"
    -DConfigPackageLocation="$INSTALL_CMAKE_DIR"
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DUSE_BINARIES=OFF
    -DUSE_TESTS=OFF
    -DALEMBIC_ILMBASE_LINK_STATIC=ON
    -DALEMBIC_SHARED_LIBS=OFF
    -DCMAKE_DEBUG_POSTFIX=_d
)

echo Configuring build for Alembic version $ALEMBIC_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building Alembic for Debug...
cmake --build . --config Debug

echo Installing Alembic for Debug...
cmake --install . --config Debug

echo Building Alembic for Release...
cmake --build . --config Release

echo Installing Alembic for Release...
cmake --install . --config Release

popd

echo Done.
