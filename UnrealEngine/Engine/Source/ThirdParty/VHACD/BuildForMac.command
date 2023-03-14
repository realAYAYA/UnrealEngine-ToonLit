#!/bin/bash

set -e

UE_MODULE_LOCATION=`pwd`
SOURCE_LOCATION="$UE_MODULE_LOCATION/"

echo Building VHACD for Debug...
CONFIGURATION=Debug
xcodebuild clean build -scheme=VHACD -project $SOURCE_LOCATION"/build/Xcode/VHACD.xcodeproj" -configuration $CONFIGURATION -xcconfig "$SOURCE_LOCATION/XcodeConfig"

echo Building VHACD for Release...
CONFIGURATION=Release
xcodebuild clean build -scheme=VHACD -project $SOURCE_LOCATION"/build/Xcode/VHACD.xcodeproj" -configuration $CONFIGURATION -xcconfig "$SOURCE_LOCATION/XcodeConfig"

echo Done.
