#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.
# This script gets can be used to build and clean individual projects using UnrealBuildTool

set -e

cd "`dirname "$0"`/../../../.." 

# Setup Environment and dotnet
source Engine/Build/BatchFiles/Linux/SetupEnvironment.sh -dotnet Engine/Build/BatchFiles/Linux

# Skip UBT and SCW compile step if this is an installed build.
if [ ! -f Engine/Build/InstalledBuild.txt ]; then
	# use two for-loops here as -buildscw depends on UBT being built.
	# So if -buildscw then -buildubt was passed it would fail/use out of date UBT
	for i in "$@" ; do
	# First make sure that the UnrealBuildTool is up-to-date
	if [[ $i == "-buildubt" ]] ; then
		if ! dotnet build Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj -c Development -v quiet; then
			echo "Failed to build the build tool (UnrealBuildTool)"
			exit 1
		fi
	fi
	done

	for i in "$@" ; do
	# build SCW if specified
	if [[ $i == "-buildscw" ]] ; then
		echo Building ShaderCompileWorker...
		dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll ShaderCompileWorker Linux Development
		break
	fi
	done
fi

echo Running command : dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll "$@"
dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll "$@"
exit $?
