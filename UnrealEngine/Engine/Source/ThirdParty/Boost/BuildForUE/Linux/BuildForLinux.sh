#!/bin/bash

set -e

UsageAndExit()
{
    echo "Build Boost for use with Unreal Engine on Linux"
    echo
    echo "Usage:"
    echo
    echo "    BuildForLinux.sh <Boost Version> [<library name> [<library name> ...]]"
    echo
    echo "Usage examples:"
    echo
    echo "    BuildForLinux.command 1.55.0"
    echo "      -- Installs Boost version 1.55.0 as header-only."
    echo
    echo "    BuildForLinux.command 1.66.0 iostreams system thread"
    echo "      -- Builds and installs Boost version 1.66.0 with iostreams, system, and thread libraries."
    echo
    echo "    BuildForLinux.command 1.72.0 all"
    echo "      -- Builds and installs Boost version 1.72.0 with all of its libraries."
    exit 1
}

# Set the following variable to 1 if you have already downloaded and extracted
# the Boost sources and you need to play around with the build configuration.
ALREADY_HAVE_SOURCES=0

BUILD_SCRIPT_DIRECTORY=`cd $(dirname "$0"); pwd`
UE_MODULE_LOCATION=`cd $BUILD_SCRIPT_DIRECTORY/../..; pwd`
UE_THIRD_PARTY_LOCATION=`cd $UE_MODULE_LOCATION/..; pwd`

UE_ENGINE_DIR=`cd $UE_MODULE_LOCATION/../../..; pwd`
PYTHON_EXECUTABLE_LOCATION="$UE_ENGINE_DIR/Binaries/ThirdParty/Python3/Linux/bin/python3"
PYTHON_VERSION=3.9

# Get version from arguments.
BOOST_VERSION=$1
if [ -z "$BOOST_VERSION" ]
then
    UsageAndExit
fi

shift

BOOST_UNDERSCORE_VERSION="`echo $BOOST_VERSION | sed s/\\\./_/g`"

ARG_LIBRARIES=()
BOOST_WITH_LIBRARIES=""

for arg in "$@"
do
    ARG_LIBRARIES+=("$arg")
    BOOST_WITH_LIBRARIES="$BOOST_WITH_LIBRARIES --with-$arg"
done

echo [`date +"%r"`] Building Boost for Unreal Engine
echo "    Version  : $BOOST_VERSION"
if [ ${#ARG_LIBRARIES[@]} -eq 0 ]
then
    echo "    Libraries: <headers-only>"
else
    echo "    Libraries: ${ARG_LIBRARIES[*]}"
fi

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

ARCH_NAME=x86_64-unknown-linux-gnu

INSTALL_INCLUDEDIR=include
INSTALL_LIB_DIR="lib/Unix/$ARCH_NAME"

INSTALL_LOCATION="$UE_MODULE_LOCATION/boost-$BOOST_UNDERSCORE_VERSION"
INSTALL_INCLUDE_LOCATION="$INSTALL_LOCATION/$INSTALL_INCLUDEDIR"
INSTALL_LIB_LOCATION="$INSTALL_LOCATION/$INSTALL_LIB_DIR"

BOOST_VERSION_FILENAME="boost_$BOOST_UNDERSCORE_VERSION"

if [ $ALREADY_HAVE_SOURCES -eq 0 ]
then
    rm -rf $BUILD_LOCATION
    mkdir $BUILD_LOCATION
else
    echo Expecting sources to already be available at $BUILD_LOCATION/$BOOST_VERSION_FILENAME
fi

pushd $BUILD_LOCATION > /dev/null

if [ $ALREADY_HAVE_SOURCES -eq 0 ]
then
    # Download Boost source file.
    BOOST_SOURCE_FILE="$BOOST_VERSION_FILENAME.tar.gz"
    BOOST_URL="https://boostorg.jfrog.io/artifactory/main/release/$BOOST_VERSION/source/$BOOST_SOURCE_FILE"

    echo [`date +"%r"`] Downloading $BOOST_URL...
    curl -# -L -o $BOOST_SOURCE_FILE $BOOST_URL

    # Extract Boost source file.
    echo
    echo [`date +"%r"`] Extracting $BOOST_SOURCE_FILE...
    tar -xf $BOOST_SOURCE_FILE
fi

pushd $BOOST_VERSION_FILENAME > /dev/null

if [ ${#ARG_LIBRARIES[@]} -eq 0 ]
then
    # No libraries requested. Just copy header files.
    echo [`date +"%r"`] Copying header files...
    
    BOOST_HEADERS_DIRECTORY_NAME=boost

    mkdir -p $INSTALL_INCLUDE_LOCATION

    cp -rp $BOOST_HEADERS_DIRECTORY_NAME $INSTALL_INCLUDE_LOCATION
else
    # Build and install with libraries.
    echo [`date +"%r"`] Building Boost libraries...

    # Set tool set to current UE tool set.
    BOOST_TOOLSET=clang

    # Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
    # that the toolchain is setup and verify that the path where it installed
    # the compiler matches the path specified in the user-config.jam file.

    # Provide user config to specify compiler and Python configuration.
    BOOST_USER_CONFIG="$BUILD_SCRIPT_DIRECTORY/user-config.jam"

    CXX_FLAGS="-fPIC -I$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/include/c++/v1"
    CXX_LINKER="-nodefaultlibs -L$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/ -lc++ -lc++abi -lm -lc -lgcc_s -lgcc"

    # Bootstrap before build.
    echo [`date +"%r"`] Bootstrapping Boost $BOOST_VERSION build...
    ./bootstrap.sh \
        --prefix=$INSTALL_LOCATION \
        --includedir=$INSTALL_INCLUDE_LOCATION \
        --libdir=$INSTALL_LIB_LOCATION \
        --with-toolset=$BOOST_TOOLSET \
        --with-python=$PYTHON_EXECUTABLE_LOCATION \
        --with-python-version=$PYTHON_VERSION
        cxxflags="$CXX_FLAGS" \
        cflags="$CXX_FLAGS" \
        linkflags="$CXX_LINKER"

    echo [`date +"%r"`] Building Boost $BOOST_VERSION...

    NUM_CPU=`grep -c ^processor /proc/cpuinfo`

    ./b2 \
        --prefix=$INSTALL_LOCATION \
        --includedir=$INSTALL_INCLUDE_LOCATION \
        --libdir=$INSTALL_LIB_LOCATION \
        -j$NUM_CPU \
        address-model=64 \
        threading=multi \
        variant=release \
        $BOOST_WITH_LIBRARIES \
        --user-config=$BOOST_USER_CONFIG \
        --hash \
        --build-type=complete \
        --layout=tagged \
        --debug-configuration \
        toolset=$BOOST_TOOLSET \
        cflags="$CXX_FLAGS" \
        cxxflags="$CXX_FLAGS" \
        linkflags="$CXX_LINKER" \
        install

    echo [`date +"%r"`] Converting installed Boost library symlinks to files...
    pushd $INSTALL_LIB_LOCATION > /dev/null
    for SYMLINKED_LIB in `find . -type l`
    do
        cp --remove-destination `readlink $SYMLINKED_LIB` $SYMLINKED_LIB
    done
    popd > /dev/null
fi

popd > /dev/null

popd > /dev/null

echo [`date +"%r"`] Boost $BOOST_VERSION installed to $INSTALL_LOCATION
echo [`date +"%r"`] Done.
