#!/bin/sh
# Needs to be run using MSYS shell on Windows

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=libvpx-1.6.1

# don't forget to match archive options with tarball type (bz/gz)
TARBALL=../$VER.tar.bz2

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
tar xjf $TARBALL -C $SCRATCH_DIR

#####################
# build

cd $DIR
echo "#######################################"
echo "# Configuring $VER"
patch -p1 < ../../whole_program_optimization_vs.patch
./configure --disable-examples --disable-unit-tests --target=x86_64-win64-vs14 > $DEST_DIR/build.log
echo "# Building $VER"
make -j8 >> $DEST_DIR/build.log
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
