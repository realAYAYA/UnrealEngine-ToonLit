#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# $1 is the SketchUp version number

set -e # Exit immediately if a command exits with a non-zero status
set -x

cd "`dirname "$0"`"

source CollectSketchUp.sh

SUVERS=${1}
INSTALLERNAME=${2}
PLUGINNAME="DatasmithSketchUpExporter${SUVERS}"

CollectInstalled $SUVERS "${INSTALLERNAME}"

echo "--- $INSTALLERNAME - Install plugin for all SketchUp $SUVERS ---"
TMPPATH="/var/tmp/$PLUGINNAME"
for ((i = 0; i < $NBSUPLUGINSFOLDERS; i++)); do
    PLUGINSPATH="${SUPLUGINSFOLDERS[$i]}"
    echo "    --- $INSTALLERNAME - Installing: "${PLUGINSPATH}/$PLUGINNAME" ---"
    # Two items is copied - plugin loader script that should reside in Plugins foder directly...
    rsync -av --delete "$TMPPATH/UnrealDatasmithSketchUp.rb" "${PLUGINSPATH}"
    # and the rest of the plugin's files(contained in a single folder)
    rsync -av --delete "$TMPPATH/UnrealDatasmithSketchUp" "${PLUGINSPATH}"
	# allow writing to plugin folder
	sudo chmod -R 777 "${PLUGINSPATH}/UnrealDatasmithSketchUp"
done

# todo: we don't need sudo to copy plugin files to $HOME folder but need to cleanup files installed in /var/tmp
# This probably can be fixed by installing directly to $HOME without intermediate /var/tmp?
# echo "--- $INSTALLERNAME - Clean up tmp folder ---"
# sudo rm -Rf "$TMPPATH"

if [ $NBSUPLUGINSFOLDERS -ne 0 ]
then
    echo "$NBSUPLUGINSFOLDERS SU$SUVERS installed"
else
	echo "--- $INSTALLERNAME - Could not find SketchUp $SUVERS Plugins Folder ---"
	exit 1
fi

# Success
exit 0
