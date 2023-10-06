#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

cd $(dirname "$0")

DestDir="$HOME/Library/Unreal Engine/P4VUtils"

SourceDir=../../../Extras/P4VUtils/Binaries/Linux
Message="Installing P4VUtils into p4v..."
if [ -e ../../../Restricted/NotForLicensees/Extras/P4VUtils ]; then
	Message="Installing P4VUtils [with Epic extensions] into p4v..."
	SourceDir=../../../Restricted/NotForLicensees/Extras/P4VUtils/Binaries/Linux
fi

echo
echo "Copying P4VUtils files to $DestDir..."
rm -rf "$DestDir"
mkdir -p "$DestDir"
cp -r "$SourceDir/." "$DestDir"

echo
echo $Message
"$DestDir/P4VUtils" install
