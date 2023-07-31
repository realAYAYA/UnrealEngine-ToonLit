#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
VERSION="libxml2-2.9.10"


mkdir tmp
tar -C ./tmp -zxvf libxml2-2.9.10.tar.gz
cd ./tmp/$VERSION

# add the --prefix "$DIR/$SOURCE" option to prevent the compilation to override
# the system's libxml2 library.
./configure --prefix "$DIR/tmp/build" --with-pic=yes --with-python=no --with-lzma=no

# compile libxml
make;
make install;

cd "$DIR"
cp -R ./tmp/build/include/libxml2/* ./$VERSION/include/
cp -R ./tmp/build/lib/* ./$VERSION/lib/x86_64-unknown-linux-gnu/

rm -rf tmp
