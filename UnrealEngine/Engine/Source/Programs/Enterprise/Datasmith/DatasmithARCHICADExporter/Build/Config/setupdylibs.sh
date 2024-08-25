#!/bin/sh

set -e

ConfigPath=`dirname "$0"`
pushd $ConfigPath/..
projectPath=`pwd`
cd ../../../../../..
EnginePath=`pwd`
popd

echo "UE_SDKS_ROOT = ${UE_SDKS_ROOT}"

if [ -z ${UE_SDKS_ROOT+x} ]; then
    echo "UE_SDKS_ROOT is unset";
    exit -1;
fi

if [ ! -f "$ConfigPath/SDKsRoot.xcconfig" ]; then
    echo "Create $ConfigPath/SDKsRoot.xcconfig"
    echo UESDKRoot = $UE_SDKS_ROOT > "$ConfigPath/SDKsRoot.xcconfig"
    echo UE_Engine = $EnginePath >> "$ConfigPath/SDKsRoot.xcconfig"
fi

# Remove ArchiCAD resource tool from quarantine
pushd "$UE_SDKS_ROOT/HostMac/Mac/Archicad"
	chmod 777 23.1/Support/Tools/OSX/ResConv
	xattr -r -d com.apple.quarantine 23.1/Support/Tools/OSX/ResConv

	chmod 777 24/Support/Tools/OSX/ResConv
	xattr -r -d com.apple.quarantine 24/Support/Tools/OSX/ResConv

	chmod 777 25/Support/Tools/OSX/ResConv
	xattr -r -d com.apple.quarantine 25/Support/Tools/OSX/ResConv

	chmod 777 26/Support/Tools/OSX/ResConv
	xattr -r -d com.apple.quarantine 26/Support/Tools/OSX/ResConv

	chmod 777 27/Support/Tools/OSX/ResConv
	xattr -r -d com.apple.quarantine 27/Support/Tools/OSX/ResConv
popd

OurDylibFolder=$projectPath/Dylibs

mkdir -p "$OurDylibFolder"

dylibLibFreeImage=libfreeimage-3.18.0.dylib
dylibtbb=libtbb.dylib
dylibtbbmalloc=libtbbmalloc.dylib

SetUpThirdPartyDll() {
	DylibName=$1
	DylibPath=$2
	if [[ "$DylibPath" -nt "$OurDylibFolder/$DylibName" ]]; then
		if [ -f "$OurDylibFolder/$DylibName" ]; then
			unlink "$OurDylibFolder/$DylibName"
		fi
	fi
	if [ ! -f "$OurDylibFolder/$DylibName" ]; then
		echo "Copy $DylibName"
		cp "$DylibPath" "$OurDylibFolder"
		chmod +w "$OurDylibFolder/$DylibName"
		install_name_tool -id @loader_path/$DylibName "$OurDylibFolder/$DylibName" > /dev/null 2>&1
	fi
}

SetUpDll() {
	DylibName=$1
	OriginalDylibPath="$EnginePath/Binaries/Mac/DatasmithUE4ArchiCAD/$DylibName"

	if [[ "$OriginalDylibPath" -nt "$OurDylibFolder/$DylibName" ]]; then
		if [ -f "$OurDylibFolder/$DylibName" ]; then
			unlink "$OurDylibFolder/$DylibName"
		fi
	fi
	if [ ! -f "$OurDylibFolder/$DylibName" ]; then
		if [ -f "$OriginalDylibPath" ]; then
			echo "Copy $DylibName"
			cp "$OriginalDylibPath" "$OurDylibFolder"
			install_name_tool -id @loader_path/$DylibName "$OurDylibFolder/$DylibName" > /dev/null 2>&1
			install_name_tool -change @rpath/$dylibLibFreeImage @loader_path/$dylibLibFreeImage "$OurDylibFolder/$DylibName" > /dev/null 2>&1
		else
			echo "Missing $DylibName"
		fi
	fi
}

SetUpThirdPartyDll $dylibLibFreeImage "$EnginePath/Binaries/ThirdParty/FreeImage/Mac/$dylibLibFreeImage"
SetUpThirdPartyDll $dylibtbb "$EnginePath/Binaries/ThirdParty/Intel/TBB/Mac/$dylibtbb"
SetUpThirdPartyDll $dylibtbbmalloc "$EnginePath/Binaries/ThirdParty/Intel/TBB/Mac/$dylibtbbmalloc"

SetUpDll DatasmithUE4ArchiCAD.dylib
SetUpDll DatasmithUE4ArchiCAD-Mac-Debug.dylib
