#!/bin/bash

# TODO: check if this is under UE_SDKS_ROOT ?
CURRENT_FOLDER=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
echo "$CURRENT_FOLDER"

OUTPUT_ENV_VARS_FOLDER="$CURRENT_FOLDER/.."

rm "$OUTPUT_ENV_VARS_FOLDER/OutputEnvVars.txt"
