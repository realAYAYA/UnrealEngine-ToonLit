// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

#include <ElectraPlayerPlugin.h>

IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return nullptr;
}

#endif // PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS
