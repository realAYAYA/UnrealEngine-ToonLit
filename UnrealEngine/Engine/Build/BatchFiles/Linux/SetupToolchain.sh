#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# put ourselves into Engine directory (three up from location of this script)
pushd "`dirname "$0"`/../../.." > /dev/null

if [ -z $GIT_DIR ]; then
	GIT_DIR=$(git rev-parse --git-common-dir);
	if [ $? -ne 0 ]; then
		GIT_DIR=../.git
	fi
fi

# Parse the Toolchain Version from Linux_SDK.json
REGEX='\"MainVersion\"\s*:\s*\"(.*)\"'
[[ $(grep MainVersion Config/Linux/Linux_SDK.json) =~ $REGEX ]]
TOOLCHAIN_VERSION=${BASH_REMATCH[1]}

# Warn if nothing was parsed from Config/Linux/Linux_SDK.json
if [ -z "$TOOLCHAIN_VERSION" ]
then
	echo "Couldn't find current toolchain, check MainVersion in $(realpath "Config/Linux/Linux_SDK.json")"
	exit
fi

TOOLCHAIN_ARCHIVE=$TOOLCHAIN_VERSION.tar.gz

TOOLCHAIN_URL=https://cdn.unrealengine.com/Toolchain_Linux/native-linux-$TOOLCHAIN_ARCHIVE
TOOLCHAIN_ROOT=Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/
TOOLCHAIN_CACHE="$GIT_DIR/ue4-sdks/"

if [ -z $UE_SDK_CACHE_SIZE ]; then
TOOLCHAIN_CACHE_SIZE=2
else
TOOLCHAIN_CACHE_SIZE=$UE_SDK_CACHE_SIZE
fi

if [ -d $TOOLCHAIN_ROOT/$TOOLCHAIN_VERSION ]; then
	echo "Toolchain already installed skipping."
	exit
fi

echo "Downloading toolchain."
mkdir -p $TOOLCHAIN_ROOT

# If this is a git build then cache the downloaded zip
if [ ! -f Build/PerforceBuild.txt ]; then
	mkdir -p $TOOLCHAIN_CACHE
	TOOLCHAIN_ARCHIVE=$TOOLCHAIN_CACHE$TOOLCHAIN_ARCHIVE
else
	TOOLCHAIN_ARCHIVE=$TOOLCHAIN_ROOT$TOOLCHAIN_ARCHIVE
fi

if ! tar tfz ${TOOLCHAIN_ARCHIVE} 2>/dev/null; then
	echo Removing corrupt toolchain and redownloading
	rm -f ${TOOLCHAIN_ARCHIVE}
fi

if [ ! -f $TOOLCHAIN_ARCHIVE ]; then
    rm -f ${TOOLCHAIN_ARCHIVE}.tmp
	if which curl 1>/dev/null; then
		curl $TOOLCHAIN_URL -o ${TOOLCHAIN_ARCHIVE}.tmp || exit 1
	elif which wget 1>/dev/null; then
		wget $TOOLCHAIN_URL -O ${TOOLCHAIN_ARCHIVE}.tmp || exit 1
	else 
		echo "Please install curl or wget"
		exit
	fi
    mv ${TOOLCHAIN_ARCHIVE}.tmp $TOOLCHAIN_ARCHIVE
else
	echo "Using cached toolchain."
fi

if [ -f $TOOLCHAIN_ARCHIVE ]; then
	echo "Extracting toolchain."
	tar -xvf $TOOLCHAIN_ARCHIVE -C $TOOLCHAIN_ROOT || exit 1
	# If this is not a git build then do not cache the downloaded zip
	if [ -f Build/PerforceBuild.txt ]; then
		rm -f $TOOLCHAIN_ARCHIVE
	else
		touch $TOOLCHAIN_ARCHIVE

		# If this is a git build then clean up older toolchains
		SORTED_ARCHIVES_STRING=`ls -ut "$TOOLCHAIN_CACHE"`
		OLD_ARCHIVES=($SORTED_ARCHIVES_STRING)
		# Only allow the n most recent toolchains to be archived.
		INDEX=$TOOLCHAIN_CACHE_SIZE
		NUM_OLD_ARCHIVES=${#OLD_ARCHIVES[@]} 
		while [ $INDEX -lt $NUM_OLD_ARCHIVES ]; do
			echo "Removing stale toolchain ${OLD_ARCHIVES[$INDEX]}"
			rm -f $TOOLCHAIN_CACHE${OLD_ARCHIVES[$INDEX]}
			INDEX=$(($INDEX+1))
		done
	fi
else
	echo "Download failed please fetch from $TOOLCHAIN_URL manually".
fi

