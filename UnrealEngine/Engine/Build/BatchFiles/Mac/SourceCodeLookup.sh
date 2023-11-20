#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

FunctionName=$1
DylibFullPath=$2

if [ -f "$DylibFullPath" ]; then
    nm -Cg "$DylibFullPath" | grep "\b$FunctionName\b" | cut -d' ' -f1 | atos --fullPath -o "$DylibFullPath"
else
    echo "$DylibFullPath does not exist"
    exit 1
fi
