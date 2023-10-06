#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

set -e

SCRIPT_PATH=$0
if [ -L "$SCRIPT_PATH" ]; then
	SCRIPT_PATH="$(dirname "$SCRIPT_PATH")/$(readlink "$SCRIPT_PATH")"
fi

cd "$(dirname "$SCRIPT_PATH")" && SCRIPT_PATH="`pwd`/$(basename "$SCRIPT_PATH")"

cd ../../../..

# Select the preferred architecture for the current system
ARCH=x64
[ $(uname -m) == "arm64" ] && ARCH=arm64 

./Engine/Binaries/DotNET/GitDependencies/osx-$ARCH/GitDependencies "$@"

pushd "$(dirname "$SCRIPT_PATH")" > /dev/null
sh FixDependencyFiles.sh
popd > /dev/null
