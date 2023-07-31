#!/bin/sh

# Copyright Epic Games, Inc. All Rights Reserved.

LIB_NAME="Expat"

# Drops from the location of this script to the relative root location of LIBFILES
DROP_TO_LIBROOT=../..

# Drops from the location of LIBROOT to Engine/Source/ThirdParty
DROP_TO_THIRDPARTY=../..

# Files we build, relative to LIBROOT
LIBFILES=( "Mac/Debug/libexpat.a" "Mac/Release/libexpat.a" )

ENABLE_CHECKOUT_FILES="1"

##
## Common setup steps
##

# Build script will be in <lib>/Build/Mac so get that path and drop two folders to leave us in
# the actual lib folder.
pushd . > /dev/null
SCRIPT_DIR="`dirname "${BASH_SOURCE[0]}"`"
cd ${SCRIPT_DIR}/${DROP_TO_LIBROOT}
LIB_ROOT_DIR=${PWD}
echo Changed to ${LIB_ROOT_DIR}

# We should be in ThirdParty/<libname>/<libname-version> and we want to pull in some command
# things from ThirdParty/BuildScripts/Mac/Common.
source ${DROP_TO_THIRDPARTY}/BuildScripts/Mac/Common/Common.sh

# Create a temp-dir and save it (note the TMPDIR variable is used by the functions that check
# file state).
TMPDIR="/tmp/${LIB_NAME}-$$"
mkdir -p ${TMPDIR}/x86_64 > /dev/null 2>&1
mkdir -p ${TMPDIR}/arm64  > /dev/null 2>&1

echo Rebuilding ${LIB_NAME} using temp path ${TMPDIR}

# Checkout the library list and save their state
if [ "${ENABLE_CHECKOUT_FILES}" = "1" ]; then
    checkoutFiles ${LIBFILES[@]}
fi
saveFileStates ${LIBFILES[@]}

##
## libstrophe specific steps
##

UE_SYSROOT=`xcrun --sdk macosx --show-sdk-path`
UE_C_CXX_LD_FLAGS="-isysroot ${UE_SYSROOT} -mmacosx-version-min=10.14 -gdwarf-2"

# x86_64
pwd
./configure --disable-dependency-tracking \
            --prefix=${TMPDIR}/x86_64 \
              --host=x86_64-apple-darwin19.6.0 \
              CFLAGS="-arch x86_64 ${UE_C_CXX_LD_FLAGS}" \
            CXXFLAGS="-arch x86_64 ${UE_C_CXX_LD_FLAGS}" \
             LDFLAGS="${UE_C_CXX_LD_FLAGS}" \
                  CC=`xcrun -f clang`
                 CXX=`xcrun -f clang++`
make -j$(get_core_count)
make install

if [ "$BUILD_UNIVERSAL" = true ] ; then
    make clean
    make distclean

    # arm64
    ./configure --disable-dependency-tracking \
                --prefix=${TMPDIR}/arm64 \
                  --host=aarch64-apple-darwin19.6.0 \
                  CFLAGS="-arch arm64 ${UE_C_CXX_LD_FLAGS}" \
                CXXFLAGS="-arch arm64 ${UE_C_CXX_LD_FLAGS}" \
                 LDFLAGS="${UE_C_CXX_LD_FLAGS}" \
                      CC=`xcrun -f clang`
                     CXX=`xcrun -f clang++`
    make -j$(get_core_count)
    make install
    make clean
    make distclean

    # lipo the results into universal binaries
    lipo -create ${TMPDIR}/x86_64/lib/libexpat.a ${TMPDIR}/arm64/lib/libexpat.a -output ./Mac/Release/libexpat.a
else
    cp -v ${TMPDIR}/x86_64/lib/libexpat.a ./Mac/Release/libexpat.a
fi

cp -v ./Mac/Release/libexpat.a ./Mac/Debug/libexpat.a

# check the files were all touched
checkFilesWereUpdated ${LIBFILES[@]}
checkFilesAreFatBinaries ${LIBFILES[@]}

echo The following files were rebuilt: ${LIBFILES[@]}

popd > /dev/null
