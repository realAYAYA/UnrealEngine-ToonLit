#!/bin/sh

OCIO_VERSION="2.2.0"
OCIO_LIB_NAME="OpenColorIO-$OCIO_VERSION"

SCRIPT_DIR=`cd $(dirname "$BASH_SOURCE"); pwd`
UE_ENGINE_DIR=`cd $SCRIPT_DIR/../../../../../..; pwd`
UE_THIRD_PARTY_DIR="$UE_ENGINE_DIR/Source/ThirdParty"

# Using the toolchain downloaded by Engine/Build/BatchFiles/Linux/SetupToolchain.sh
#source "$UE_ENGINE_DIR/Build/BatchFiles/Linux/SetupToolchain.sh"

C_COMPILER="$UE_ENGINE_DIR/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v20_clang-13.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang"
CXX_COMPILER="$UE_ENGINE_DIR/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v20_clang-13.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang++"

cd $SCRIPT_DIR

# Download library source if not present
# if [ ! -f "$OCIO_LIB_NAME.zip" ]; then
#     wget "https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.0.zip" -O "$OCIO_LIB_NAME.zip"
# fi

# Remove previously extracted build library folder
if [ -d "$OCIO_LIB_NAME" ]; then
    echo "Deleting previously extracted $OCIO_LIB_NAME folder"
    rm -rf "$OCIO_LIB_NAME"
fi

# echo "Extracting $OCIO_LIB_NAME.zip..."
# unzip "$OCIO_LIB_NAME.zip"

git clone --depth 1 --branch v2.2.0 https://github.com/AcademySoftwareFoundation/OpenColorIO.git $OCIO_LIB_NAME

cd $OCIO_LIB_NAME

ARCH_NAME=x86_64-unknown-linux-gnu
CXX_FLAGS="-nostdinc++ -I$UE_THIRD_PARTY_DIR/Unix/LibCxx/include  -I$UE_THIRD_PARTY_DIR/Unix/LibCxx/include/c++/v1"
LIBS="$UE_THIRD_PARTY_DIR/Unix/LibCxx/lib/Unix/$ARCH_NAME/libc++.a $UE_THIRD_PARTY_DIR/Unix/LibCxx/lib/Unix/$ARCH_NAME/libc++abi.a"
IMATH_CMAKE_LOCATION="$UE_THIRD_PARTY_DIR/Imath/Deploy/Imath-3.1.3/Unix/$ARCH_NAME/lib/cmake/Imath"

export CC=$C_COMPILER
export CXX=$CXX_COMPILER

# Configure OCIO cmake and launch a release build
echo "Configuring build..."
cmake -S . -Bbuild \
    -DCMAKE_BUILD_TYPE="Release" \
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION" \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_CXX_STANDARD=11 \
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$LIBS" \
    -DCMAKE_MODULE_LINKER_FLAGS="$LIBS" \
    -DCMAKE_STATIC_LINKER_FLAGS="$LIBS" \
    -DCMAKE_SHARED_LINKER_FLAGS="$LIBS -nodefaultlibs -nostdlib++ -lm -lc -lgcc_s -lgcc" \
    -DOCIO_BUILD_APPS=OFF \
    -DOCIO_BUILD_GPU_TESTS=OFF \
    -DOCIO_BUILD_NUKE=OFF \
    -DOCIO_BUILD_DOCS=OFF \
    -DOCIO_BUILD_TESTS=OFF \
    -DOCIO_BUILD_PYTHON=OFF \
    -DOCIO_INSTALL_EXT_PACKAGES=MISSING \
    -Dexpat_STATIC_LIBRARY=ON \
    -DEXPAT_CXX_FLAGS="$CXX_FLAGS" \
    -Dyaml-cpp_STATIC_LIBRARY=ON \
    -Dyaml-cpp_CXX_FLAGS="$CXX_FLAGS" \
    -Dpystring_STATIC_LIBRARY=ON \
    -Dpystring_CXX_FLAGS="$CXX_FLAGS" \
    -DCMAKE_INSTALL_PREFIX:PATH=./install

echo "Building Release build..."
NUM_CPU=`grep -c ^processor /proc/cpuinfo`
cmake --build build --config Release --target install -- -j $NUM_CPU

#echo "Copying library build files..."
cp build/install/lib64/libOpenColorIO.so.2.2.0 ../../../../Binaries/ThirdParty/Linux/libOpenColorIO.so.2.2
