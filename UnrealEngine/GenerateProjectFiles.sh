#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

set -e

cd "`dirname "$0"`"

if [ ! -f Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh ]; then
	echo "GenerateProjectFiles ERROR: This script does not appear to be located \
       in the root Unreal Engine directory and must be run from there."
  exit 1
fi 

if [ -f Setup.sh ]; then
	if [ ! -f .uedependencies ] && [ ! -f .ue4dependencies ]; then
		echo "Please run Setup to download dependencies before generating project files."
		exit 1
	fi
fi

if [ "$(uname)" = "Darwin" ]; then
	cd Engine/Build/BatchFiles/Mac
	sh ./GenerateLLDBInit.sh
	sh ./GenerateProjectFiles.sh "$@"
else
	# assume (GNU/)Linux
	cd Engine/Build/BatchFiles/Linux
	bash ./GenerateLLDBInit.sh
	bash ./GenerateGDBInit.sh
	bash ./GenerateProjectFiles.sh "$@"
fi
