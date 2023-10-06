#!/bin/sh

if [ "$1" == "" ]; then
    echo "Usage: $0 <path to CEF drop>"
    exit 1
fi

# Sniff for the libcef_dll_wrapper.a lib in the CEF drop to confirm a valid location
if [ ! -f "$1/Release/libcef_dll_wrapper.a" ]; then
    echo "\"$1\" is not a valid CEF drop directory;"
    exit 1
fi

# make sure we are in the right spot
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $SCRIPT_DIR

CEF_DROP_DIR=$( basename $1 )

echo "Copying CEF drop into source folder"
# p4 edit the existing folder in case
p4 edit $CEF_DROP_DIR/...
cp -aR "$1" . 
# add all files to catch a new drop
find $CEF_DROP_DIR -type f -exec p4 add {} +
# revert unchanged
p4 revert -a $CEF_DROP_DIR/...
p4 revert .../.DS_Store

echo "Updating binaries"
p4 edit ../../../Binaries/ThirdParty/CEF3/Mac/...
cp -aR "$CEF_DROP_DIR/Release/Chromium Embedded Framework.framework/" "../../../Binaries/ThirdParty/CEF3/Mac/Chromium Embedded Framework x86.framework"
p4 revert -a ../../../Binaries/ThirdParty/CEF3/Mac/...

popd

echo "Drop updated, look in p4 for the new files to submit."