#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

set -e
set -x

if [ -z ${UE_SDKS_ROOT+x} ]; then
    echo "UE_SDKS_ROOT is unset";
    exit -1;
fi

pushd `dirname "$0"`/..
PluginSrcPath=`pwd`
cd "$PluginSrcPath/../../../../.."
EnginePath=`pwd`
popd

mkdir -p /tmp/Python3/Deploy
ln -f -s "$EnginePath/Binaries/ThirdParty/Python3/Mac/lib" /tmp/Python3/Deploy

Intermediatex86_64Path="$EnginePath/Intermediate/Build/Mac/x86_64/SketchUp"
IntermediateUniversalPath="$EnginePath/Intermediate/Build/Mac/arm64+x86_64/SketchUp"

SetupIntermediateDirectory() {
    IntermediatePath="${1}"

    rm -rf "$IntermediatePath"
    mkdir -p "$IntermediatePath"

    dylibLibFreeImage=libfreeimage-3.18.0.dylib
    dylibLibTbb=libtbb.dylib
    dylibLibTbbMalloc=libtbbmalloc.dylib

    # Copy and fixup dylibs
    Dylibs="$IntermediatePath/Dylibs"
    rm -rf "$Dylibs"
    mkdir -p "$Dylibs"

    cp "$EnginePath/Binaries/ThirdParty/FreeImage/Mac/$dylibLibFreeImage" "$Dylibs"
    cp "$EnginePath/Binaries/Mac/DatasmithSDK/DatasmithSDK.dylib" "$Dylibs"
    cp "$EnginePath/Binaries/ThirdParty/Intel/TBB/Mac/$dylibLibTbb" "$Dylibs"
    cp "$EnginePath/Binaries/ThirdParty/Intel/TBB/Mac/$dylibLibTbbMalloc" "$Dylibs"

    chmod 777 "$Dylibs/$dylibLibFreeImage"
    chmod 777 "$Dylibs/$dylibLibTbb"
    chmod 777 "$Dylibs/$dylibLibTbbMalloc"

    install_name_tool -id @loader_path/Dylibs/DatasmithSDK.dylib "$Dylibs/DatasmithSDK.dylib" > /dev/null 2>&1
    install_name_tool -id @loader_path/Dylibs/$dylibLibFreeImage "$Dylibs/$dylibLibFreeImage" > /dev/null 2>&1
    install_name_tool -change @rpath/$dylibLibFreeImage @loader_path/$dylibLibFreeImage "$Dylibs/DatasmithSDK.dylib" > /dev/null 2>&1
}

# Technicaly our dylibs are already Universal Arm64+x86_64 binaries, so they shouldn't really go into the x86_64 folder,
# but for the sake of simplicity we keep all the x86_64 dependencies into the x86_64 Intermediate folder with the rest.
SetupIntermediateDirectory "$Intermediatex86_64Path"
SetupIntermediateDirectory "$IntermediateUniversalPath"

BuildSketchUpPlugin() {
    SUVERSION=${1}
    SUSDKVERSION=${2}
	ARCHS=${3}
    IntermediateDir="${4}/$SUVERSION"
    Dylibs="${4}/Dylibs"

    # Compile plugin bundle
    # BuildDir="$PluginSrcPath/.build/$SUVERSION"
    BuildDir="$EnginePath/Binaries/Mac/SketchUp/$SUVERSION"
    rm -rf "$BuildDir"
    mkdir -p "$BuildDir"
    #DEBUG_BUILD_SCRIPT="--no-compile --verbose"
    #DEBUG_BUILD_SCRIPT="--verbose"
    DEBUG_BUILD_SCRIPT=
    "$EnginePath/Binaries/ThirdParty/Python3/Mac/bin/python3" "$PluginSrcPath/Build/build_mac.py" --multithread --sdks-root="$UE_SDKS_ROOT" --sketchup-version=$SUVERSION --sketchup-sdk-version=$SUSDKVERSION --target_archs $ARCHS --output-path="$BuildDir" --intermediate-path="$IntermediateDir" --datasmithsdk-lib="$Dylibs/DatasmithSDK.dylib" $DEBUG_BUILD_SCRIPT

    # Copy plugin files as they should b ein the plugin folder:
    # ruby code
    cp -r "$PluginSrcPath/Plugin" "$BuildDir"
	# ruby code needs to be writable for the plugin to be "un-installable"
	chmod -R 777 "$BuildDir"
    # support libs
    cp -r "$Dylibs" "$BuildDir/Plugin/UnrealDatasmithSketchUp"
    # resources
    cp -r "$PluginSrcPath/Resources/Windows" "$BuildDir/Plugin/UnrealDatasmithSketchUp"
    mv "$BuildDir/Plugin/UnrealDatasmithSketchUp/Windows" "$BuildDir/Plugin/UnrealDatasmithSketchUp/Resources"

    #version file
    "$EnginePath/Binaries/ThirdParty/Python3/Mac/bin/python3" "$PluginSrcPath/Build/create_version_file.py" "$EnginePath" "$BuildDir/Plugin/UnrealDatasmithSketchUp/version"
}

BuildSketchUpPlugin 2019 SDK_Mac_2019-3-252 "x86_64" "$Intermediatex86_64Path"
BuildSketchUpPlugin 2020 SDK_Mac_2020-2-171 "x86_64" "$Intermediatex86_64Path"
BuildSketchUpPlugin 2021 SDK_Mac_2021-0-338 "x86_64" "$Intermediatex86_64Path"
BuildSketchUpPlugin 2022 SDK_Mac_2022-0-353 "x86_64 arm64" "$IntermediateUniversalPath"
BuildSketchUpPlugin 2023 SDK_Mac_2023-0-366 "x86_64 arm64" "$IntermediateUniversalPath"
BuildSketchUpPlugin 2024 SDK_Mac_2024-0-483 "x86_64 arm64" "$IntermediateUniversalPath"


# install_name_tool -change @rpath/DatasmithSDK.dylib @loader_path/Dylibs/DatasmithSDK.dylib DatasmithSketchUp.bundle 
# install_name_tool -change @loader_path/libfreeimage-3.18.0.dylib @loader_path/Dylibs/libfreeimage-3.18.0.dylib DatasmithSketchUp.bundle

rm -f "$Intermediatex86_64Path/Packages"
mkdir -p "$Intermediatex86_64Path/Packages"

rm -f "$IntermediateUniversalPath/Packages"
mkdir -p "$IntermediateUniversalPath/Packages"

rm -rf /tmp/Python3

