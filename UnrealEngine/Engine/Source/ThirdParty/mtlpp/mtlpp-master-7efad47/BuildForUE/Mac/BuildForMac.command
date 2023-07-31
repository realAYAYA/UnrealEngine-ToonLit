#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

## 
## Most of the following script is intended to be consistent for building all Mac 
## third-party source. The sequence of steps are -
## 1) Determine location of script then CD into the lib folder so paths are relative
## 2) Set up constants, create temp dir, checkout files, save file info
## 3) lib-specific build steps
## 4) Check files were updated

##
## Lib specific constants

# Name of lib
LIB_NAME="mtlpp"
# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParty
DROP_TO_THIRDPARTY=../..

# files we build, relative to LIB_ROOT
LIBFILES_MACOS=( "lib/Mac/libmtlpp.a" "lib/Mac/libmtlppd.a" "lib/Mac/libmtlpps.a")
LIBFILES_IOS=( "lib/IOS/libmtlpp.a" "lib/IOS/libmtlppd.a" "lib/IOS/libmtlpps.a")
LIBFILES_TVOS=( "lib/TVOS/libmtlpp.a" "lib/TVOS/libmtlppd.a" "lib/TVOS/libmtlpps.a")

##
## Common setup steps

# Build script will be in <lib>/Build/Mac so get that path and drop two folders to leave us
# in the actual lib folder
pushd . > /dev/null
SCRIPT_DIR="`dirname "${BASH_SOURCE[0]}"`"
cd ${SCRIPT_DIR}/${DROP_TO_LIBROOT}
LIB_ROOT_DIR=${PWD}
echo Changed to ${LIB_ROOT_DIR}

# We should be in ThirdParty/LibName and we want to pull in some common things from
# ThirdParty/BuildScripts/Mac/Common
source ${DROP_TO_THIRDPARTY}/BuildScripts/Mac/Common/Common.sh

# create a tempdir and save it (note the tmpdir variable is used by the functions that 
# check file state)
TMPDIR="/tmp/${LIB_NAME}"
mkdir -p ${TMPDIR} > /dev/null 2>&1

echo Rebuilding ${LIB_NAME} using temp path ${TMPDIR}

echo $PWD

# checkout the library list and save their state
checkoutFiles ${LIBFILES_MACOS[@]}
checkoutFiles ${LIBFILES_IOS[@]}
checkoutFiles ${LIBFILES_TVOS[@]}
saveFileStates ${LIBFILES_MACOS[@]}
saveFileStates ${LIBFILES_IOS[@]}
saveFileStates ${LIBFILES_TVOS[@]}

xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-mac" -configuration "Debug" -xcconfig BuildForUE/Mac/XcodeConfig
xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-mac" -configuration "Release" -xcconfig BuildForUE/Mac/XcodeConfig
xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-mac" -configuration "Shipping" -xcconfig BuildForUE/Mac/XcodeConfig

xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-ios" -configuration "Debug" -xcconfig BuildForUE/Mac/XcodeConfigIOS
xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-ios" -configuration "Release" -xcconfig BuildForUE/Mac/XcodeConfigIOS
xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-ios" -configuration "Shipping" -xcconfig BuildForUE/Mac/XcodeConfigIOS

xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-tvos" -configuration "Debug" -xcconfig BuildForUE/Mac/XcodeConfigTVOS
xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-tvos" -configuration "Release" -xcconfig BuildForUE/Mac/XcodeConfigTVOS
xcodebuild clean build -project projects/mtlpp.xcodeproj -target "mtlpp-tvos" -configuration "Shipping" -xcconfig BuildForUE/Mac/XcodeConfigTVOS

# check the files were all touched
checkFilesWereUpdated ${LIBFILES_MACOS[@]}
checkFilesWereUpdated ${LIBFILES_IOS[@]}
checkFilesWereUpdated ${LIBFILES_TVOS[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES_MACOS[@]} ${LIBFILES_IOS[@]} ${LIBFILES_TVOS[@]}

popd > /dev/null
