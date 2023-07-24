#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

echo
echo Setting up Unreal Engine project files...
echo

# If ran from somewhere other then the script location we'll have the full base path
BASE_PATH="`dirname "$0"`"

# this is located inside an extra 'Linux' path unlike the Windows variant.

if [ ! -d "$BASE_PATH/../../../Binaries/DotNET" ]; then
	echo GenerateProjectFiles ERROR: It looks like you are missing some files that are required in order to generate projects.  Please check that you have downloaded and unpacked the engine source code, binaries, content and third-party dependencies before running this script.
	exit 1
fi

if [ ! -d "$BASE_PATH/../../../Source" ]; then
	echo GenerateProjectFiles ERROR: This script file does not appear to be located inside the Engine/Build/BatchFiles/Linux directory.
	exit 1
fi

source "$BASE_PATH/SetupEnvironment.sh" -dotnet "$BASE_PATH"
# ensure UnrealBuildTool is up to date if the project file exists, but not if running from an installed build
if [ -f "$BASE_PATH/../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj" -a ! -f "$BASE_PATH/../../../Build/InstalledBuild.txt" ]; then
	"$BASE_PATH/../BuildUBT.sh"
	if [ $? -ne 0 ]; then
		echo GenerateProjectFiles ERROR: Failed to build UnrealBuildTool
		exit 1
	fi
fi


# pass all parameters to UBT
dotnet "$BASE_PATH/../../../Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" -projectfiles "$@"
