#!/bin/bash

set -e

LLVM_VERSION=13.0.1

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`

UE_MODULE_LOCATION=`pwd`

LLVM_SOURCE_LOCATION="$UE_MODULE_LOCATION/llvm-$LLVM_VERSION/llvm/"
LLVM_BUILD_LOCATION="$LLVM_SOURCE_LOCATION/build/"
LLVM_INSTALL_LOCATION="$UE_MODULE_LOCATION/llvm/"

rm -rf $LLVM_SOURCE_LOCATION
rm -rf $LLVM_BUILD_LOCATION
rm -rf "$UE_MODULE_LOCATION/llvm-$LLVM_VERSION"

echo Cloning and building llvm $LLVM_VERSION...
git clone https://github.com/llvm/llvm-project.git --branch llvmorg-$LLVM_VERSION llvm-$LLVM_VERSION

mkdir -p $LLVM_BUILD_LOCATION

CMAKE_ARGS=(
    -DCMAKE_INSTALL_PREFIX="$LLVM_INSTALL_LOCATION"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9"
    -DCMAKE_BUILD_TYPE=Release
    -DLLVM_ENABLE_DUMP=ON
    -DLLVM_ENABLE_ASSERTIONS=ON
    -DLLVM_INSTALL_UTILS=ON
    -DLLVM_TARGETS_TO_BUILD="AArch64;ARM;X86"
    -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly
    -DLLVM_ENABLE_PROJECTS=clang
)

if [ "$BUILD_UNIVERSAL" = true ] ; then
    CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
fi

BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

rm -rf $BUILD_LOCATION
mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

echo Configuring build for llvm version $LLVM_VERSION...
cmake -G "Xcode" $LLVM_SOURCE_LOCATION "${CMAKE_ARGS[@]}"

echo Building llvm for Release...
cmake --build . --config Release

echo Installing llvm for Release...
cmake --install . --config Release

popd

echo Done.
