#!/bin/bash

set -e

UE_THIRD_PARTY_LOCATION=`cd $(pwd)/..; pwd`

UE_MODULE_LOCATION=`pwd`

SOURCE_LOCATION="$UE_MODULE_LOCATION"
BUILD_LOCATION="$UE_MODULE_LOCATION/Intermediate"

rm -rf $BUILD_LOCATION

mkdir $BUILD_LOCATION
pushd $BUILD_LOCATION

xcodebuild clean -scheme=SpeedTreeCore -project $SOURCE_LOCATION/Source/Core/MacOSX/SpeedTreeCore.xcodeproj -configuration Debug -xcconfig "$SOURCE_LOCATION/Source/Core/MacOSX/XcodeConfig"
xcodebuild build -scheme=SpeedTreeCore -project $SOURCE_LOCATION/Source/Core/MacOSX/SpeedTreeCore.xcodeproj -configuration Debug -xcconfig "$SOURCE_LOCATION/Source/Core/MacOSX/XcodeConfig"
xcodebuild build -scheme=SpeedTreeCore -project $SOURCE_LOCATION/Source/Core/MacOSX/SpeedTreeCore.xcodeproj -configuration Release -xcconfig "$SOURCE_LOCATION/Source/Core/MacOSX/XcodeConfig"

# Remove temporary build data
rm -rf "$SOURCE_LOCATION/Lib/MacOSX/XCBuildData"

popd

echo Done.
