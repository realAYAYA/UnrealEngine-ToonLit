#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

ARGS=$@

SCRIPT_PATH=$0
if [ -L "$SCRIPT_PATH" ]; then
    SCRIPT_PATH=$(dirname "$SCRIPT_PATH")/$(readlink "$SCRIPT_PATH")
fi

cd "$(dirname "$SCRIPT_PATH")" && SCRIPT_PATH="`pwd`/$(basename "$SCRIPT_PATH")"

BASE_PATH="`dirname "$SCRIPT_PATH"`"

# cd to Engine root
cd ../../../..
RESULT=0

while : ; do
        ./Engine/Binaries/DotNET/GitDependencies/linux-x64/GitDependencies $ARGS
        RESULT=$?

        echo "Result: $RESULT"
        # quit if not crashed
        [[ $RESULT -lt 129 ]] && break
        echo "./Engine/Binaries/DotNET/GitDependencies/linux-x64/GitDependencies $ARGS crashed with return code $RESULT" >> GitDependencies.crash.log
done

exit $RESULT
