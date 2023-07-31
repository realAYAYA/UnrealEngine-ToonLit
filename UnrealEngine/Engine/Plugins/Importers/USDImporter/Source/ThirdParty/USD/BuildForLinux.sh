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
# We also apply a patch for the usdMtlx plugin to ensure that we do not
# bake a hard-coded path to the MaterialX standard data libraries into the
# built plugin:
#     git apply USD_v2208_usdMtlx_undef_stdlib_dir.patch
# Specifically for Linux when building with clang, an additional patch is
# needed to ensure that type comparisons work correctly across shared library
# boundaries:
#     git apply USD_v2208_Linux_clang_TfSafeTypeCompare.patch
# Note also that this path may be emitted as part of USD error messages, so
# it is suggested that it not reveal any sensitive information.
SOURCE_LOCATION="/tmp/USD_src"

ARCH_NAME=x86_64-unknown-linux-gnu

SCRIPT_DIR=`cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd`

UE_ENGINE_LOCATION=`cd $SCRIPT_DIR/../../../../../..; pwd`

UE_THIRD_PARTY_LOCATION="$UE_ENGINE_LOCATION/Source/ThirdParty"
TBB_LOCATION="$UE_THIRD_PARTY_LOCATION/Intel/TBB/IntelTBB-2019u8"
TBB_INCLUDE_LOCATION="$TBB_LOCATION/include"
TBB_LIB_LOCATION="$TBB_LOCATION/lib/Linux"
BOOST_LOCATION="$UE_THIRD_PARTY_LOCATION/Boost/boost-1_70_0"
BOOST_INCLUDE_LOCATION="$BOOST_LOCATION/include"
BOOST_LIB_LOCATION="$BOOST_LOCATION/lib/Unix/$ARCH_NAME"
IMATH_LOCATION="$UE_THIRD_PARTY_LOCATION/Imath/Deploy/Imath-3.1.3"
IMATH_LIB_LOCATION="$IMATH_LOCATION/Unix/$ARCH_NAME"
IMATH_CMAKE_LOCATION="$IMATH_LIB_LOCATION/lib/cmake/Imath"
ALEMBIC_LOCATION="$UE_THIRD_PARTY_LOCATION/Alembic/Deploy/alembic-1.8.2"
ALEMBIC_INCLUDE_LOCATION="$ALEMBIC_LOCATION/include"
ALEMBIC_LIB_LOCATION="$ALEMBIC_LOCATION/Unix/$ARCH_NAME"
MATERIALX_LOCATION="$UE_THIRD_PARTY_LOCATION/MaterialX/Deploy/MaterialX-1.38.5"
MATERIALX_LIB_LOCATION="$MATERIALX_LOCATION/Unix/$ARCH_NAME/lib"
MATERIALX_CMAKE_LOCATION="$MATERIALX_LIB_LOCATION/cmake/MaterialX"

PYTHON_BINARIES_LOCATION="$UE_ENGINE_LOCATION/Binaries/ThirdParty/Python3/Linux"
PYTHON_EXECUTABLE_LOCATION="$PYTHON_BINARIES_LOCATION/bin/python3"
PYTHON_SOURCE_LOCATION="$UE_THIRD_PARTY_LOCATION/Python3/Linux"
PYTHON_INCLUDE_LOCATION="$PYTHON_SOURCE_LOCATION/include/python3.9"
PYTHON_LIBRARY_LOCATION="$PYTHON_SOURCE_LOCATION/lib/libpython3.9.a"

UE_MODULE_USD_LOCATION=$SCRIPT_DIR

BUILD_LOCATION="$UE_MODULE_USD_LOCATION/Intermediate"

# USD build products are written into a deployment directory and must then
# be manually copied from there into place.
INSTALL_LOCATION="$BUILD_LOCATION/Deploy/USD-$USD_VERSION"

rm -rf $BUILD_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION > /dev/null

# Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
# that the toolchain is setup and verify that this name matches.
TOOLCHAIN_NAME=v20_clang-13.0.1-centos7

UE_TOOLCHAIN_LOCATION="$UE_ENGINE_LOCATION/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME/$ARCH_NAME"

C_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang"
CXX_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang++"

CXX_FLAGS="-std=c++11 -fPIC -nostdinc++ -I$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/include/c++/v1 -I$PYTHON_INCLUDE_LOCATION"
CXX_LINKER="-stdlib=libc++ -L$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/ -lc++ -lc++abi $PYTHON_LIBRARY_LOCATION -lm -lc -lgcc_s -lgcc -lutil"

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$INSTALL_LOCATION"
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_EXE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_MODULE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_SHARED_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_STATIC_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION;$MATERIALX_CMAKE_LOCATION"
    -DTBB_INCLUDE_DIR="$TBB_INCLUDE_LOCATION"
    -DTBB_LIBRARY="$TBB_LIB_LOCATION"
    -DBoost_NO_BOOST_CMAKE=ON
    -DBoost_NO_SYSTEM_PATHS=ON
    -DBoost_ARCHITECTURE="-x64"
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
    -DPXR_ENABLE_MATERIALX_SUPPORT=ON
    -DBUILD_SHARED_LIBS=ON
    -DPXR_BUILD_TESTS=OFF
    -DPXR_BUILD_EXAMPLES=OFF
    -DPXR_BUILD_TUTORIALS=OFF
    -DPXR_BUILD_USD_TOOLS=OFF
    -DPXR_BUILD_IMAGING=OFF
    -DPXR_BUILD_USD_IMAGING=OFF
    -DPXR_BUILD_USDVIEW=OFF
)

NUM_CPU=`grep -c ^processor /proc/cpuinfo`

echo Configuring build for USD version $USD_VERSION...
cmake -G "Unix Makefiles" $SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building USD for Release...
cmake --build . -j$NUM_CPU

echo Installing USD for Release...
cmake --install .

popd > /dev/null

echo Moving built-in USD plugins to UsdResources plugins directory...
INSTALL_LIB_LOCATION="$INSTALL_LOCATION/lib"
INSTALL_RESOURCES_LOCATION="$INSTALL_LOCATION/Resources/UsdResources/Linux"
INSTALL_RESOURCES_PLUGINS_LOCATION="$INSTALL_RESOURCES_LOCATION/plugins"
mkdir -p $INSTALL_RESOURCES_LOCATION
mv "$INSTALL_LIB_LOCATION/usd" "$INSTALL_RESOURCES_PLUGINS_LOCATION"

echo Moving USD plugin shared libraries to lib directory...
INSTALL_PLUGIN_LOCATION="$INSTALL_LOCATION/plugin"
INSTALL_PLUGIN_USD_LOCATION="$INSTALL_PLUGIN_LOCATION/usd"
mv $INSTALL_PLUGIN_USD_LOCATION/*.so "$INSTALL_LIB_LOCATION"

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
INSTALL_CONTENT_LOCATION="$INSTALL_LOCATION/Content/Python/Lib/Linux/site-packages"
mkdir -p "$INSTALL_CONTENT_LOCATION"
mv "$INSTALL_LOCATION/lib/python/pxr" "$INSTALL_CONTENT_LOCATION"
rmdir "$INSTALL_LOCATION/lib/python"

echo Cleaning @rpath entries for shared libraries...
# The locations of the shared libraries where they will live when ultimately
# deployed are used to generate relative paths for use as rpaths.
# The USD Python module shared libraries all exist at the same directory level,
# so any of them can be used to generate a relative path.
USD_PYTHON_MODULE_LOCATION="$UE_ENGINE_LOCATION/Plugins/Importers/USDImporter/Content/Python/Lib/Linux/site-packages/pxr/Usd"
USD_LIBS_LOCATION="$UE_ENGINE_LOCATION/Plugins/Importers/USDImporter/Source/ThirdParty/Linux/bin/$ARCH_NAME"
ENGINE_BINARIES_LOCATION="$UE_ENGINE_LOCATION/Binaries/Linux"

# The USD Python modules link first against the USD libraries within the plugin
# directory followed by libraries in the engine binaries.
PYTHON_TO_USD_LIBS_REL_PATH=`python -c "import os.path; print os.path.relpath('$USD_LIBS_LOCATION', '$USD_PYTHON_MODULE_LOCATION')"`
PYTHON_TO_ENGINE_BINARIES_REL_PATH=`python -c "import os.path; print os.path.relpath('$ENGINE_BINARIES_LOCATION', '$USD_PYTHON_MODULE_LOCATION')"`

for PY_SHARED_LIB in `find $INSTALL_CONTENT_LOCATION -name '*.so'`
do
    patchelf --set-rpath "\$ORIGIN/$PYTHON_TO_USD_LIBS_REL_PATH:\$ORIGIN/$PYTHON_TO_ENGINE_BINARIES_REL_PATH" --force-rpath $PY_SHARED_LIB
done

# The USD libraries link first against sibling libraries in the same directory
# followed by libraries in the engine binaries.
USD_LIBS_TO_ENGINE_BINARIES_REL_PATH=`python -c "import os.path; print os.path.relpath('$ENGINE_BINARIES_LOCATION', '$USD_LIBS_LOCATION')"`

for USD_SHARED_LIB in `find $INSTALL_LIB_LOCATION -name '*.so'`
do
    patchelf --set-rpath "\$ORIGIN/.:\$ORIGIN/$USD_LIBS_TO_ENGINE_BINARIES_REL_PATH" --force-rpath $USD_SHARED_LIB
done

echo Done.
