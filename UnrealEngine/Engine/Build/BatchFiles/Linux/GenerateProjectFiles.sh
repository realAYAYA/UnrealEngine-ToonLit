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

# If this is a source drop of the engine make sure that the UnrealBuildTool is up-to-date
if [ ! -f "$BASE_PATH/../../InstalledBuild.txt" ]; then
	if [ -f "$BASE_PATH/../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj" ]; then
		dotnet build "$BASE_PATH/../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj" -c Development -v quiet
		if [ $? -ne 0 ]; then
			echo GenerateProjectFiles ERROR: Failed to build UnrealBuildTool
			exit 1
		fi
	fi
fi

# pass all parameters to UBT
dotnet "$BASE_PATH/../../../Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" -projectfiles "$@"
