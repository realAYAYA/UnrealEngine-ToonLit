#!/bin/bash

cd $(dirname "$0")
source SetupDotnet.sh

LibPath="$HOME/Library/Unreal Engine/P4VUtils"

echo
echo Copying P4VUtils files to '$LibPath'...
mkdir -p "$LibPath"
cp -f ../../../Extras/P4VUtils/Binaries/Mac/* "$LibPath"
cp -f ../../../Extras/P4VUtils/P4VUtils.ini "$LibPath"
if [ -e ../../../Restricted/NotForLicensees/Extras/P4VUtils ]; then
	mkdir "$LibPath/NotForLicensees"
	cp -f ../../../Restricted/NotForLicensees/Extras/P4VUtils/* "$LibPath/NotForLicensees/"
fi

echo
echo Installing P4VUtils into p4v...
dotnet "$LibPath/P4VUtils.dll" install
