#!/bin/bash

## Unreal Engine UnrealBuildTool build script
## Copyright Epic Games, Inc. All Rights Reserved
##
## This script is expecting to exist in the Engine/Build/BatchFiles directory.  It will not work correctly
## if you copy it to a different location and run it.

## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
## verify that our relative path to the /Engine/Source directory is correct
SCRIPT_DIR=$(cd "`dirname "$0"`" && pwd)
pushd "$SCRIPT_DIR/../../Source" >/dev/null

if [ ! -f ../Build/BatchFiles/BuildUBT.sh ]; then
  echo
  echo "BuildUBT ERROR: The script does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory."
  echo
  popd >/dev/null
  exit 1
fi

MSBuild_Verbosity="${1:-quiet}"

# Check to see if the files in the UnrealBuildTool, EpicGames.Build, EpicGames.Core, or UnrealBuildTool
# directory have changed.

mkdir -p ../Intermediate/Build

PERFORM_REBUILD=0
if [ "$1" == "FORCE" ]; then
  PERFORM_REBUILD=1
  echo "Rebuilding: build requested"

elif [ ! -f ../Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll ]; then
  PERFORM_REBUILD=1
  echo "Rebuilding: UnrealBuildTool assembly not found"

elif [ -f ../Intermediate/Build/UnrealBuildToolLastBuildTime ]; then
  UPDATED_DEP_FILES="$(find \
    Programs/Shared/EpicGames.Build \
    Programs/Shared/EpicGames.Core \
    Programs/Shared/EpicGames.IoHash \
    Programs/Shared/EpicGames.MsBuild \
    Programs/Shared/EpicGames.Serialization \
    Programs/Shared/EpicGames.UHT \
    Programs/UnrealBuildTool \
    ../Restricted/**/Source/Programs/UnrealBuildTool \
    ../Platforms/*/Source/Programs/UnrealBuildTool \
    -type f \
    \( -iname \*.cs -or -iname \*.csproj \) \
    -newer ../Intermediate/Build/UnrealBuildToolLastBuildTime)"
  
  if [ -n "$UPDATED_DEP_FILES" ]; then
    PERFORM_REBUILD=1
    echo "Rebuilding: found updated files:"
    echo "$UPDATED_DEP_FILES"
  fi

  UPDATED_DEP_FILES="$(find \
    Programs/Shared \
    -maxdepth 1 \
    -type f \
    \( -iname \*.cs -or -iname \*.csproj \) \
    -newer ../Intermediate/Build/UnrealBuildToolLastBuildTime)"
  if [ -n "$UPDATED_DEP_FILES" ]; then
    PERFORM_REBUILD=1
    echo "Rebuilding: Found updated files:"
    echo "$UPDATED_DEP_FILES"
  fi

else
  PERFORM_REBUILD=1
  echo "Rebuilding: No record of previous builld"
fi

if [ $PERFORM_REBUILD -eq 1 ]; then
  if [ "$(uname)" = "Darwin" ]; then
    # Setup Environment
    source "$SCRIPT_DIR/Mac/SetupEnvironment.sh" -dotnet "$SCRIPT_DIR/Mac"
  fi

  if [ "$(uname)" = "Linux" ]; then
    # Setup Environment
    source "$SCRIPT_DIR/Linux/SetupEnvironment.sh" -dotnet "$SCRIPT_DIR/Linux"
  fi

  dotnet build Programs/UnrealBuildTool/UnrealBuildTool.csproj -c Development -v $MSBuild_Verbosity
  if [ $? -ne 0 ]; then
    echo "Compilation failed"
    popd >/dev/null
    exit 1
  fi

  touch ../Intermediate/Build/UnrealBuildToolLastBuildTime
else
  echo "UnrealBuildTool.dll is up to date"
fi

popd >/dev/null
exit 0

