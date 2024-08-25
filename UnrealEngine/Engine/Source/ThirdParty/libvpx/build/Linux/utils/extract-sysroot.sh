#!/usr/bin/env bash

usage ()
{
cat << EOF

Usage:
   $0 /path/to/UnrealEngine

Extracts Sysroot for Linux build

EOF
}

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Verify that the user specified a path to the Unreal Engine source tree from which to extract our sysroot
if [[ -z "$1" ]]; then
	usage;
	exit 1
fi
UNREAL_ROOT="$1"

# Verify that the specified Unreal Engine source tree is a source build rather than an Installed Build
if [ ! -f "$UNREAL_ROOT/Engine/Build/SourceDistribution.txt" ]; then
	echo "Error: the specified Unreal Engine source tree is an Installed Build rather than a source build!"
	exit 1
fi

# Get UE version
BUILD_VERSION_FILE_CONTENT=$(cat "$UNREAL_ROOT/Engine/Build/Build.version")
MAJOR_REGEX="\"MajorVersion\": ([0-9]+),"
MINOR_REGEX="\"MinorVersion\": ([0-9]+),"
if [[ "$BUILD_VERSION_FILE_CONTENT" =~ $MAJOR_REGEX ]]; then 
	UE_MAJOR_VERSION="${BASH_REMATCH[1]}"
else
  echo "Failed to detect UE major version while extracting sysroot, exiting..."
  exit 1
fi

if [[ "$BUILD_VERSION_FILE_CONTENT" =~ $MINOR_REGEX ]]; then 
	UE_MINOR_VERSION="${BASH_REMATCH[1]}"
else
  echo "Failed to detect UE minor version while extracting sysroot, exiting..."
  exit 1
fi

# Set variables to help specify locations for headers and libraries for specific versions of UE
case "$UE_MAJOR_VERSION.$UE_MINOR_VERSION" in
	4.*		) 	# UE4
				PLATFORM_PATH_FRAGMENT="Linux"
				;; 
	5.[012]	) 	# UE5.0 - UE5.2
				PLATFORM_PATH_FRAGMENT="Unix"
				;; 
	5.*		) 	# UE5.3 onward
				PLATFORM_PATH_FRAGMENT="Unix"
				;;
	*		)	echo "Unknown UE version ($UE_MAJOR_VERSION.$UE_MINOR_VERSION) found while extracting sysroot, exiting..."
				exit 1
				;;
esac

# Locate the bundled libc++ headers and binaries
THIRDPARTY="$UNREAL_ROOT/Engine/Source/ThirdParty"
LIBCXX="$THIRDPARTY/$PLATFORM_PATH_FRAGMENT/LibCxx"

TARGET_CPU="x64"

# Identify the clang toolchain for the Engine, if multiple toolchains exist, get the last one found (should be the latest)
TOOLCHAIN_SENTINEL=$(find "$UNREAL_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64" -name 'ToolchainVersion.txt' | tail -n 1)

CHECK_TOOLCHAIN="${TOOLCHAIN_SENTINEL/ToolchainVersion.txt/x86_64-unknown-linux-gnu}"
if [[ -z "$CHECK_TOOLCHAIN" ]]; then
	echo 'Error: could not locate the clang toolchain for the specified Unreal Engine source tree!'
	echo 'Try running "Engine/Build/BatchFiles/Linux/SetupToolchain.sh" (this can be done under Windows with MinGW env like git bash)'
	exit 1
fi

# Remove any existing sysroot
SYSROOT="$(dirname $SCRIPT_DIR)/sysroot"
if [[ -d "$SYSROOT" ]]; then
	echo 'Removing existing sysroot directory...'
	rm -rf "$SYSROOT"
fi
mkdir "$SYSROOT"
mkdir "$SYSROOT/lib"
mkdir "$SYSROOT/lib/x86_64-unknown-linux-gnu"
mkdir "$SYSROOT/lib/aarch64-unknown-linux-gnueabi"
mkdir -p "$SYSROOT/include/c++"

# Extract the files we need from the Unreal Engine source tree
echo "Extracting sysroot from Unreal Engine source tree: $UNREAL_ROOT"
cp -r -v "${TOOLCHAIN_SENTINEL/ToolchainVersion.txt/x86_64-unknown-linux-gnu}" "$SYSROOT"
cp -r -v "${TOOLCHAIN_SENTINEL/ToolchainVersion.txt/aarch64-unknown-linux-gnueabi}" "$SYSROOT"
cp -r -v "$LIBCXX/include/c++/v1" "$SYSROOT/include/c++/v1"
cp -v "$LIBCXX"/lib/$PLATFORM_PATH_FRAGMENT/x86_64-unknown-linux-gnu/* "$SYSROOT/lib/x86_64-unknown-linux-gnu/"
cp -v "$LIBCXX"/lib/$PLATFORM_PATH_FRAGMENT/aarch64-unknown-linux-gnueabi/* "$SYSROOT/lib/aarch64-unknown-linux-gnueabi/"

# Copy the version information for the Unreal Engine into the sysroot to make subsequent identification easier
cp -v "$UNREAL_ROOT/Engine/Build/Build.version" "$SYSROOT/"
