#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

AddAppFolderPath() {
    APPPATH="${1}"
    if [ -d "$APPPATH" ]
    then
        for ((i = 0; i <= $NBACAPPFOLDERS-1; i++)); do
            if [ "${ACAPPFOLDERS[$i]}" == "$APPPATH" ]
            then
                return
            fi
        done
        ACAPPFOLDERS[NBACAPPFOLDERS++]="$APPPATH"
        if [ $ACVERS -gt 22 ]
        then
            ADDONSPATH="$(find "${APPPATH}" -type d -maxdepth 3 -name "Collada In-Out.bundle" )"
        else
            ADDONSPATH="$(find "${APPPATH}" -type d -maxdepth 3 -name "Collada In-Out" )"
        fi
        if [ "$ADDONSPATH" ]
        then
            ACADDONSFOLDERS[NBACADDONSFOLDERS++]="`dirname "$ADDONSPATH"`"
        fi
    fi
}

AddAppBundlePath() {
    if [ -d "${1}" ]
    then
        TMP=$(pwd)
        FOLDER="$( cd "$( dirname "${1}" )" && pwd )"
        cd "$TMP"
        AddAppFolderPath "$FOLDER"
    fi
}

AddAppPathFromPref() {
    if [ "${1}" ]
    then
        ACAPPPATH=$(/usr/libexec/PlistBuddy  -c "Print :'Last Started Path'" "${1}") || echo "No path in ${1}"
        if [ "$ACAPPPATH" ]
        then
            AddAppBundlePath "$ACAPPPATH"
        else
            EMPTY_AC_KEY=$(plutil -convert xml1 "${1}" -o - | grep -A1 '<key></key>') || echo "No empty key in ${1}"
            ACAPPPATH=$(expr "$EMPTY_AC_KEY" : '.*<string>\(.*\)</string>')
            if [ "$ACAPPPATH" ]
            then
                AddAppBundlePath "$ACAPPPATH"
            fi
        fi
    fi
}

CollectInstalled() {
    ACVERS=${1}
    INSTALLERNAME=${2}
    unset ACAPPFOLDERS NBACAPPFOLDERS ACADDONSFOLDERS NBACADDONSFOLDERS
    NBACADDONSFOLDERS=0

    echo "--- $INSTALLERNAME - Collect ARCHICAD $ACVERS from preferences files ---"
    if [ $ACVERS -gt 22 ]
    then
        while IFS= read -r -d $'\0' file; do
            AddAppPathFromPref "$file"
        done < <(find "$HOME/Library/Preferences" -name "com.graphisoft.AC $ACVERS*.plist" -print0)
    else
        while IFS= read -r -d $'\0' file; do
            AddAppPathFromPref "$file"
        done < <(find "$HOME/Library/Preferences" -name "com.graphisoft.AC-64 $ACVERS*.plist" -print0)
    fi

    echo "--- $INSTALLERNAME - Collect ARCHICAD $ACVERS from installer's files ---"
    while IFS= read -r -d $'\0' file; do
        INSTALLPATH=$(/usr/libexec/PlistBuddy  -c "Print :'InstallLocation'" "$file") || echo "No path in $file"
        AddAppFolderPath "$INSTALLPATH"
    done < <(find "/Users/Shared/GRAPHISOFT" -name "com.graphisoft.installers.ARCHICAD*$ACVERS*.plist" -print0)
}
