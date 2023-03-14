#!/bin/bash

set -e

USD_VERSION=22.08

# This path may be adjusted to point to wherever the USD source is located.
# It is typically obtained by either downloading a zip/tarball of the source
# code, or more commonly by cloning the GitHub repository, e.g. for the
# current engine USD version:
#     git clone --branch v22.08 https://github.com/PixarAnimationStudios/USD.git USD_src
# Note that a small patch to the USD CMake build is currently necessary for
# the usdAbc plugin to require and link against Imath instead of OpenEXR:
#     git apply USD_v2208_usdAbc_Imath.patch
# Note also that this path may be emitted as part of USD error messages, so
# it is suggested that it not reveal any sensitive information.
SOURCE_LOCATION="/tmp/USD_src"

SCRIPT_DIR=`cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd`

UE_ENGINE_LOCATION=`cd $SCRIPT_DIR/../../../../../..; pwd`

UE_THIRD_PARTY_LOCATION="$UE_ENGINE_LOCATION/Source/ThirdParty"
TBB_LOCATION="$UE_THIRD_PARTY_LOCATION/Intel/TBB/IntelTBB-2019u8"
TBB_INCLUDE_LOCATION="$TBB_LOCATION/include"
TBB_LIB_LOCATION="$TBB_LOCATION/lib/Mac"
BOOST_LOCATION="$UE_THIRD_PARTY_LOCATION/Boost/boost-1_70_0"
BOOST_INCLUDE_LOCATION="$BOOST_LOCATION/include"
BOOST_LIB_LOCATION="$BOOST_LOCATION/lib/Mac"
IMATH_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.3"
IMATH_LIB_LOCATION="$IMATH_LOCATION/Mac"
IMATH_CMAKE_LOCATION="$IMATH_LIB_LOCATION/lib/cmake/Imath"
ALEMBIC_LOCATION="$UE_THIRD_PARTY_LOCATION/Alembic/Deploy/alembic-1.8.2"
ALEMBIC_INCLUDE_LOCATION="$ALEMBIC_LOCATION/include"
ALEMBIC_LIB_LOCATION="$ALEMBIC_LOCATION/Mac"

PYTHON_BINARIES_LOCATION="$UE_ENGINE_LOCATION/Binaries/ThirdParty/Python3/Mac"
PYTHON_EXECUTABLE_LOCATION="$PYTHON_BINARIES_LOCATION/bin/python3"
PYTHON_SOURCE_LOCATION="$UE_THIRD_PARTY_LOCATION/Python3/Mac"
PYTHON_INCLUDE_LOCATION="$PYTHON_SOURCE_LOCATION/include"
PYTHON_LIBRARY_LOCATION="$PYTHON_BINARIES_LOCATION/libpython3.9.dylib"

UE_MODULE_USD_LOCATION=$SCRIPT_DIR

BUILD_LOCATION="$UE_MODULE_USD_LOCATION/Intermediate"

# USD build products are written into a deployment directory and must then
# be manually copied from there into place.
INSTALL_LOCATION="$BUILD_LOCATION/Deploy/USD-$USD_VERSION"

rm -rf $BUILD_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DTBB_INCLUDE_DIR="$TBB_INCLUDE_LOCATION"
    -DTBB_LIBRARY="$TBB_LIB_LOCATION"
    -DBoost_NO_BOOST_CMAKE=ON
    -DBoost_NO_SYSTEM_PATHS=ON
    -DBOOST_INCLUDEDIR="$BOOST_INCLUDE_LOCATION"
    -DBOOST_LIBRARYDIR="$BOOST_LIB_LOCATION"
    -DPXR_USE_PYTHON_3=ON
    -DPYTHON_EXECUTABLE="$PYTHON_EXECUTABLE_LOCATION"
    -DPYTHON_INCLUDE_DIR="$PYTHON_INCLUDE_LOCATION"
    -DPYTHON_LIBRARY="$PYTHON_LIBRARY_LOCATION"
    -DPXR_BUILD_ALEMBIC_PLUGIN=ON
    -DPXR_ENABLE_HDF5_SUPPORT=OFF
    -DALEMBIC_INCLUDE_DIR="$ALEMBIC_INCLUDE_LOCATION"
    -DALEMBIC_DIR="$ALEMBIC_LIB_LOCATION"
    -DBUILD_SHARED_LIBS=ON
    -DPXR_BUILD_TESTS=OFF
    -DPXR_BUILD_EXAMPLES=OFF
    -DPXR_BUILD_TUTORIALS=OFF
    -DPXR_BUILD_USD_TOOLS=OFF
    -DPXR_BUILD_IMAGING=OFF
    -DPXR_BUILD_USD_IMAGING=OFF
    -DPXR_BUILD_USDVIEW=OFF
)

echo Configuring build for USD version $USD_VERSION...
cmake -G "Xcode" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building USD for Release...
cmake --build . --config Release -j8

echo Installing USD for Release...
cmake --install . --config Release

popd

echo Moving built-in USD plugins to UsdResources plugins directory...
INSTALL_LIB_LOCATION="$INSTALL_LOCATION/lib"
INSTALL_RESOURCES_LOCATION="$INSTALL_LOCATION/Resources/UsdResources/Mac"
INSTALL_RESOURCES_PLUGINS_LOCATION="$INSTALL_RESOURCES_LOCATION/plugins"
mkdir -p $INSTALL_RESOURCES_LOCATION
mv "$INSTALL_LIB_LOCATION/usd" "$INSTALL_RESOURCES_PLUGINS_LOCATION"

echo Moving USD plugin shared libraries to lib directory...
INSTALL_PLUGIN_LOCATION="$INSTALL_LOCATION/plugin"
INSTALL_PLUGIN_USD_LOCATION="$INSTALL_PLUGIN_LOCATION/usd"
mv $INSTALL_PLUGIN_USD_LOCATION/*.dylib "$INSTALL_LIB_LOCATION"

echo Removing top-level USD plugins plugInfo.json file...
rm -f "$INSTALL_PLUGIN_USD_LOCATION/plugInfo.json"

echo Moving UsdAbc plugin directory to UsdResources plugins directory
mv "$INSTALL_PLUGIN_USD_LOCATION/usdAbc" "$INSTALL_RESOURCES_PLUGINS_LOCATION"

rmdir "$INSTALL_PLUGIN_USD_LOCATION"
rmdir "$INSTALL_PLUGIN_LOCATION"

echo Removing CMake files...
rm -rf "$INSTALL_LOCATION/cmake"
rm -f $INSTALL_LOCATION/*.cmake

echo Removing Python .pyc files...
find "$INSTALL_LOCATION" -name "*.pyc" -delete

echo Removing pxr.Tf.testenv Python module...
rm -rf "$INSTALL_LOCATION/lib/python/pxr/Tf/testenv"

echo Moving Python modules to Content...
INSTALL_CONTENT_LOCATION="$INSTALL_LOCATION/Content/Python/Lib/Mac/site-packages"
mkdir -p "$INSTALL_CONTENT_LOCATION"
mv "$INSTALL_LOCATION/lib/python/pxr" "$INSTALL_CONTENT_LOCATION"
rmdir "$INSTALL_LOCATION/lib/python"

echo Cleaning @rpath entries for shared libraries...
for SHARED_LIB in `find $INSTALL_LOCATION -name '*.so' -o -name '*.dylib'`
do
    RPATHS_TO_DELETE=(
        $BOOST_LIB_LOCATION
        $PYTHON_BINARIES_LOCATION
        $INSTALL_LIB_LOCATION
        $INSTALL_PLUGIN_USD_LOCATION
    )

    OTOOL_OUTPUT=`otool -l $SHARED_LIB`

    for RPATH_TO_DELETE in ${RPATHS_TO_DELETE[@]}
    do
        if [[ $OTOOL_OUTPUT == *"path $RPATH_TO_DELETE"* ]]
        then
            install_name_tool -delete_rpath $RPATH_TO_DELETE $SHARED_LIB
        fi
    done

    # Also replace any architecture-specific libboost_python paths to use the
    # universal binary instead.
    install_name_tool -change @rpath/libboost_python39-mt-x64.dylib @rpath/libboost_python39-mt.dylib $SHARED_LIB
    install_name_tool -change @rpath/libboost_python39-mt-a64.dylib @rpath/libboost_python39-mt.dylib $SHARED_LIB
done

echo Done.
