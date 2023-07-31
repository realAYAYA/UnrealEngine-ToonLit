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
LIB_NAME="freeimage"
# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParty
DROP_TO_THIRDPARTY=../..
# Drops from the location of LIBROOT to Engine/ThirdParty/Binaries
DROP_TO_THIRDPARTY_BINARIES=${DROP_TO_THIRDPARTY}/../../Binaries/ThirdParty

# files we build, relative to LIBROOT
LIBFILES=( 
    "${DROP_TO_THIRDPARTY_BINARIES}/FreeImage/Mac/libfreeimage-3.18.0.dylib"
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

echo Rebuilding ${LIB_NAME} using temp path ${TMPDIR}

# checkout the library list and save their state
checkoutFiles ${LIBFILES[@]}
saveFileStates ${LIBFILES[@]}

#################################################
## Begin lib specific build steps
make clean && make -j$(get_core_count)

LIBOUTPUT=dist/libfreeimage-3.18.0.dylib

# Set the install name to the one unreal expect
install_name_tool -id @rpath/libfreeimage-3.18.0.dylib ${LIBOUTPUT}

# Move the output file into the expected place
echo "Copying ${LIBOUTPUT} to ${LIBFILES[0]}"
cp ${LIBOUTPUT} ${LIBFILES[0]}

## End of lib specific build steps
#################################################

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

popd > /dev/null