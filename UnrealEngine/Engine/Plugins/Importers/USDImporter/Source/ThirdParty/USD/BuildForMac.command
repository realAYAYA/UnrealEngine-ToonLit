#!/bin/bash

set -e

OPENUSD_VERSION=24.03

# This path may be adjusted to point to wherever the OpenUSD source is located.
# It is typically obtained by either downloading a zip/tarball of the source
# code, or more commonly by cloning the GitHub repository, e.g. for the
# current engine OpenUSD version:
#     git clone --branch v24.03 https://github.com/PixarAnimationStudios/OpenUSD.git OpenUSD_src
# We apply a patch for the usdMtlx plugin to ensure that we do not
# bake a hard-coded path to the MaterialX standard data libraries into the
# built plugin:
#     git apply OpenUSD_v2403_usdMtlx_undef_stdlib_dir.patch
# We apply a patch to explicitly declare, define, and export a destructor for
# SdfAssetPaths so that allocations of its member strings can be tracked and
# deallocated using the correct deallocator:
#     git apply OpenUSD_v2403_explicit_SdfAssetPath_dtor.patch
# We apply a patch to switch between two alternative set of macros in the Tf
# library based on whether we're compiling with MSVC *and* whether its
# "traditional" preprocessor is being used, not just whether we're using
# MSVC or not:
#     git apply OpenUSD_v2403_msvc_preprocessor_version_handling.patch
# Note also that this path may be emitted as part of OpenUSD error messages, so
# it is suggested that it not reveal any sensitive information.
OPENUSD_SOURCE_LOCATION="/tmp/OpenUSD_src"

SCRIPT_DIR=`cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd`

UE_ENGINE_LOCATION=`cd $SCRIPT_DIR/../../../../../..; pwd`

UE_THIRD_PARTY_LOCATION="$UE_ENGINE_LOCATION/Source/ThirdParty"
TBB_LOCATION="$UE_THIRD_PARTY_LOCATION/Intel/TBB/IntelTBB-2019u8"
TBB_INCLUDE_LOCATION="$TBB_LOCATION/include"
TBB_LIB_LOCATION="$TBB_LOCATION/lib/Mac"
BOOST_LOCATION="$UE_THIRD_PARTY_LOCATION/Boost/boost-1_82_0"
BOOST_INCLUDE_LOCATION="$BOOST_LOCATION/include"
BOOST_LIB_LOCATION="$BOOST_LOCATION/lib/Mac"
IMATH_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.9"
IMATH_LIB_LOCATION="$IMATH_LOCATION/Mac"
IMATH_CMAKE_LOCATION="$IMATH_LIB_LOCATION/lib/cmake/Imath"
OPENSUBDIV_LOCATION="$UE_THIRD_PARTY_LOCATION/OpenSubdiv/Deploy/OpenSubdiv-3.6.0"
OPENSUBDIV_INCLUDE_DIR="$OPENSUBDIV_LOCATION/include"
OPENSUBDIV_LIB_LOCATION="$OPENSUBDIV_LOCATION/Mac/lib"
ALEMBIC_LOCATION="$UE_THIRD_PARTY_LOCATION/Alembic/Deploy/alembic-1.8.6"
ALEMBIC_INCLUDE_LOCATION="$ALEMBIC_LOCATION/include"
ALEMBIC_LIB_LOCATION="$ALEMBIC_LOCATION/Mac"
MATERIALX_LOCATION="$UE_THIRD_PARTY_LOCATION/MaterialX/Deploy/MaterialX-1.38.5"
MATERIALX_LIB_LOCATION="$MATERIALX_LOCATION/Mac/lib"
MATERIALX_CMAKE_LOCATION="$MATERIALX_LIB_LOCATION/cmake/MaterialX"

PYTHON_BINARIES_LOCATION="$UE_ENGINE_LOCATION/Binaries/ThirdParty/Python3/Mac"
PYTHON_EXECUTABLE_LOCATION="$PYTHON_BINARIES_LOCATION/bin/python3"
PYTHON_SOURCE_LOCATION="$UE_THIRD_PARTY_LOCATION/Python3/Mac"
PYTHON_INCLUDE_LOCATION="$PYTHON_SOURCE_LOCATION/include"
PYTHON_LIBRARY_LOCATION="$PYTHON_BINARIES_LOCATION/libpython3.11.dylib"

UE_MODULE_USD_LOCATION=$SCRIPT_DIR

BUILD_LOCATION="$UE_MODULE_USD_LOCATION/Intermediate"

# OpenUSD build products are written into a deployment directory and must then
# be manually copied from there into place.
INSTALL_LOCATION="$BUILD_LOCATION/Deploy/OpenUSD-$OPENUSD_VERSION"

rm -rf $BUILD_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION;$MATERIALX_CMAKE_LOCATION"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    -DTBB_INCLUDE_DIR="$TBB_INCLUDE_LOCATION"
    -DTBB_LIBRARY="$TBB_LIB_LOCATION"
    -DBoost_NO_BOOST_CMAKE=ON
    -DBoost_NO_SYSTEM_PATHS=ON
    -DBoost_ARCHITECTURE="-a64"
    -DBOOST_INCLUDEDIR="$BOOST_INCLUDE_LOCATION"
    -DBOOST_LIBRARYDIR="$BOOST_LIB_LOCATION"
    -DPython3_EXECUTABLE="$PYTHON_EXECUTABLE_LOCATION"
    -DPython3_INCLUDE_DIR="$PYTHON_INCLUDE_LOCATION"
    -DPython3_LIBRARY="$PYTHON_LIBRARY_LOCATION"
    -DPXR_BUILD_ALEMBIC_PLUGIN=ON
    -DPXR_ENABLE_HDF5_SUPPORT=OFF
    -DOPENSUBDIV_INCLUDE_DIR="$OPENSUBDIV_INCLUDE_DIR"
    -DOPENSUBDIV_ROOT_DIR="$OPENSUBDIV_LIB_LOCATION"
    -DALEMBIC_INCLUDE_DIR="$ALEMBIC_INCLUDE_LOCATION"
    -DALEMBIC_DIR="$ALEMBIC_LIB_LOCATION"
    -DPXR_ENABLE_MATERIALX_SUPPORT=ON
    -DBUILD_SHARED_LIBS=ON
    -DPXR_BUILD_TESTS=OFF
    -DPXR_BUILD_EXAMPLES=OFF
    -DPXR_BUILD_TUTORIALS=OFF
    -DPXR_BUILD_USD_TOOLS=OFF
    -DPXR_BUILD_IMAGING=ON
    -DPXR_BUILD_USD_IMAGING=ON
    -DPXR_BUILD_USDVIEW=OFF
)

echo Configuring build for OpenUSD version $OPENUSD_VERSION...
cmake -G "Xcode" $OPENUSD_SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building OpenUSD for Release...
cmake --build . --config Release -j8

echo Installing OpenUSD for Release...
cmake --install . --config Release

popd > /dev/null

INSTALL_BIN_LOCATION="$INSTALL_LOCATION/bin"
INSTALL_LIB_LOCATION="$INSTALL_LOCATION/lib"

echo Removing command-line tools...
rm -rf "$INSTALL_BIN_LOCATION"

echo Moving built-in OpenUSD plugins to UsdResources plugins directory...
INSTALL_RESOURCES_LOCATION="$INSTALL_LOCATION/Resources/UsdResources/Mac"
INSTALL_RESOURCES_PLUGINS_LOCATION="$INSTALL_RESOURCES_LOCATION/plugins"
mkdir -p $INSTALL_RESOURCES_LOCATION
mv "$INSTALL_LIB_LOCATION/usd" "$INSTALL_RESOURCES_PLUGINS_LOCATION"

echo Moving OpenUSD plugin shared libraries to lib directory...
INSTALL_PLUGIN_LOCATION="$INSTALL_LOCATION/plugin"
INSTALL_PLUGIN_USD_LOCATION="$INSTALL_PLUGIN_LOCATION/usd"
mv $INSTALL_PLUGIN_USD_LOCATION/*.dylib "$INSTALL_LIB_LOCATION"

echo Removing top-level OpenUSD plugins plugInfo.json file...
rm -f "$INSTALL_PLUGIN_USD_LOCATION/plugInfo.json"

echo Moving OpenUSD plugin resource directories to UsdResources plugins directory
mv "$INSTALL_PLUGIN_USD_LOCATION/hdStorm" "$INSTALL_RESOURCES_PLUGINS_LOCATION"
mv "$INSTALL_PLUGIN_USD_LOCATION/sdrGlslfx" "$INSTALL_RESOURCES_PLUGINS_LOCATION"
mv "$INSTALL_PLUGIN_USD_LOCATION/usdAbc" "$INSTALL_RESOURCES_PLUGINS_LOCATION"
mv "$INSTALL_PLUGIN_USD_LOCATION/usdShaders" "$INSTALL_RESOURCES_PLUGINS_LOCATION"

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

echo Removing share directory...
rm -rf "$INSTALL_LOCATION/share"

# The locations of the shared libraries where they will live when ultimately
# deployed are used to generate relative paths for use as LibraryPaths in
# plugInfo.json files.
# The OpenUSD plugins all exist at the same directory level, so any of them can
# be used to generate a relative path.
USD_PLUGIN_LOCATION="$UE_ENGINE_LOCATION/Plugins/Importers/USDImporter/Resources/UsdResources/Mac/plugins/usd"
USD_LIBS_LOCATION="$UE_ENGINE_LOCATION/Plugins/Importers/USDImporter/Source/ThirdParty/Mac/bin"

echo Adjusting plugInfo.json LibraryPath fields...
USD_PLUGIN_TO_USD_LIBS_REL_PATH=`python3 -c "import os.path; print(os.path.relpath('$USD_LIBS_LOCATION', '$USD_PLUGIN_LOCATION'))"`

for PLUG_INFO_FILE in `find $INSTALL_RESOURCES_LOCATION -name plugInfo.json | xargs grep LibraryPath -l`
do
    sed -E -e "s|\"LibraryPath\": \"[\./]+(.*)\"|\"LibraryPath\": \"$USD_PLUGIN_TO_USD_LIBS_REL_PATH/\1\"|" -i "" $PLUG_INFO_FILE
done

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
done

echo Done.
