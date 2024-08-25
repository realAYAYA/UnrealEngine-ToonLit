#!/bin/bash

## Unreal Engine dotnet setup script
## Copyright Epic Games, Inc. All Rights Reserved.

## This script is expecting to exist in the UE/Engine/Build/BatchFiles directory.  It will not work correctly
## if you copy it to a different location and run it.

EnvironmentType=-dotnet

# If ran from somewhere other then the script location we'll have the full base path
BASE_PATH="`dirname "$0"`"

echo
echo Running Dotnet...
echo

if [ "$(uname)" = "Darwin" ]; then
	# Setup Environment
	source "$BASE_PATH/Mac/SetupEnvironment.sh" $EnvironmentType "$BASE_PATH/Mac"
fi

if [ "$(uname)" = "Linux" ]; then
	# Setup Environment
	source "$BASE_PATH/Linux/SetupEnvironment.sh" $EnvironmentType "$BASE_PATH/Linux"
fi

# put ourselves into Engine directory (two up from location of this script)
pushd "`dirname "$0"`/../.."

if [ ! -f Build/BatchFiles/RunDotnet.sh ]; then
	echo RunDotnet ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
	exit 1
fi

dotnet "$@"
exit $?
