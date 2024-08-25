#!/bin/bash

set -e

LIBRARY_NAME="OpenSubdiv"
REPOSITORY_NAME="OpenSubdiv"

# Informational, for the usage message.
CURRENT_LIBRARY_VERSION=3.6.0

BUILD_SCRIPT_NAME="$(basename $BASH_SOURCE)"
BUILD_SCRIPT_DIR=`cd $(dirname "$BASH_SOURCE"); pwd`

UsageAndExit()
{
    echo "Build $LIBRARY_NAME for use with Unreal Engine on Mac"
    echo
    echo "Usage:"
    echo
    echo "    $BUILD_SCRIPT_NAME <$LIBRARY_NAME Version>"
    echo
    echo "Usage examples:"
    echo
    echo "    $BUILD_SCRIPT_NAME $CURRENT_LIBRARY_VERSION"
    echo "      -- Installs $LIBRARY_NAME version $CURRENT_LIBRARY_VERSION."
    echo
    exit 1
}

# Get version from arguments.
LIBRARY_VERSION=$1
if [ -z "$LIBRARY_VERSION" ]
then
    UsageAndExit
fi

UE_MODULE_LOCATION=`cd $BUILD_SCRIPT_DIR/../..; pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION/$REPOSITORY_NAME-$LIBRARY_VERSION"

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

# Specify all of the include/bin/lib directory variables so that CMake can
# compute relative paths correctly for the imported targets.
INSTALL_INCLUDEDIR=include
INSTALL_BIN_DIR="Mac/bin"
INSTALL_LIB_DIR="Mac/lib"

INSTALL_LOCATION="$UE_MODULE_LOCATION/Deploy/$REPOSITORY_NAME-$LIBRARY_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_MAC_LOCATION="$INSTALL_LOCATION/Mac"

rm -rf $BUILD_LOCATION
rm -rf $INSTALL_INCLUDE_LOCATION
rm -rf $INSTALL_MAC_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_BINDIR_BASE="$INSTALL_BIN_DIR"
    -DCMAKE_LIBDIR_BASE="$INSTALL_LIB_DIR"
    -DCMAKE_INSTALL_LIBDIR="$INSTALL_LIB_DIR"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DCMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS="-o linker-signed"
    -DCMAKE_DEBUG_POSTFIX=_d
    -DNO_REGRESSION=ON
    -DNO_TESTS=ON
    -DNO_DOC=ON
    -DNO_EXAMPLES=ON
    -DNO_TUTORIALS=ON
    -DNO_PTEX=ON
    -DNO_TBB=ON
    -DNO_OMP=ON
    -DNO_CUDA=ON
    -DNO_OPENCL=ON
    -DNO_DX=ON
    -DNO_GLEW=ON
    -DNO_GLFW=ON
    -DBUILD_SHARED_LIBS=OFF
    -DNO_MACOS_FRAMEWORK=ON
)

NUM_CPU=8

echo Configuring build for $LIBRARY_NAME version $LIBRARY_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building $LIBRARY_NAME for Debug...
cmake --build . --config Debug -j$NUM_CPU

echo Installing $LIBRARY_NAME for Debug...
cmake --install . --config Debug

echo Building $LIBRARY_NAME for Release...
cmake --build . --config Release -j$NUM_CPU

echo Installing $LIBRARY_NAME for Release...
cmake --install . --config Release

popd > /dev/null

# Remove the installed bin dir. The only thing in there will be stringify
# which we don't need.
echo Removing unused executables in bin directory...
if [ -d "$INSTALL_LOCATION/$INSTALL_BIN_DIR" ]
then
    rm -rf "$INSTALL_LOCATION/$INSTALL_BIN_DIR"
fi

echo Done.
