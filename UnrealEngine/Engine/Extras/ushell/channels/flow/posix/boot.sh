#!/bin/bash

# Copyright Epic Games, Inc. All Rights Reserved.

self_dir=$(dirname $0)
working_dir=~/.ushell/.working

# provision Python using provision.sh
if ! $self_dir/provision.sh $working_dir; then
    echo Failed to provision Python
    exit 1
fi

declare -a channels_dir
channels_dir+=("$self_dir/../..")
channels_dir+=("$HOME/.ushell/channels")
if [[ -n "$FLOW_CHANNELS_DIR" ]]; then
    channels_dir+=("$FLOW_CHANNELS_DIR")
fi

"$working_dir/python/current/bin/python" -Esu "$self_dir/../core/system/boot.py" "$working_dir" "${channels_dir[@]}" -- "$@"
if [ ! $? ]; then
    echo boot.py failed
    exit 1
fi
