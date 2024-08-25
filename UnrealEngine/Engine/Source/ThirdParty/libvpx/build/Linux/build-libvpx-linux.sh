#!/bin/sh
# Needs to be run on a Linux installation

usage ()
{
cat << EOF

Usage:
   $0 [OPTIONS]

Build libvpx for linux platform.

OPTIONS:
   -v VER    libVpx version [$VER]
   -d        Debug mode (trace commands)
   -h        Show this message
EOF
}

while getopts :v:dh OPTION; do
  case $OPTION in
  v) VER=$OPTARG ;;
  d) DEBUG=1 ;;
  h) usage; exit 1 ;;
  esac
done

[ "$DEBUG" = 1 ] && set -x

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=${VER:-1.13.1}
LIBVPX_DIR=libvpx-$VER
CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# don't forget to match archive options with tarball type (bz/gz)
TARBALL=$CUR_DIR/../$LIBVPX_DIR.tar.gz

# includ PID in scratch dir - needs to be absolute
SCRATCH_DIR=/tmp/scratch/$$
DIR=$SCRATCH_DIR/$LIBVPX_DIR

DEST_DIR=$CUR_DIR

#####################
# unpack

rm -rf $SCRATCH_DIR
mkdir -p $SCRATCH_DIR

echo "#######################################"
echo "# Unpacking the tarballs"
tar xzf $TARBALL -C $SCRATCH_DIR

#####################
# build

BUILD_CFLAGS="-fvisibility=hidden"
CONFIG_OPTIONS="--enable-postproc --enable-multi-res-encoding --enable-temporal-denoising --enable-vp9-temporal-denoising --enable-vp9-postproc --size-limit=16384x16384 --enable-realtime-only --disable-examples --disable-tools --disable-docs --disable-unit-tests --disable-libyuv --enable-vp9-highbitdepth --disable-avx512 --disable-shared --enable-static --as=yasm"
SLICE="x86_64-linux-gcc"
# SLICE="generic-gnu"

# setup for using custom toolchain
if [[ ! -z "$UE_SYSROOT" ]]; then
   TOOLCHAIN="x86_64-unknown-linux-gnu"
   export CFLAGS="--sysroot=$UE_SYSROOT/$TOOLCHAIN"
   export CXXFLAGS="--sysroot=$UE_SYSROOT/$TOOLCHAIN -I$UE_SYSROOT/include/c++/v1"
   export LDFLAGS="--sysroot=$UE_SYSROOT/$TOOLCHAIN"
   export CROSS="$TOOLCHAIN-"
   export PATH=$UE_SYSROOT/$TOOLCHAIN/bin:$PATH
fi

cd $DIR
echo "#######################################"
echo "# Configuring $LIBVPX_DIR"
./configure --target=${SLICE} ${CONFIG_OPTIONS} --disable-pic --extra-cflags="${BUILD_CFLAGS}" > $DEST_DIR/build.log
echo "# Building $LIBVPX_DIR"
# use make -j SHELL="sh -x" to see command line of every call
make -j8 >> $DEST_DIR/build.log 2>&1
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $DIR/libvpx.a $DEST_DIR/libvpx.a
cp $DIR/libvpx_g.a $DEST_DIR/libvpx_g.a

#####################
# build PIC version

cd $DIR
make clean > /dev/null
echo "#######################################"
echo "# Configuring $LIBVPX_DIR with PIC"
./configure --target=${SLICE} ${CONFIG_OPTIONS} --enable-pic --extra-cflags="${BUILD_CFLAGS}" > $DEST_DIR/build-pic.log
echo "# Building $LIBVPX_DIR with PIC"
# use make -j SHELL="sh -x" to see command line of every call
make -j8 >> $DEST_DIR/build-pic.log 2>&1
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $DIR/libvpx.a $DEST_DIR/libvpx_fPIC.a
cp $DIR/libvpx_g.a $DEST_DIR/libvpx_g_fPIC.a

if [ $? -eq 0 ]; then
	echo ""
	echo "#######################################"
	echo "# Newly built libs have been put into $DEST_DIR directory."
	echo ""
fi
