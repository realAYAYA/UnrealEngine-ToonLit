#!/bin/bash

## Unreal Engine dotnet setup script
## Copyright Epic Games, Inc. All Rights Reserved.

## This script is expecting to exist in the UE/Engine/Build/BatchFiles directory.  It will not work correctly
## if you copy it to a different location and run it.

echo
echo Running dotnet msbuild...
echo

source "`dirname "$0"`/SetupEnvironment.sh" -dotnet "`dirname "$0"`"

# put ourselves into Engine directory (two up from location of this script)
pushd "`dirname "$0"`/../../.."

if [ ! -f Build/BatchFiles/RunDotnetMSBuild.sh ]; then
	echo RunDotnetMSBuild ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
	exit 1
fi

dotnet msbuild /verbosity:quiet /nologo "$@" |grep -wi error
if [ $? -ne 1 ]; then
	exit 1
else
	exit 0
fi
