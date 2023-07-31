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
LIB_NAME="WebM"
# Drops from the location of this script to where libfiles are relative to
#  e.g.
#  {DROP_TO_LIBROOT}/README
#  {DROP_TO_LIBROOT}/include)
#  ${DROP_TO_LIBROOT}/$LIBFILES[0])
DROP_TO_LIBROOT=../..
# Drops from the location of LIBROOT to Engine/Source/ThirdParrty
DROP_TO_THIRDPARTY=../../../../../../Source/ThirdParty

# Path to libs from libroot
LIB_PATH=lib/Mac

# files we build
LIBFILES=( 
    "${LIB_PATH}/libwebm.a"
    "${LIB_PATH}/libwebm_fPIC.a"
)

##
## Common setup steps

# Build script will be in <lib>/Build/Mac so get that path and drop two folders to leave us
# in the actual lib folder
pushd . > /dev/null
SCRIPT_DIR="$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)"
cd ${SCRIPT_DIR}/${DROP_TO_LIBROOT}
LIB_ROOT_DIR=${PWD}
echo Changed to ${LIB_ROOT_DIR}

# We should be in ThirdParty/LibName and we want to pull in some common things from
# ThirdParty/BuildScripts/Mac/Common
source ${DROP_TO_THIRDPARTY}/BuildScripts/Mac/Common/Common.sh

echo Rebuilding ${LIB_NAME} in $PWD

# create a tempdir and save it (note the tmpdir variable is used by the functions that 
# check file state)
TMPDIR="/tmp/${LIB_NAME}-$$"
mkdir -p ${TMPDIR} > /dev/null 2>&1

# checkout the library list and save their state (uses TMPDIR)
checkoutFiles ${LIBFILES[@]}
saveFileStates ${LIBFILES[@]}

####################
# libm specific steps

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=libwebm-1.0.0.27

# don't forget to match archive options with tarball type (bz/gz)
TARBALL=${SCRIPT_DIR}/../$VER.tar.bz2

# includ PID in scratch dir - needs to be absolute
SCRATCH_DIR=${TMPDIR}/build-$$
SOURCE_DIR=$SCRATCH_DIR/$VER
BUILD_DIR=$SCRATCH_DIR/build

#####################
# unpack

rm -rf $SCRATCH_DIR
mkdir -p $SCRATCH_DIR

echo "#######################################"
echo "# Unpacking the tarballs"
tar xjf $TARBALL -C $SCRATCH_DIR

if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# tarball $PWD/$TARBALL not found !"
	echo ""
	exit 1
fi

#####################
# build

OSX_ARCHITECTURES="x86_64"
if [ "$BUILD_UNIVERSAL" = true ] ; then
	OSX_ARCHITECTURES="${OSX_ARCHITECTURES};arm64"
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR
cp ${SCRIPT_DIR}/CMakeLists.txt $SOURCE_DIR/CMakeLists.txt

# Unreal uses the dwarf-2 format.  Updating it will require removing '-gdwarf-2' from:
#   Engine/Source/Programs/UnrealBuildTool/Platform/Mac/MacToolChain.cs
export CFLAGS="-gdwarf-2"
export CXXFLAGS="-gdwarf-2"

echo "#######################################"
echo "# Configuring $VER"
cmake -DCMAKE_OSX_ARCHITECTURES="${OSX_ARCHITECTURES}" $SOURCE_DIR > $SCRIPT_DIR/build.log
echo "# Building $VER"
make -j$(get_core_count) webm >> $SCRIPT_DIR/build.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $BUILD_DIR/libwebm.a ${LIB_ROOT_DIR}/${LIB_PATH}

#####################
# build PIC version

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cp ${SCRIPT_DIR}/CMakeLists_Editor.txt $SOURCE_DIR/CMakeLists.txt
echo "#######################################"
echo "# Configuring $VER with PIC"
cmake -DCMAKE_OSX_ARCHITECTURES="${OSX_ARCHITECTURES}" $SOURCE_DIR > $SCRIPT_DIR/build-pic.log
echo "# Building $VER with PIC"
make -j$(get_core_count) webm >> $SCRIPT_DIR/build-pic.log

if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $BUILD_DIR/libwebm.a ${LIB_ROOT_DIR}/${LIB_PATH}/libwebm_fPIC.a

if [ $? -eq 0 ]; then
	echo ""
	echo "#######################################"
	echo "# Newly built libs have been put into ${LIB_ROOT_DIR}/${LIB_PATH}"
	echo ""
fi

# back to where our libs are relative to
cd ${LIB_ROOT_DIR}

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}

checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

popd > /dev/null
