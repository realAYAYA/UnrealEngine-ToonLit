#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# $1 is the SketchUp version number
# $2 is the Installer name (for messages)

set -e # Exit immediately if a command exits with a non-zero status

cd "`dirname "$0"`"

source CollectSketchUp.sh

# todo: doesn't seems there's a reliable way to check SketchUp installation. 
# Maybe plist(which seems to be creates after SketchUp is run):
# find "$HOME/Library/Preferences" -name "com.sketchup.SketchUp.*.plist" 

INSTALLERNAME="${2}"
CollectInstalled ${1} "${INSTALLERNAME}"

echo "--- $INSTALLERNAME - $NBSUPLUGINSFOLDERS plugin folders found ---"
for ((i = 0; i < $NBSUPLUGINSFOLDERS; i++)); do
    echo "    \"${SUPLUGINSFOLDERS[$i]}\""
done

exit $NBSUPLUGINSFOLDERS
