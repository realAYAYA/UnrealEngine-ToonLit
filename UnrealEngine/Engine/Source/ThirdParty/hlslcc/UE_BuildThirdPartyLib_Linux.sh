#!/bin/bash

if [ -z "$LINUX_ROOT" ]; then 
	echo "Set LINUX_ROOT to the UE clang centos build, available in //depot/CarefullyRedist/Host*/Linux_x64/"
	exit 1
fi

pushd hlslcc/projects/Linux

export TARGET_ARCH=$(uname -m)-unknown-linux-gnu
mkdir ../../lib/Linux/$TARGET_ARCH/
make -j 16

popd

echo .
echo Remember to recompile for other platforms too!
