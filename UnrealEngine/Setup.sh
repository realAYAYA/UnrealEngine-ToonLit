#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

if [ -z $GIT_DIR ]; then
	GIT_DIR=$(git rev-parse --git-common-dir);
	if [ $? -ne 0 ]; then
		GIT_DIR=.git
	fi
fi

set -e

cd "`dirname "$0"`"

# Select the preferred architecture for the current system
ARCH=x64
[ $(uname -m) == "arm64" ] && ARCH=arm64 

if [ ! -f Engine/Binaries/DotNET/GitDependencies/linux-x64/GitDependencies ] && [ ! -f Engine/Binaries/DotNET/GitDependencies/osx-$ARCH/GitDependencies ] ; then
	echo "GitSetup ERROR: This script does not appear to be located \
	   in the root UE directory and must be run from there."
	exit 1
fi

function FindPromptOrForceArg()
{
	read -r -a Args <<< "$@"
	for Arg in "${Args[@]}"
	do
		case $Arg in
				--prompt)
					echo "${Arg}"
					return
				;;
				--force)
					echo "${Arg}"
					return
				;;
		esac
	done
}

declare -a GitDepsArgs
PromptOrForce=$(FindPromptOrForceArg "$@")
if [ -z "${PromptOrForce}" ];
then
	# Use --prompt by default, whenever caller hasn't supplied a --prompt or --force arg
  GitDepsArgs=(--prompt)
fi
GitDepsArgs+=("$@")

if [ "$(uname)" = "Darwin" ]; then
	# Setup the git hooks
	if [ -d "$GIT_DIR/hooks" ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		rm -f "$GIT_DIR/hooks/post-checkout"
		rm -f "$GIT_DIR/hooks/post-merge"
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh "$GIT_DIR/hooks/post-checkout"
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh "$GIT_DIR/hooks/post-merge"
	fi

	# Get the dependencies for the first time
	Engine/Build/BatchFiles/Mac/GitDependencies.sh "${GitDepsArgs[@]}"
else
	# Setup the git hooks
	if [ -d "$GIT_DIR/hooks" ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		echo \#!/bin/sh > "$GIT_DIR/hooks/post-checkout"
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >> "$GIT_DIR/hooks/post-checkout"
		chmod +x "$GIT_DIR/hooks/post-checkout"

		echo \#!/bin/sh > "$GIT_DIR/hooks/post-merge"
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >> "$GIT_DIR/hooks/post-merge"
		chmod +x "$GIT_DIR/hooks/post-merge"
	fi

	# Get the dependencies for the first time
	Engine/Build/BatchFiles/Linux/GitDependencies.sh "${GitDepsArgs[@]}"

	echo Register the engine installation...
	if [ -f Engine/Binaries/Linux/UnrealVersionSelector-Linux-Shipping ]; then
		pushd Engine/Binaries/Linux > /dev/null
		./UnrealVersionSelector-Linux-Shipping -register > /dev/null &
		popd > /dev/null
	fi

	pushd Engine/Build/BatchFiles/Linux > /dev/null
	./Setup.sh "$@"
	popd > /dev/null
fi
