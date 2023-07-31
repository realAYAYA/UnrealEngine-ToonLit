#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Engine/Source/ThirdParty location
export THIRD_PARTY=$(cd "${DIR}/.." ; pwd)

# Folder containing HostLinux
export UE_SDKS_ROOT="${UE_SDKS_ROOT:-/epic}"

# Sanity SDKs existence
if [[ ! -d "${UE_SDKS_ROOT}" ]]; then
	echo ERROR: UE_SDKS_ROOT envvar not set or specifies non-existant location
	exit 1
fi

# Full path to toolchain root
export LINUX_MULTIARCH_ROOT="${LINUX_MULTIARCH_ROOT:-${UE_SDKS_ROOT}/HostLinux/Linux_x64/v19_clang-11.0.1-centos7}"

# Sanity compiler existence
if [[ ! -d "${LINUX_MULTIARCH_ROOT}" ]]; then
	echo ERROR: LINUX_MULTIARCH_ROOT envvar not set or specifies non-existant toolchain
	exit 1
fi

# this is a tag in the vcpkg repository
VCPKG_VERSION=2022.03.10

# enable manifest mode
VCPKG_FEATURE_FLAGS=manifests

# deduce vcpkg nomenclature for this system
if [ `uname` == "Linux" ]; then
	VCPKG_SYSTEM=Linux
else
	echo Error: Linux libraries can only be built on Linux.
	exit 1
fi

VCPKG_ROOT=${TMPDIR-/tmp}/vcpkg-${VCPKG_SYSTEM}-${VCPKG_VERSION}

echo
echo === Checking out vcpkg to ${VCPKG_ROOT}===
git clone --single-branch --branch $VCPKG_VERSION -- https://github.com/microsoft/vcpkg.git $VCPKG_ROOT

echo
echo === Bootstrapping vcpkg ===
${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

echo
echo === Making Linux artifacts writeable ===
chmod -R u+w $DIR/Linux

echo === Making LinuxArm64 artifacts writeable ===
chmod -R u+w $DIR/LinuxArm64

echo
echo === Copying vcpkg.json ===
mkdir -p $DIR/Linux/x86_64-unknown-linux-gnu
mkdir -p $DIR/LinuxArm64/aarch64-unknown-linux-gnueabi
cp $DIR/vcpkg.json $DIR/Linux/x86_64-unknown-linux-gnu/vcpkg.json
cp $DIR/vcpkg.json $DIR/LinuxArm64/aarch64-unknown-linux-gnueabi/vcpkg.json

echo
echo === Running vcpkg in manifest mode ===
# --x-manifest-root tells it to consume build directives in vcpkg.json
# --overlay-triplets tells it to resolve a named triplet via additional paths outside vcpkg/, PWD relative
# --triplet names the triplet to configure the build with, our custom triplet file w/o .cmake extentions
# --debug will provide extra information to stdout
${VCPKG_ROOT}/vcpkg install \
	--overlay-ports=$DIR/overlay-ports \
	--overlay-triplets=$DIR/overlay-triplets \
	--x-manifest-root=$DIR/Linux/x86_64-unknown-linux-gnu \
	--x-packages-root=$DIR/Linux/x86_64-unknown-linux-gnu \
	--triplet=x86_64-unknown-linux-gnu

${VCPKG_ROOT}/vcpkg install \
	--overlay-ports=$DIR/overlay-ports \
	--overlay-triplets=$DIR/overlay-triplets \
	--x-manifest-root=$DIR/LinuxArm64/aarch64-unknown-linux-gnueabi \
	--x-packages-root=$DIR/LinuxArm64/aarch64-unknown-linux-gnueabi \
	--triplet=aarch64-unknown-linux-gnueabi

echo
echo === Replacing symlinks with actual files ===
for f in $(find $DIR/Linux -type l);
do
	rsync `realpath $f` $f
done
for f in $(find $DIR/LinuxArm64 -type l);
do
	rsync `realpath $f` $f
done

echo
echo === Reconciling $VCPKG_SYSTEM artifacts ===
p4 reconcile $DIR/Linux/...
p4 reconcile $DIR/LinuxArm64/...
