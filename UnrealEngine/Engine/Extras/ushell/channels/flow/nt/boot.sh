#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

self_dir=$(cygpath --windows --absolute $(dirname $0))
working_dir=$LOCALAPPDATA\\ushell\\.working

# Provision Python
cmd.exe /d/c "$self_dir/provision.bat" "$working_dir"
if [ ! $? ]; then
    exit 1
fi

declare -a channels_dir
channels_dir+=("$self_dir/../..")
channels_dir+=("$USERPROFILE/.ushell/channels")
channels_dir+=("$FLOW_CHANNELS_DIR")

"$working_dir/python/current/flow_python.exe" -Esu "$self_dir/../core/system/boot.py" "$working_dir" "${channels_dir[@]}" -- $@
if [ ! $? ]; then
    echo boot.py failed
    exit 1
fi
