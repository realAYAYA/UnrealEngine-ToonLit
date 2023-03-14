#!/bin/zsh -e
# Copyright Epic Games, Inc. All Rights Reserved.

LIB_ROOT=${0:a:h:h:h}
${LIB_ROOT:h:h}/CMake/PlatformScripts/TVOS/BuildLibForTVOS.command ${LIB_ROOT:h:t} ${LIB_ROOT:t} Release
