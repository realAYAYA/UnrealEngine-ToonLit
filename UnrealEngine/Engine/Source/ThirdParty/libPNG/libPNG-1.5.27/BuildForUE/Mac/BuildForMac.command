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
LIB_NAME="libpng"
# In this lib there are multiple versions, so we CD in here first and have less drops
LIB_VERSION=libPNG-1.5.27
# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParrty
DROP_TO_THIRDPARTY=../..

# files we build, relative to LIB_VERSION
LIBFILES=( "lib/Mac/libpng.a")

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
checkoutFiles ${LIBFILES[@]}
saveFileStates ${LIBFILES[@]}

## 
## libpng specific steps for make

## Copied from makefile.darwin as we want to add -arch for arm64/x86_64 so rather than modify
## the makefile we'll export all the flags here
ZLIBLIB=../zlib
ZLIBINC=../zlib
CFLAGS="-I${ZLIBINC} -W -Wall -O -funroll-loops -arch x86_64"
LDFLAGS=-"L. -L${ZLIBLIB} -lpng15 -lz -arch x86_64"

if [ "$BUILD_UNIVERSAL" = true ] ; then
    CFLAGS="${CFLAGS}  -arch arm64 -DPNG_ARM_NEON_OPT=0"
    LDFLAGS="${LDFLAGS}  -arch arm64"
fi

export CFLAGS
export LDFLAGS
make -f scripts/makefile.darwin clean
make -f scripts/makefile.darwin libpng.a -e -j$(get_core_count)

LIBOUTPUT=libpng.a
echo "Moving ${LIBOUTPUT} to ${LIBFILES[0]}"
mv -f ${LIBOUTPUT} ${LIBFILES[0]}

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

popd > /dev/null