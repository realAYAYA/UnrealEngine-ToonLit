#!/bin/bash

## Unreal Engine 4 Build script for SDL2
## Copyright Epic Games, Inc. All Rights Reserved.

pushd "`dirname "$0"`/"

if [ -z "$TARGET_ARCH" ]; then
	TARGET_ARCH=x86_64-unknown-linux-gnu
fi

export VULKAN_SDK=`pwd`/../Vulkan
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:$PKG_CONFIG_PATH

BuildWithOptions()
{
	local StaticLibName=$1
	local BuildDir=$2
	local SdlDir=$3
	local SdlLibName=$4
	shift
	shift
	shift
	shift
	local Options=$@

	rm -rf $BuildDir
	mkdir -p $BuildDir
	pushd $BuildDir

	cmake $Options $SdlDir
	make -j 4
	mkdir -p $SdlDir/lib/Linux/$TARGET_ARCH/
	cp --remove-destination $StaticLibName $SdlDir/lib/Linux/$TARGET_ARCH/$SdlLibName
	popd
}

set -e
SDL_DIR=SDL-gui-backend
BUILD_DIR=build-$SDL_DIR

# build Debug with -fPIC so it's usable in any type of build
BuildWithOptions libSDL2d.a $BUILD_DIR-Debug ../$SDL_DIR libSDL2_fPIC_Debug.a -DCMAKE_BUILD_TYPE=Debug -DSDL_STATIC_PIC=ON -DVIDEO_MIR=OFF -DVIDEO_KMSDRM=OFF -DCMAKE_C_FLAGS=-gdwarf-4
#exit 0
BuildWithOptions libSDL2.a $BUILD_DIR-Release ../$SDL_DIR libSDL2.a -DCMAKE_BUILD_TYPE=Release -DVIDEO_MIR=OFF -DVIDEO_KMSDRM=OFF -DCMAKE_C_FLAGS=-gdwarf-4
BuildWithOptions libSDL2.a $BUILD_DIR-ReleasePIC ../$SDL_DIR libSDL2_fPIC.a -DCMAKE_BUILD_TYPE=Release -DSDL_STATIC_PIC=ON -DVIDEO_MIR=OFF -DVIDEO_KMSDRM=OFF -DCMAKE_C_FLAGS=-gdwarf-4
set +e
