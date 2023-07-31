#!/bin/bash

set -e

pushd . > /dev/null

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/../../..; pwd`
UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION=$UE_MODULE_LOCATION
INSTALL_LOCATION="$UE_THIRD_PARTY_LOCATION/../../Binaries/ThirdParty/Intel/ISPCTexComp/Mac64-Release"

echo Building ISPCTexComp for Release...
xcodebuild clean build -scheme=ispc_texcomp -project $SOURCE_LOCATION/ispc_texcomp.xcodeproj -configuration Release -xcconfig "XcodeConfig"

echo Installing ISPCTexComp...
cp "$SOURCE_LOCATION/build/libispc_texcomp.dylib" "$INSTALL_LOCATION/libispc_texcomp.dylib"

popd

echo Done.
