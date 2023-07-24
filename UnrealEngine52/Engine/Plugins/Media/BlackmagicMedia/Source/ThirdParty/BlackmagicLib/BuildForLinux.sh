

set -e

DECKLINK_VERSION=12.2

ARCH_NAME=x86_64-unknown-linux-gnu
UE_ENGINE_LOCATION=`cd $(pwd)/../../../../../..; pwd`
UE_THIRD_PARTY_LOCATION="$UE_ENGINE_LOCATION/Source/ThirdParty/"

BUILD_LOCATION="build"

rm -rf $BUILD_LOCATION

mkdir $BUILD_LOCATION

# Run Engine/Build/BatchFiles/Linux/SetupToolchain.sh first to ensure
# that the toolchain is setup and verify that this name matches.
TOOLCHAIN_NAME=v20_clang-13.0.1-centos7

UE_TOOLCHAIN_LOCATION="$UE_ENGINE_LOCATION/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$TOOLCHAIN_NAME/$ARCH_NAME"

C_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang"
CXX_COMPILER="$UE_TOOLCHAIN_LOCATION/bin/clang++"

CXX_FLAGS="-fPIC -I$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/include/c++/v1"
CXX_LINKER="-nodefaultlibs -L$UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/ $UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/libc++.a $UE_THIRD_PARTY_LOCATION/Unix/LibCxx/lib/Unix/$ARCH_NAME/libc++abi.a -lm -lc -lgcc_s -lgcc"

CMAKE_ARGS=(
    -DCMAKE_C_COMPILER="$C_COMPILER"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_CXX_STANDARD=14
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS"
    -DCMAKE_EXE_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_SHARED_LINKER_FLAGS="$CXX_LINKER"
    -DCMAKE_MODULE_LINKER_FLAGS="$CXX_LINKER"
)

NUM_CPU=`grep -c ^processor /proc/cpuinfo`

pwd

echo Configuring Release build for Blackmagic Decklink version $DECKLINK_VERSION...
cmake -S . -B build "${CMAKE_ARGS[@]}"

echo Building Blackmagic Decklink for Release...
cmake --build build --config Release --target all -- -j$NUM_CPU

echo Done.
