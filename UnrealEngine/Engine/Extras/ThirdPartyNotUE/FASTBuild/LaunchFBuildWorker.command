# Copyright Epic Games, Inc. All Rights Reserved.
#!/bin/sh

if [ -f ~/.unrealrc ]; then
    source ~/.unrealrc
fi

if [ -f .unrealrc ]; then
    source .unrealrc
fi

if [ -z "$FASTBUILD_BROKERAGE_PATH" ]; then
    export FASTBUILD_BROKERAGE_PATH=/Volumes/FASTBuildBrokerage
fi

echo Using FASTBUILD_BROKERAGE_PATH at $FASTBUILD_BROKERAGE_PATH
`dirname "$0"`/Mac/FBuildWorker -mode=idle -cpus=-1
