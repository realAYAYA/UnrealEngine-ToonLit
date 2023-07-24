#!/bin/sh
## Copyright Epic Games, Inc. All Rights Reserved.
##
## Unreal Engine AutomationTool setup script
##
## This script uses the .command extenion so that is clickable in
## in the OSX Finder.  All it does is call RunUAT.sh which does
## the real work.

exec "`dirname "$0"`"/RunUAT.sh "$@"
