#!/bin/sh
# Needs to be run using MSYS shell on Windows. GitHub shell will not work.

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=libvpx-1.13.1

# don't forget to match archive options with tarball type (bz/gz)
TARBALL=../$VER.tar.gz

# includ PID in scratch dir - needs to be absolute
SCRATCH_DIR=`pwd`/Temp
DIR=$SCRATCH_DIR/$VER

DEST_DIR=`pwd`

#####################
# unpack

rm -rf $SCRATCH_DIR
mkdir -p $SCRATCH_DIR

echo "#######################################"
echo "# Unpacking the tarballs"
tar xzf $TARBALL -C $SCRATCH_DIR

#####################
# build
CONFIG_OPTIONS="--enable-postproc --enable-multi-res-encoding --enable-temporal-denoising --enable-vp9-temporal-denoising --enable-vp9-postproc --size-limit=16384x16384 --enable-realtime-only --disable-examples --disable-tools --disable-docs --disable-unit-tests --disable-libyuv --enable-vp9-highbitdepth --disable-avx512 --disable-shared --enable-static --as=yasm"
SLICE="x86_64-win64-vs17"

cd $DIR
echo "#######################################"
echo "# Configuring $VER"
patch -p1 < ../../whole_program_optimization_vs.patch
./configure --target=${SLICE} ${CONFIG_OPTIONS}
echo "# Building $VER"
make -j >> $DEST_DIR/build.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi

if [ $? -eq 0 ]; then
	echo ""
	echo "#######################################"
	echo "# Visual Studio solution was generated in Temp\libvpx-x.x.x directory. Remember to copy your Yasm binary there."
	echo ""
fi
