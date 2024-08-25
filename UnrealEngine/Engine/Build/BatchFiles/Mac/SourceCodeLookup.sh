#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.

FunctionName=$1
DylibFullPath=$2
DsymFullPath=$3

if [ -f "$DylibFullPath" ]; then
# Use .dsym file if provided and exists
    if [ ! -z "$DsymFullPath" ] && [ -f "$DsymFullPath" ]; then
        nm -Cg "$DylibFullPath" | grep "\b$FunctionName\b" | cut -d' ' -f1 | atos --fullPath -o "$DsymFullPath"
    else
        nm -Cg "$DylibFullPath" | grep "\b$FunctionName\b" | cut -d' ' -f1 | atos --fullPath -o "$DylibFullPath"
    fi
else
    echo "$DylibFullPath does not exist"
    exit 1
fi
