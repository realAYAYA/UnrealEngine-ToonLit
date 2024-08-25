#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

set -e

echo $1 $2

# $1 is the path to the executable
# $2 is the path to the dSYM we want to create

# make sure extension of param 2 is a dsym since we are about to delete it, protect against pass / or something
extension=${2##*.}
if [[ "${extension}" != "dSYM" &&  "${extension}" != "dsym" ]]; then
	echo "Usage: GenerateUniversalDSYMl.sh <path to input binary> <path to output dsym>"
	echo "       For executables in a .app, specify the full path (such as "Foo.app/Contents/MacOS/Foo")"
	exit 0
fi

# Cleanup any old dSYM
if [ -e "$2" ]; then
	echo "Deleting existing .dSYM"
	rm -r "$2"
fi

mkdir -p "$(dirname $2)"
binaryname=$(basename "$1")
tempdir=`mktemp -d`

# See if the binary is fat (arches will be "x86_64 arm64" for universal)
arches=(`lipo -archs "$1"`)
if [ ${#arches[@]} -gt 1 ]; then
	for i in ${!arches[@]} 
	do
		arch=${arches[$i]}

		echo "Extracting $arch from $1..."
		lipo -extract $arch -output $tempdir/$arch "$1" 

		# Get paths to the extracted binary and the binary inside the dSYM
		dsympaths[$i]="$tempdir/$arch.dSYM"
		binpaths[$i]="$tempdir/$arch.dSYM/Contents/Resources/DWARF/$arch"
	done

	for i in ${!arches[@]} 
	do
		arch=${arches[$i]}

		echo "Generating debug symbols (dSYM) for $arch" 
		dsymutil -o $tempdir/$arch.dSYM $tempdir/$arch &    
	done

	# wait for the dsymutils to finish
	wait

	# Copy the first dSYM back to result dSYM location - we will overwrite one file inside the dir structure
	ditto ${dsympaths[0]} "$2"

	# remove the arch-named file inside, will replace with lipo below
	rm "$2/Contents/Resources/DWARF/${arches[0]}"
  
	# lipo all dsym binaries directly to the project-named file that we'd expect
	echo "Merging architectures '${binpaths[*]}' together into $2"
	
	lipo ${binpaths[*]} -create -output "$2/Contents/Resources/DWARF/${binaryname}"
	
	retVal=$?
	if [ $retVal -ne 0 ]; then
		echo "ERROR: Something went wrong with lipo. We're in trouble now."
	fi

	# Clean up
	rm -rf "$tempdir"
	
	exit $retVal
else
  echo "Using standard dsymutil because the binary was not universal..."
  dsymutil "$1" -o "$2"
fi
