#!/bin/sh

# Copyright Epic Games, Inc. All Rights Reserved.

SCRIPT_DIR=$(cd $(dirname $0) && pwd)

BUILD_DIR="${SCRIPT_DIR}/../../IOS/Build"

if [ -d "${BUILD_DIR}" ]; then
    rm -rf "${BUILD_DIR}"
fi
mkdir -pv "${BUILD_DIR}"

cd "${BUILD_DIR}"
${UE4_ENGINE_ROOT_DIR}/Engine/Extras/ThirdPartyNotUE/CMake/bin/cmake -G "Xcode" -DSOCKET_IMPL=../../src/sock.c -DDISABLE_TLS=0 -DOPENSSL_PATH=${UE4_OPENSSL_ROOT_DIR}/include/IOS -DEXPAT_PATH=${UE4_EXPAT_ROOT_DIR}/lib -DCMAKE_XCODE_ATTRIBUTE_SDKROOT=iphoneos -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=8.0 -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO -DCMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY=1,2 "${SCRIPT_DIR}/../../BuildForUE"

function build()
{
    CONFIGURATION=$1
    xcodebuild BITCODE_GENERATION_MODE=bitcode -project libstrophe.xcodeproj -configuration $CONFIGURATION -destination generic/platform=iOS
}

build Release
build Debug
cd "${SCRIPT_DIR}"
rm -rf "${BUILD_DIR}"
