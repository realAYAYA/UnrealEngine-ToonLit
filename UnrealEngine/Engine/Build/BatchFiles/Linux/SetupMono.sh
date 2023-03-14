#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# Fix Mono and engine dependencies if needed
START_DIR=`pwd`
cd "$1"

export HOST_ARCH=x86_64-unknown-linux-gnu

bash FixMonoFiles.sh
bash FixDependencyFiles.sh
IS_MONO_INSTALLED=0
IS_MS_BUILD_AVAILABLE=0
MONO_VERSION_PATH=$(command -v mono) || true

if [ "$UE_USE_SYSTEM_MONO" == "1" ] && [ ! $MONO_VERSION_PATH == "" ] && [ -f $MONO_VERSION_PATH ]; then
	# If Mono is installed, check if it's 5.0 or higher
	MONO_VERSION_PREFIX="Mono JIT compiler version "
	MONO_VERSION_PREFIX_LEN=${#MONO_VERSION_PREFIX}
	MONO_VERSION=`"${MONO_VERSION_PATH}" --version |grep "$MONO_VERSION_PREFIX"`
	MONO_VERSION=(`echo ${MONO_VERSION:MONO_VERSION_PREFIX_LEN} |tr '.' ' '`)
	if [ ${MONO_VERSION[0]} -ge 5 ]; then
		if [ ${MONO_VERSION[1]} -ge 0 ] || [ ${MONO_VERSION[2]} -ge 2 ]; then
			IS_MONO_INSTALLED=1

			# see if msbuild is installed
			MS_BUILD_PATH=`which msbuild` || true
			if [ -f "$MS_BUILD_PATH" ]; then
				IS_MS_BUILD_AVAILABLE=1
				echo "Running system mono/msbuild, version: ${MONO_VERSION}"
			else
				echo "Running system mono, version: ${MONO_VERSION}"
			fi
		fi
	fi
fi

# Setup bundled Mono if cannot use installed one
if [ $IS_MONO_INSTALLED -eq 0 ]; then
	echo Setting up Mono
	CUR_DIR=`pwd`
	export UE_MONO_DIR=$CUR_DIR/../../../Binaries/ThirdParty/Mono/Linux
	export PATH=$UE_MONO_DIR/bin:$PATH
	export MONO_PATH=$UE_MONO_DIR/lib/mono/4.5:$MONO_PATH
	export MONO_CFG_DIR=$UE_MONO_DIR/etc
	export LD_LIBRARY_PATH=$UE_MONO_DIR/$HOST_ARCH/lib:$LD_LIBRARY_PATH
else
	export IS_MONO_INSTALLED=$IS_MONO_INSTALLED
	export IS_MS_BUILD_AVAILABLE=$IS_MS_BUILD_AVAILABLE
fi

cd "$START_DIR"
