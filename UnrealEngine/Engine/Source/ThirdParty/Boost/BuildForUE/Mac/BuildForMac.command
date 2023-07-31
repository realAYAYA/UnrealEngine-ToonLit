#!/bin/bash

set -e

UsageAndExit()
{
    echo "Build Boost for use with Unreal Engine on Mac"
    echo
    echo "Usage:"
    echo
    echo "    BuildForMac.command <Boost Version> [<library name> [<library name> ...]]"
    echo
    echo "Usage examples:"
    echo
    echo "    BuildForMac.command 1.55.0"
    echo "      -- Installs Boost version 1.55.0 as header-only."
    echo
    echo "    BuildForMac.command 1.66.0 iostreams system thread"
    echo "      -- Builds and installs Boost version 1.66.0 with iostreams, system, and thread libraries."
    echo
    echo "    BuildForMac.command 1.72.0 all"
    echo "      -- Builds and installs Boost version 1.72.0 with all of its libraries."
    exit 1
}

# Set the following variable to 1 if you have already downloaded and extracted
# the Boost sources and you need to play around with the build configuration.
ALREADY_HAVE_SOURCES=0

BUILD_SCRIPT_DIRECTORY=`cd $(dirname "$0"); pwd`
UE_MODULE_LOCATION=`cd $BUILD_SCRIPT_DIRECTORY/../..; pwd`

UE_ENGINE_DIR=`cd $UE_MODULE_LOCATION/../../..; pwd`
PYTHON_EXECUTABLE_LOCATION="$UE_ENGINE_DIR/Binaries/ThirdParty/Python3/Mac/bin/python3"
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

INSTALL_INCLUDEDIR=include
INSTALL_LIB_DIR="lib/Mac"

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

    # Provide user config to specify Python configuration.
    BOOST_USER_CONFIG="$BUILD_SCRIPT_DIRECTORY/user-config.jam"

    MACOS_DEPLOYMENT_TARGET=10.9

    # Bootstrap before build.
    echo [`date +"%r"`] Bootstrapping Boost $BOOST_VERSION build...
    ./bootstrap.sh \
        --prefix=$INSTALL_LOCATION \
        --includedir=$INSTALL_INCLUDE_LOCATION \
        --libdir=$INSTALL_LIB_LOCATION \
        --with-toolset=$BOOST_TOOLSET \
        --with-python=$PYTHON_EXECUTABLE_LOCATION \
        --with-python-version=$PYTHON_VERSION
        cxxflags="-arch x86_64 -arch arm64" \
        cflags="-arch x86_64 -arch arm64" \
        linkflags="-arch x86_64 -arch arm64"

    for arch in "x86_64" "arm64"
    do
        echo [`date +"%r"`] Building Boost $BOOST_VERSION for architecture $arch...

        B2_ARCH="`echo $arch | sed 's/_*64//g'`"

        ./b2 \
            --prefix=$INSTALL_LOCATION \
            --includedir=$INSTALL_INCLUDE_LOCATION \
            --libdir=$INSTALL_LIB_LOCATION \
            -j8 \
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
            architecture=$B2_ARCH \
            cflags="-mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET -arch $arch" \
            cxxflags="-mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET -arch $arch" \
            mflags="-mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET -arch $arch" \
            mmflags="-mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET -arch $arch" \
            linkflags="-mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET -arch $arch" \
            install
    done

    echo [`date +"%r"`] Creating universal libraries for Boost $BOOST_VERSION...
    for INTEL_LIB in $INSTALL_LIB_LOCATION/*-mt-x64.dylib $INSTALL_LIB_LOCATION/*-mt-x64.a $INSTALL_LIB_LOCATION/*-mt-s-x64.a
    do
        ARM_LIB="`echo $INTEL_LIB | sed 's/x64/a64/g'`"
        UNIVERSAL_LIB="`echo $INTEL_LIB | sed 's/-x64//g'`"
        lipo -create \
            -arch x86_64 $INTEL_LIB \
            -arch arm64 $ARM_LIB \
            -output $UNIVERSAL_LIB
    done
fi

popd > /dev/null

popd > /dev/null

echo [`date +"%r"`] Boost $BOOST_VERSION installed to $INSTALL_LOCATION
echo [`date +"%r"`] Done.
