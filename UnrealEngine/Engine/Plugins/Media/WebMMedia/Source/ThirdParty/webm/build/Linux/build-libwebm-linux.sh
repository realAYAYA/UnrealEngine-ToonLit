#!/bin/sh
# Needs to be run on a Linux installation

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=libwebm-1.0.0.27

# don't forget to match archive options with tarball type (bz/gz)
TARBALL=../$VER.tar.bz2

# includ PID in scratch dir - needs to be absolute
SCRATCH_DIR=/tmp/scratch/$$
SOURCE_DIR=$SCRATCH_DIR/$VER
BUILD_DIR=$SCRATCH_DIR/build

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

mkdir -p $BUILD_DIR
cd $BUILD_DIR
echo "#######################################"
echo "# Configuring $VER"
cmake $SOURCE_DIR > $DEST_DIR/build.log
echo "# Building $VER"
make -j8 webm >> $DEST_DIR/build.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $BUILD_DIR/libwebm.a $DEST_DIR

#####################
# build PIC

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cp $DEST_DIR/CMakeLists_Editor.txt $SOURCE_DIR/CMakeLists.txt
echo "#######################################"
echo "# Configuring $VER with PIC"
cmake $SOURCE_DIR > $DEST_DIR/build-pic.log
echo "# Building $VER with PIC"
make -j8 webm >> $DEST_DIR/build-pic.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi
# use some hardcoded knowledge and get static library out
cp $BUILD_DIR/libwebm.a $DEST_DIR/libwebm_fPIC.a

if [ $? -eq 0 ]; then
	echo ""
	echo "#######################################"
	echo "# Newly built libs have been put into current directory."
	echo ""
fi
