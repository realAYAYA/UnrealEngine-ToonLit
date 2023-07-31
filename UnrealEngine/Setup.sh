#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -e

cd "`dirname "$0"`"

if [ ! -f Engine/Binaries/DotNET/GitDependencies/linux-x64/GitDependencies ] && [ ! -f Engine/Binaries/DotNET/GitDependencies/osx-x64/GitDependencies ] ; then
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
	if [ -d .git/hooks ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		rm -f .git/hooks/post-checkout
		rm -f .git/hooks/post-merge
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh .git/hooks/post-checkout
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh .git/hooks/post-merge
	fi

	# Get the dependencies for the first time
	Engine/Build/BatchFiles/Mac/GitDependencies.sh "${GitDepsArgs[@]}"
else
	# Setup the git hooks
	if [ -d .git/hooks ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		echo \#!/bin/sh >.git/hooks/post-checkout
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >>.git/hooks/post-checkout
		chmod +x .git/hooks/post-checkout

		echo \#!/bin/sh >.git/hooks/post-merge
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >>.git/hooks/post-merge
		chmod +x .git/hooks/post-merge
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
