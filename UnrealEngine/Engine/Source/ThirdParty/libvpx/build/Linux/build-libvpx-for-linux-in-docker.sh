#!/bin/bash

usage ()
{
cat << EOF

Usage:
   $0 [OPTIONS]

Run docker container and build libvpx inside of it for further linux platform.

OPTIONS:
   -v VER    libVpx version [$VER]
   -d        Debug mode (trace commands)
   -t        Using Epic's toolchain
   -h        Show this message
EOF
}

VER=${VER:-1.13.1}
EXTERNAL_TOOLCHAIN=0

while getopts :v:dth OPTION; do
  case $OPTION in
  v) VER=$OPTARG ;;
  d) DEBUG=1 ;;
  t) EXTERNAL_TOOLCHAIN=1;;
  h) usage; exit 1 ;;
  esac
done

[ "$DEBUG" = 1 ] && set -x

UE_ROOT_DIR="$( cd "$( dirname "../../../../../../../" )" && pwd )"

export UE_ROOT_DIR

sys_root=""

if [ "$EXTERNAL_TOOLCHAIN" = 1 ]; then
   echo "Checking if sysroot is present ($UE_ROOT_DIR)"
   if [ ! -f ./sysroot.copied ]; then
	   source ./utils/extract-sysroot.sh "$UE_ROOT_DIR" || exit 1
	   touch ./sysroot.copied
	   echo "Sysroot copied from $UE_ROOT_DIR"
   fi
fi

# Stop and remove any old instances of the builder
docker stop centos7_build_libvpx > /dev/null 2>&1
docker rm centos7_build_libvpx > /dev/null 2>&1

# Build our image
echo "Building Linux libvpx builder image..."
docker build -q -f centos7_build_libvpx.dockerfile -t centos7_build_libvpx . || exit 1

# Run our container with the provided options
echo "Running Linux builder image..."

MSYS_NO_PATHCONV=1 builder_args="/mnt/libvpx/Linux/build-libvpx-linux.sh -v $VER"
MSYS_NO_PATHCONV=1 [ "$DEBUG" = 1 ] && builder_args="$builder_args -d"

MSYS_NO_PATHCONV=1 libvpx_mnt="/mnt/libvpx"
[ "$EXTERNAL_TOOLCHAIN" = 1 ] && MSYS_NO_PATHCONV=1 sys_root="$libvpx_mnt/Linux/sysroot"

# If the user environment variable isn't set (ie. our host is Windows), set the user and group id's to 
# 0 (root) in the container, otherwise we will encounter file permission errors under a Windows host
if [ -z "$USER" ]; then 
	USER_ID=0
	GROUP_ID=0
fi

pushd ../ > /dev/null
# # We want to mount parent folder for PWD (build folder)
MSYS_NO_PATHCONV=1 libvpx_root=$(pwd)
popd > /dev/null

interactive_arg="--interactive"

# uncomment to run shell in the docker
# interactive_arg="-it"
# MSYS_NO_PATHCONV=1 builder_args="/bin/bash"

MSYS_NO_PATHCONV=1 docker run \
	$interactive_arg \
	--name centos7_build_libvpx \
	-u $USER_ID:$GROUP_ID \
	-v "$libvpx_root:$libvpx_mnt:rw" \
   --mount type=bind,source="$UE_ROOT_DIR",target="/mnt/ue,readonly" \
   --env UE_ROOT_DIR="/mnt/ue" \
	--env UE_SYSROOT="$sys_root" \
	centos7_build_libvpx:latest "$builder_args" || exit 1

#Success
exit 0
