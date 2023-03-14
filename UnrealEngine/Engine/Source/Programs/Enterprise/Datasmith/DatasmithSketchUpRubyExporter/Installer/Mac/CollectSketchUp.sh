#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

CollectInstalled() {
    SUVERS=${1}
    INSTALLERNAME=${2}
    unset SUPLUGINSFOLDERS NBSUPLUGINSFOLDERS
    NBSUPLUGINSFOLDERS=0

    SUPLUGINSFOLDER="$HOME/Library/Application Support/SketchUp $SUVERS/SketchUp/Plugins"
    if [ -d "${SUPLUGINSFOLDER}" ]
    then
        SUPLUGINSFOLDERS[NBSUPLUGINSFOLDERS++]="${SUPLUGINSFOLDER}"
    fi
}
