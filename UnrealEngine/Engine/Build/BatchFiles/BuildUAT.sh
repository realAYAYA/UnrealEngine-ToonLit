#!/bin/bash

## Unreal Engine AutomationTool build script
## Copyright Epic Games, Inc. All Rights Reserved
##
## This script is expecting to exist in the Engine/Build/BatchFiles directory.  It will not work correctly
## if you copy it to a different location and run it.

## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
## verify that our relative path to the /Engine/Source directory is correct
SCRIPT_DIR=$(cd "`dirname "$0"`" && pwd)
pushd "$SCRIPT_DIR/../../Source" >/dev/null

if [ ! -f ../Build/BatchFiles/BuildUAT.sh ]; then
  echo
  echo "BuildUAT ERROR: The script does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory."
  echo
  popd >/dev/null
  exit 1
fi

MSBuild_Verbosity="${1:-quiet}"

# Check to see if the files in the AutomationTool, EpicGames.Build, EpicGames.Core, or UnrealBuildTool
# directory have changed.

mkdir -p ../Intermediate/Build

PERFORM_REBUILD=0
if [ "$1" == "FORCE" ]; then
  PERFORM_REBUILD=1
  echo "Rebuilding: build requested"

elif [ ! -f ../Binaries/DotNET/AutomationTool/AutomationTool.dll ]; then
  PERFORM_REBUILD=1
  echo "Rebuilding: AutomationTool assembly not found"

elif [ -f ../Intermediate/Build/AutomationToolLastBuildTime ]; then
  UPDATED_DEP_FILES="$(find \
    Programs/Shared/EpicGames.Build \
    Programs/Shared/EpicGames.Core \
    Programs/Shared/EpicGames.Horde \
    Programs/Shared/EpicGames.IoHash \
    Programs/Shared/EpicGames.MsBuild \
    Programs/Shared/EpicGames.OIDC \
    Programs/Shared/EpicGames.Serialization \
    Programs/Shared/EpicGames.UBA \
    Programs/Shared/EpicGames.UHT \
    Programs/UnrealBuildTool \
    ../Restricted/**/Source/Programs/Shared \
    ../Platforms/*/Source/Programs/Shared \
    ../Restricted/**/Source/Programs/UnrealBuildTool \
    ../Platforms/*/Source/Programs/UnrealBuildTool \
    ../Restricted/**/Source/Programs/AutomationTool \
    ../Platforms/*/Source/Programs/AutomationTool \
    -type f \
    \( -iname \*.cs -or -iname \*.csproj \) \
    -newer ../Intermediate/Build/AutomationToolLastBuildTime)"
  
  if [ -n "$UPDATED_DEP_FILES" ]; then
    PERFORM_REBUILD=1
    echo "Rebuilding: found updated files:"
    echo "$UPDATED_DEP_FILES"
  fi

  UPDATED_AUTOMATIONTOOL_FILES="$(find \
    Programs/Shared \
    Programs/AutomationTool \
    -maxdepth 1 \
    -type f \
    \( -iname \*.cs -or -iname \*.csproj \) \
    -newer ../Intermediate/Build/AutomationToolLastBuildTime)"
  if [ -n "$UPDATED_AUTOMATIONTOOL_FILES" ]; then
    PERFORM_REBUILD=1
    echo "Rebuilding: Found updated files:"
    echo "$UPDATED_AUTOMATIONTOOL_FILES"
  fi

  UPDATED_DEP_FILES="$(find \
    ../Binaries/Linux* \
    ../Binaries/Mac \
    -maxdepth 2 \
    -type f \
    \( -iname \*Uba* \) \
    -newer ../Intermediate/Build/AutomationToolLastBuildTime)"
  if [ -n "$UPDATED_DEP_FILES" ]; then
    PERFORM_REBUILD=1
    echo "Rebuilding: Found updated files:"
    echo "$UPDATED_DEP_FILES"
  fi

else
  PERFORM_REBUILD=1
  echo "Rebuilding: No record of previous build"
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

  dotnet build Programs/AutomationTool/AutomationTool.csproj -c Development -v $MSBuild_Verbosity
  if [ $? -ne 0 ]; then
    echo "Compilation failed"
    popd >/dev/null
    exit 1
  fi

  touch ../Intermediate/Build/AutomationToolLastBuildTime
else
  echo "AutomationTool.dll is up to date"
fi

popd >/dev/null
exit 0

