#!/bin/bash

## Unreal Engine 4 Build script for Expat
## Copyright Epic Games, Inc. All Rights Reserved.

# Should be run in docker image, launched something like this (see RunMe.sh script):
#   docker run --name ${ImageName} -v ${SCRIPT_DIR}:/src ${Image} /src/docker-build-expat.sh
#
# Built expat libraries are in /build directory

if [ $UID -eq 0 ]; then
  # Centos 7
  yum install -y make gcc-c++ wget

  # Cmake
  wget --no-check-certificate https://cmake.org/files/v3.12/cmake-3.12.3.tar.gz
  tar zxvf cmake-3.*
  cd cmake-3.*
  ./bootstrap --prefix=/usr/local
  make -j$(getconf _NPROCESSORS_ONLN)
  make install

  # Create non-privileged user and workspace
  adduser buildmaster
  mkdir -p /build
  chown buildmaster:nobody -R /build
  cd /build

  exec su buildmaster "$0"
fi

# This will be run from user buildmaster

export EXPAT_DIR=/expat

# Get num of cores
export CORES=$(getconf _NPROCESSORS_ONLN)
echo Using ${CORES} cores for building

BuildWithOptions()
{
	local BuildDir=$1
	shift 1
	local Options="$@"

	rm -rf $BuildDir
	mkdir -p $BuildDir
	pushd $BuildDir

	cmake $Options ${EXPAT_DIR}
	make -j${CORES}

	popd
}

set -e

BuildWithOptions Debug   -DCMAKE_BUILD_TYPE=Debug   -DCMAKE_C_FLAGS="-fPIC -gdwarf-4" -DEXPAT_BUILD_TOOLS=0 -DEXPAT_BUILD_EXAMPLES=0 -DEXPAT_BUILD_TESTS=0 -DEXPAT_SHARED_LIBS=0
BuildWithOptions Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-fPIC -gdwarf-4" -DEXPAT_BUILD_TOOLS=0 -DEXPAT_BUILD_EXAMPLES=0 -DEXPAT_BUILD_TESTS=0 -DEXPAT_SHARED_LIBS=0

set +e

