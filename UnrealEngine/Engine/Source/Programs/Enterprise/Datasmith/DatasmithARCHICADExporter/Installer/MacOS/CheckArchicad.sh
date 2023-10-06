#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

# $1 is the ARCHICAD version number
# $2 is the Installer name (for messages)

set -e # Exit immediately if a command exits with a non-zero status

cd "`dirname "$0"`"

source CollectArchicad.sh

INSTALLERNAME="${2}"
CollectInstalled ${1} "${INSTALLERNAME}"

echo "--- $INSTALLERNAME - $NBACADDONSFOLDERS application's folders found ---"
for ((i = 0; i < $NBACADDONSFOLDERS; i++)); do
    echo "    \"${ACADDONSFOLDERS[$i]}\""
done

exit $NBACADDONSFOLDERS
