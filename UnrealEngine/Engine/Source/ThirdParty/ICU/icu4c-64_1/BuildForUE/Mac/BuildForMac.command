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
LIB_NAME="icu"
# Drops from the location of this script to where libfiles are relative to
#  e.g. the following might exist
#  ${DROP_TO_LIBROOT}/README
#  ${DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParrty
DROP_TO_THIRDPARTY=../..

# Path to libs from libroot
LIB_PATH=lib/Mac

# files we build, relative to LIBROOT. 
LIBFILES=( 
    "${LIB_PATH}/libicu.a" 
    "${LIB_PATH}/libicud.a" 
    )

##
## Common setup steps

# Get the path of the script (e.g. <lib>/BuildForUE/Mac/BuildForMac.sh) then drop 
# two folders to leave us in the actual lib folder 
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
## ICU Specific Steps for CMake

# CMake files are in BuildForUE
CMAKE_BASE_DIR=${PWD}/BuildForUE
OSX_VERSION="10.9"
ARCHFLAGS="x86_64"

if [ "$BUILD_UNIVERSAL" = true ] ; then
    ARCHFLAGS="${ARCHFLAGS};arm64"
fi

TEMP_DIR_DEBUG="${TMPDIR}/debug"
TEMP_DIR_RELEASE="${TMPDIR}/release"

# Make release
mkdir -p ${TEMP_DIR_RELEASE} && cd ${TEMP_DIR_RELEASE}
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_OSX_ARCHITECTURES="$ARCHFLAGS" "$CMAKE_BASE_DIR"
make clean && make -j$(get_core_count)
cp -v ${TMPDIR}/libicu.a ${LIB_ROOT_DIR}/lib/Mac/libicu.a

# Make debug
mkdir -p ${TEMP_DIR_DEBUG} && cd ${TEMP_DIR_DEBUG}
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS_DEBUG="-gdwarf-2" -DCMAKE_OSX_ARCHITECTURES="$ARCHFLAGS" "$CMAKE_BASE_DIR"
make clean && make -j$(get_core_count)
cp -v ${TMPDIR}/libicu.a ${LIB_ROOT_DIR}/lib/Mac/libicud.a

# Go back to the libroot so we can check files
cd ${LIB_ROOT_DIR}

##
## Common checks

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

popd > /dev/null
