#!/bin/bash
set -e

if [ "$1" = "" ]; then 
    echo "$0 <path to Engine enlistment>";
    exit 1;
fi

if [ -z "$CEF_BRANCH" ]; then
    CEF_BRANCH=4430
fi

if [ ! -d "$1" ]; then 
    echo "Path not found ($1)";
    exit 1;
fi

if [ ! -d "$1"/Engine/Source/ThirdParty/CEF3 ]; then
    echo "Invalid Engine sync, Engine/Source/ThirdParty/CEF3 folder missing"
    exit 1;
fi

if [ ! -d "$1"/Engine/Binaries/ThirdParty/CEF3 ]; then
    echo "Invalid Engine sync, Engine/Binaries/ThirdParty/CEF3 folder missing"
    exit 1;
fi

if [ -z "$CEF_BUILD_DIR" ]; then
    CEF_BUILD_DIR=`pwd`/build/cef_$CEF_BRANCH
fi

BINARY_DIST_DIR="`find $CEF_BUILD_DIR/chromium/src/cef/binary_distrib -iname README.txt -exec dirname {} \;`"

echo "### Copying drop to Source folder"
cp -aR $BINARY_DIST_DIR "$1"/Engine/Source/ThirdParty/CEF3
echo 
echo "### Updating binaries in drop folder"
cp -aR $BINARY_DIST_DIR/Release/Chromium\ Embedded\ Framework.framework "$1"/Engine/Binaries/ThirdParty/CEF3/Mac

echo "Drop updated. Use \"p4 reconcile Binaries/ThirdParty/CEF3/Mac/... Source/ThirdParty/CEF3/...\" to update p4"