#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

## 
## Most of the following script is intended to be consistent for building all Mac 
## third-party source. The sequence of steps are -
## 1) Set up constants, create temp dir, checkout files, save file info
## 2) lib-specific build steps
## 3) Check files were updated

##
## Lib specific constants

# Name of lib
LIB_NAME="libopus"
# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParrty
DROP_TO_THIRDPARTY=../..

# files we build, relative to LIBROOT
LIBFILES=( "Mac/libopus.a" "Mac/libspeex_resampler.a" "Mac/libspeex_resamplerd.a")

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
TMPDIR="/tmp/${LIB_NAME}-$$"
mkdir -p ${TMPDIR} > /dev/null 2>&1

echo Rebuilding ${LIB_NAME} using temp path ${TMPDIR}

# checkout the library list and save their state
checkoutFiles ${LIBFILES[@]}
saveFileStates ${LIBFILES[@]}

## 
## Opus Specific Steps for automake
automake --add-missing
autoconf -f

UE_C_CPP_LD_FLAGS="-mmacosx-version-min=10.14 -gdwarf-2"

# run configure and make for each architecture
sh ./configure --prefix=${TMPDIR}/x86_64 --host=x86_64-apple-macos CFLAGS="${UE_C_CPP_LD_FLAGS}" CPPFLAGS="${UE_C_CPP_LD_FLAGS}" LDFLAGS="${UE_C_CPP_LD_FLAGS}" && make clean && make -j$(get_core_count) && make install

# build universal libs?
if [ "$BUILD_UNIVERSAL" = true ] ; then
    sh ./configure --prefix=${TMPDIR}/arm64 --host=aarch64-apple-macos CFLAGS="${UE_C_CPP_LD_FLAGS}" CPPFLAGS="${UE_C_CPP_LD_FLAGS}" LDFLAGS="${UE_C_CPP_LD_FLAGS}" && make clean && make -j$(get_core_count) && make install
    # lipo the results into a universal binary
    lipo -create ${TMPDIR}/x86_64/lib/libopus.a ${TMPDIR}/arm64/lib/libopus.a -output ./Mac/libopus.a
else
    cp -v ${TMPDIR}/x86_64/lib/libopus.a ./Mac/libopus.a
fi

# Now build libspeex_resampler via Xcode. The xcconfig is set up to build universal libs if desired directly into the desired folder
xcodebuild clean build -target "speex_resampler" -configuration Debug -project "speex_resampler/Mac/speex_resampler.xcodeproj" -xcconfig "BuildForUE/Mac/XcodeConfig"
xcodebuild clean build -target "speex_resampler" -configuration Release -project  "speex_resampler/Mac/speex_resampler.xcodeproj" -xcconfig "BuildForUE/Mac/XcodeConfig"

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

popd > /dev/null
