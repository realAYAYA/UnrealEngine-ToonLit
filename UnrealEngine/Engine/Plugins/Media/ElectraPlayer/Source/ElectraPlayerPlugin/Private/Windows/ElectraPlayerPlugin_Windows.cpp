// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_WINDOWS

#include <ElectraPlayerPlugin.h>

IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return nullptr;
}

#endif // PLATFORM_WINDOWS
