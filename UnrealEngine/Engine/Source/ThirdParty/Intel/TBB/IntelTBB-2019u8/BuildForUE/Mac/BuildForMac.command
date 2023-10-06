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
LIB_NAME="IntelTBB"
# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParrty
DROP_TO_THIRDPARTY=../../..
# Drops to location of TP binaries
DROP_TO_THIRDPARTY_BINARIES=${DROP_TO_THIRDPARTY}/../../Binaries/ThirdParty

# files we build, relative to LIB_SUBFOLDER
LIB_PATH=lib/Mac
LIBFILES=( 
    "${LIB_PATH}/libtbb_debug.a"
    "${LIB_PATH}/libtbb.a" 
    "${LIB_PATH}/libtbb.dylib"
    "${LIB_PATH}/libtbbmalloc_debug.a"
    "${LIB_PATH}/libtbbmalloc.a"
    "${LIB_PATH}/libtbbmalloc.dylib"
)

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

# checkout the library list and save their state
checkoutFiles ${LIBFILES[@]}
saveFileStates ${LIBFILES[@]}

echo Rebuilding ${LIB_NAME} using temp path ${TMPDIR}

# IntelTBB needs any build folder to be one level above build or you'll see 
# ''../../build/Makefile.tbb: No such file or directory' or similar
WORKDIR_x86=build/tmp_x86
WORKDIR_arm64=build/tmp_arm64
MAKEARGS_x86="work_dir=${WORKDIR_x86} arch=intel64 os=macos -j$(get_core_count)"
MAKEARGS_arm64="work_dir=${WORKDIR_arm64} arch=arm64 os=macos -j$(get_core_count)"

## files that will be built lipo results 
RELEASE_OUTPUT=( "libtbb.a" "libtbb.dylib" "libtbbmalloc.a" "libtbbmalloc.dylib")
DEBUG_OUTPUT=( "libtbb_debug.a" "libtbbmalloc_debug.a")

#echo make ${MAKEARGS_x86}
make ${MAKEARGS_x86} clean && make ${MAKEARGS_x86} && make ${MAKEARGS_x86} extra_inc=big_iron.inc

if [ "$BUILD_UNIVERSAL" = true ] ; then
    make ${MAKEARGS_arm64} clean && make ${MAKEARGS_arm64} && make ${MAKEARGS_arm64} extra_inc=big_iron.inc

    # release files, all four
    for file in "${RELEASE_OUTPUT[@]}"
    do
        x86slice=${WORKDIR_x86}_release/$file
        armslice=${WORKDIR_arm64}_release/$file
        fatlib=./lib/Mac/$file
        echo Building ${x86slice} ${armslice} into fatlib ${fatlib}
        lipo -create ${x86slice} ${armslice} -output ${fatlib}
    done

    # debug files, only static libs 
    for file in "${DEBUG_OUTPUT[@]}"
    do
        x86slice=${WORKDIR_x86}_debug/$file
        armslice=${WORKDIR_arm64}_debug/$file
        fatlib=./lib/Mac/$file
        echo Building ${x86slice} ${armslice} into fatlib ${fatlib}
        lipo -create ${x86slice} ${armslice} -output ${fatlib}
    done
else
    # just copy the x86 slice over
    for file in "${RELEASE_OUTPUT[@]}"
    do
        cp -v ${WORKDIR_x86}_release/$file ./lib/Mac/$file
    done

    for file in "${DEBUG_OUTPUT[@]}"
    do
        cp -v ${WORKDIR_x86}_debug/$file ./lib/Mac/$file
    done
fi

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

# now move the temp folders under /tmp so they're available if needed but will be cleaned up
mv -f ${WORKDIR_x86}_release ${TMPDIR}
mv -f ${WORKDIR_x86}_debug ${TMPDIR}
if [ "$BUILD_UNIVERSAL" = true ] ; then
    mv -f ${WORKDIR_arm64}_release ${TMPDIR}
    mv -f ${WORKDIR_arm64}_debug ${TMPDIR}
fi

popd > /dev/null
