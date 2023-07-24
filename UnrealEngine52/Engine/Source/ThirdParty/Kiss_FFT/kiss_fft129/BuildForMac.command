#!/bin/bash

set -e

UE_MODULE_LOCATION=`pwd`
SOURCE_LOCATION="$UE_MODULE_LOCATION/"

echo Building kiss_fft129 for Debug...
CONFIGURATION=Debug
xcodebuild clean build -scheme=KissFFT -project $SOURCE_LOCATION/KissFFT.xcodeproj -configuration $CONFIGURATION -xcconfig "$SOURCE_LOCATION/XcodeConfig"

echo Building kiss_fft129 for Release...
CONFIGURATION=Release
xcodebuild clean build -scheme=KissFFT -project $SOURCE_LOCATION/KissFFT.xcodeproj -configuration $CONFIGURATION -xcconfig "$SOURCE_LOCATION/XcodeConfig"

echo Done.
