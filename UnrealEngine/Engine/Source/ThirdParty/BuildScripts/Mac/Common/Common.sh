#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

# common functions used by individual build scripts for thirdparty libraries

export MACOSX_DEPLOYMENT_TARGET="10.13"
export UE_XC_SYSROOT=`xcrun --sdk macosx --show-sdk-path`

BUILD_UNIVERSAL=true

function abspath() {
    # generate absolute path from relative path
    # $1     : relative filename
    # return : absolute path
    if [ -d "$1" ]; then
        # dir
        (cd "$1"; pwd)
    elif [ -f "$1" ]; then
        # file
        if [[ $1 = /* ]]; then
            echo "$1"
        elif [[ $1 == */* ]]; then
            echo "$(cd "${1%/*}"; pwd)/${1##*/}"
        else
            echo "$(pwd)/$1"
        fi
    fi
}

function realpath() {
  OURPWD=$PWD
  cd "$(dirname "$1")"
  LINK=$(readlink "$(basename "$1")")
  while [ "$LINK" ]; do
    cd "$(dirname "$LINK")"
    LINK=$(readlink "$(basename "$1")")
  done
  REALPATH="$PWD/$(basename "$1")"
  cd "$OURPWD"
  echo "$REALPATH"
}

function checkoutFiles() {
    local success=true
    fileList=("$@")
    for file in "${fileList[@]}"
    do
        echo checking out ${file}
        p4 edit $file
        if [ -f $file ]; then
            if [ ! -w $file ]; then
                echo File $file was not checked out
                success=false
            fi
        fi
    done

    if [ "$success" = true ] ; then
        echo All library files were checked out
    else
        echo Not all library files were checked out. Aborting build.
        exit 1
    fi
}

function saveFileStates() {
    fileList=("$@")
    mkdir -p $TMPDIR/state > /dev/null
    for file in "${fileList[@]}"
    do
        filename=$(basename -- "$file")
        local tmpfile=$TMPDIR/state/$filename
        echo Saving filestate of $file to $tmpfile
        cp -rfp $file $tmpfile
    done
}

function checkFilesWereUpdated() {
    local success=true
    fileList=("$@")
    for file in "${fileList[@]}"
    do
        filename=$(basename -- "$file")
        local tmpfile=$TMPDIR/state/$filename
        if [ ! -f $file ]; then
            echo ${LIB_NAME} required library file $file does not exist.
            success=false
        elif [ ! -f $tmpfile ]; then
            echo ${LIB_NAME}: $tmpfile not found for comparison
            success=false
        elif [ ! $file -nt $tmpfile ]; then
            echo ${LIB_NAME}: $file was not updated.
            success=false
        else
            echo ${LIB_NAME}: $file was updated.
        fi
    done

    if [ "$success" = true ] ; then
        echo All library files were updated
    else
        echo Not all library files were updated. Aborting build.
        exit 1
    fi
}

function get_core_count() {
    sysctl -n hw.ncpu
}

function checkFilesAreFatBinaries() {
    local success=true
    fileList=("$@")
    for file in "${fileList[@]}"
    do
        lipo -info $file | grep 'Architectures in the fat file' &> /dev/null
        if [ $? -ne 0 ]; then
            echo ${LIB_NAME}: $file was not built as a fat-binary
            success=false
        else
            echo ${LIB_NAME}: $file was built as a fat-binary
        fi
    done

    if [ "$BUILD_UNIVERSAL" = true ] ; then
        if [ "$success" = true ] ; then
            echo All library files were built as universal binaries
        else
            echo Not all files were built as univeral. Aborting the build
            exit 1
        fi
    fi
}
