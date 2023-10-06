#!/bin/sh

OCIO_VERSION="2.2.0"
OCIO_LIB_NAME="OpenColorIO-$OCIO_VERSION"

SCRIPT_DIR=`cd $(dirname "$BASH_SOURCE"); pwd`
UE_ENGINE_DIR=`cd $SCRIPT_DIR/../../..; pwd`
UE_THIRD_PARTY_DIR="$UE_ENGINE_DIR/Source/ThirdParty"

# Using the toolchain downloaded by Engine/Build/BatchFiles/Linux/SetupToolchain.sh
#pushd "$UE_ENGINE_DIR/Build/BatchFiles/Linux"
#source "SetupToolchain.sh"
#popd

cd $SCRIPT_DIR

# Note: There is an error in the current version of this build process from github where
# compiling submodules the first time causes the library build to fail. Disabling the 
# deletion, cloning and patching, followed by a second run succeeds.

# Remove previously extracted build library folder
if [ -d "$OCIO_LIB_NAME" ]; then
    echo "Deleting previously extracted $OCIO_LIB_NAME folder"
    rm -rf "$OCIO_LIB_NAME"
fi

git clone --depth 1 --branch v2.2.0 https://github.com/AcademySoftwareFoundation/OpenColorIO.git $OCIO_LIB_NAME

cd $OCIO_LIB_NAME

git apply ../ue_ocio_v22.patch

ARCH_NAME=$1
if [ -z "$ARCH_NAME" ]
then
    echo "Arch: 'x86_64-unknown-linux-gnu' or 'aarch64-unknown-linux-gnueabi'"
    exit 1
fi

TOOLCHAIN_NAME=v21_clang-15.0.1-centos7
UE_TOOLCHAIN_DIR="$UE_ENGINE_DIR/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME"
CXX_FLAGS="-nostdinc++ -I$UE_THIRD_PARTY_DIR/Unix/LibCxx/include  -I$UE_THIRD_PARTY_DIR/Unix/LibCxx/include/c++/v1"
LINKER_FLAGS="$UE_THIRD_PARTY_DIR/Unix/LibCxx/lib/Unix/$ARCH_NAME/libc++.a $UE_THIRD_PARTY_DIR/Unix/LibCxx/lib/Unix/$ARCH_NAME/libc++abi.a"
IMATH_CMAKE_LOCATION="$UE_THIRD_PARTY_DIR/Imath/Deploy/Imath-3.1.3/Unix/$ARCH_NAME/lib/cmake/Imath"

# Determine whether we're cross compiling for an architecture that doesn't
# match the host. This is the way that CMake determines the value for the
# CMAKE_HOST_SYSTEM_PROCESSOR variable.
HOST_SYSTEM_PROCESSOR=`uname -m`
TARGET_SYSTEM_PROCESSOR=$HOST_SYSTEM_PROCESSOR

if [[ $ARCH_NAME != $HOST_SYSTEM_PROCESSOR* ]]
then
    ARCH_NAME_PARTS=(${ARCH_NAME//-/ })
    TARGET_SYSTEM_PROCESSOR=${ARCH_NAME_PARTS[0]}
fi

( cat <<_EOF_
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR ${TARGET_SYSTEM_PROCESSOR})

    set(CMAKE_SYSROOT ${UE_TOOLCHAIN_DIR}/${ARCH_NAME})
    set(CMAKE_LIBRARY_ARCHITECTURE ${ARCH_NAME})

    set(CMAKE_C_COMPILER \${CMAKE_SYSROOT}/bin/clang)
    set(CMAKE_C_COMPILER_TARGET ${ARCH_NAME})
    set(CMAKE_C_FLAGS "-target ${ARCH_NAME} ${C_FLAGS}")

    set(CMAKE_CXX_COMPILER \${CMAKE_SYSROOT}/bin/clang++)
    set(CMAKE_CXX_COMPILER_TARGET ${ARCH_NAME})
    set(CMAKE_CXX_FLAGS "-target ${ARCH_NAME} ${CXX_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS "${LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${LINKER_FLAGS} -nodefaultlibs -nostdlib++ -lm -lc -lgcc_s -lgcc")

    set(CMAKE_FIND_ROOT_PATH ${UE_TOOLCHAIN_DIR})
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
_EOF_
) > /tmp/__cmake_toolchain.cmake

# Configure OCIO cmake and launch a release build
echo "Configuring build..."
cmake -S . -Bbuild \
    -DCMAKE_BUILD_TYPE="Release" \
    -DCMAKE_TOOLCHAIN_FILE="/tmp/__cmake_toolchain.cmake" \
    -DCMAKE_PREFIX_PATH="$IMATH_CMAKE_LOCATION" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_CXX_STANDARD=11 \
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
target="../../../../Binaries/ThirdParty/OpenColorIO/Unix/$ARCH_NAME"
mkdir -p $target
cp build/install/lib/libOpenColorIO.so.2.2.0 $target/libOpenColorIO.so.2.2
