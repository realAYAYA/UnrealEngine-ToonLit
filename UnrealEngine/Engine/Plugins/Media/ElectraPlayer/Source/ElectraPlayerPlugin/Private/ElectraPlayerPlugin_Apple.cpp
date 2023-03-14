// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_MAC || PLATFORM_IOS

#include <ElectraPlayerPlugin.h>

IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return nullptr;
}

void FElectraPlayerPlugin::PlatformSetupResourceParams(Electra::FParamDict& Params)
{
}

#endif // PLATFORM_MAC || PLATFORM_IOS
