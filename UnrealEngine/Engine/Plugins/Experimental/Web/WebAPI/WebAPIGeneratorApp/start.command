#!/bin/sh
# Copyright Epic Games, Inc. All Rights Reserved.
#
# Simple wrapper around start.sh using the
# .command extension enables it to be run from the OSX Finder.

sh "`dirname "$0"`"/start.sh $*
