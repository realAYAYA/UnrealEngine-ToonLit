// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"

struct DESKTOPPLATFORM_API FLockFile
{
	static bool TryReadAndClear(const TCHAR* FileName, FString& OutContents);
};
