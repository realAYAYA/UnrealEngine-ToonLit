// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define USE_DIRECTORY_WATCHER (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

int32 DirectoryWatcherTest(const TCHAR* CommandLine);
