#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Engine/Source/ThirdParty location
export THIRD_PARTY=$(cd "${DIR}/.." ; pwd)

# this is a tag in the vcpkg repository
VCPKG_VERSION=2022.03.10

# enable manifest mode
VCPKG_FEATURE_FLAGS=manifests

# deduce vcpkg nomenclature for this system
if [ `uname` == "Darwin" ]; then
	VCPKG_SYSTEM=Mac
else
	echo Error: Mac libraries can only be built on Mac.
	exit 1
fi

VCPKG_ROOT=${TMPDIR-/tmp}/vcpkg-${VCPKG_SYSTEM}-${VCPKG_VERSION}

echo
echo === Checking out vcpkg to $VCPKG_ROOT ===
git clone --single-branch --branch $VCPKG_VERSION -- https://github.com/microsoft/vcpkg.git $VCPKG_ROOT

echo
echo === Bootstrapping vcpkg ===
${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

echo
echo === Making Mac artifacts writeable ===
chmod -R u+w $DIR/Mac

echo
echo === Copying vcpkg.json ===
mkdir -p $DIR/Mac/x86_64-osx
cp $DIR/vcpkg.json $DIR/Mac/x86_64-osx/vcpkg.json

echo
echo === Running vcpkg in manifest mode ===
# --x-manifest-root tells it to consume build directives in vcpkg.json
# --overlay-triplets tells it to resolve a named triplet via additional paths outside vcpkg/, PWD relative
# --triplet names the triplet to configure the build with, our custom triplet file w/o .cmake extentions
# --debug will provide extra information to stdout
${VCPKG_ROOT}/vcpkg install \
	--overlay-ports=$DIR/overlay-ports \
	--overlay-triplets=$DIR/overlay-triplets \
	--x-manifest-root=$DIR/Mac/x86_64-osx \
	--x-packages-root=$DIR/Mac/x86_64-osx \
	--triplet=x86_64-osx

echo
echo === Replacing symlinks with actual files ===
for f in $(find $DIR/Mac -type l);
do
	rsync `realpath $f` $f
done

echo
echo === Reconciling $VCPKG_SYSTEM artifacts ===
p4 reconcile $DIR/Mac/...
