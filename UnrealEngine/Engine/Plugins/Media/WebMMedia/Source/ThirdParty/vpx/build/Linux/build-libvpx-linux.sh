#!/bin/sh
# Needs to be run on a Linux installation

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=libvpx-1.6.1

# don't forget to match archive options with tarball type (bz/gz)
TARBALL=../$VER.tar.bz2

# includ PID in scratch dir - needs to be absolute
SCRATCH_DIR=/tmp/scratch/$$
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
./configure --disable-examples --disable-unit-tests --extra-cflags="-fvisibility=hidden" > $DEST_DIR/build.log
echo "# Building $VER"
make -j8 >> $DEST_DIR/build.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $DIR/libvpx.a $DEST_DIR

#####################
# build PIC version

cd $DIR
echo "#######################################"
echo "# Configuring $VER with PIC"
./configure --enable-pic --enable-static --disable-examples --disable-unit-tests --extra-cflags="-fvisibility=hidden" > $DEST_DIR/build-pic.log
echo "# Building $VER with PIC"
make -j8 >> $DEST_DIR/build-pic.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $DIR/libvpx.a $DEST_DIR/libvpx_fPIC.a

if [ $? -eq 0 ]; then
	echo ""
	echo "#######################################"
	echo "# Newly built libs have been put into current directory."
	echo ""
fi
