#!/bin/sh

## Unreal Engine Mono setup script
## Copyright Epic Games, Inc. All Rights Reserved.

## This script is expecting to exist in the UE4/Engine/Build/BatchFiles directory.  It will not work correctly
## if you copy it to a different location and run it.

echo
echo Running Mono...
echo

source "`dirname "$0"`/SetupEnvironment.sh" -mono "`dirname "$0"`"

# put ourselves into Engine directory (two up from location of this script)
pushd "`dirname "$0"`/../../.."

if [ ! -f Build/BatchFiles/Mac/RunMono.sh ]; then
	echo RunMono ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
	exit 1
fi

mono "$@"
exit $?
