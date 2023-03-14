#!/bin/bash

# TODO: check if this is under UE_SDKS_ROOT ?
CURRENT_FOLDER=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
echo "$CURRENT_FOLDER"

OUTPUT_ENV_VARS_FOLDER="$CURRENT_FOLDER/.."
rm "$OUTPUT_ENV_VARS_FOLDER/OutputEnvVars.txt" 2> /dev/null

# set new LINUX_MULTIARCH_ROOT that supersedes it
echo LINUX_MULTIARCH_ROOT="$CURRENT_FOLDER" > "$OUTPUT_ENV_VARS_FOLDER/OutputEnvVars.txt"

# support old branches that expect this file in a different location.
cp "$OUTPUT_ENV_VARS_FOLDER/OutputEnvVars.txt" "$CURRENT_FOLDER"
