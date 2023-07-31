#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# $1 is the ARCHICAD version number

set -e # Exit immediately if a command exits with a non-zero status

cd "`dirname "$0"`"

source CollectArchicad.sh

ACVERS=${1}
INSTALLERNAME=${2}
ADDONNAME="DatasmithARCHICAD${ACVERS}Exporter.bundle"

CollectInstalled $ACVERS "${INSTALLERNAME}"

echo "--- $INSTALLERNAME - Install add-on for all ARCHICAD $ACVERS ---"
TMPPATH="/var/tmp/$ADDONNAME"
for ((i = 0; i < $NBACADDONSFOLDERS; i++)); do
    ADDONSPATH="${ACADDONSFOLDERS[$i]}"

    echo "    --- $INSTALLERNAME - Installing: "${ADDONSPATH}/$ADDONNAME" ---"
	sudo rsync -a --delete "$TMPPATH" "${ADDONSPATH}"
    sudo chown -R root:wheel "$ADDONSPATH"
    sudo chmod -R 777 "$ADDONSPATH"
done

echo "--- $INSTALLERNAME - Clean up tmp folder ---"
sudo rm -Rf "$TMPPATH"

if [ $NBACADDONSFOLDERS ]
then
    echo "$NBACADDONSFOLDERS AC$ACVERS installed"
else
	echo "--- $INSTALLERNAME - Could not find ARCHICAD $ACVERS Addon Folder ---"
	exit 1
fi

# Success
exit 0
