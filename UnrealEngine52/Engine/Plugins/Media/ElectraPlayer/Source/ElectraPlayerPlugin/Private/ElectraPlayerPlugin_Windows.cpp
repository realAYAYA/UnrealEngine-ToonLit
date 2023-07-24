// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_WINDOWS

#include <ElectraPlayerPlugin.h>

IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return nullptr;
}

void FElectraPlayerPlugin::PlatformSetupResourceParams(Electra::FParamDict& Params)
{
	Params.Set("Device", Electra::FVariantValue(GDynamicRHI->RHIGetNativeDevice()));
}

#endif // PLATFORM_WINDOWS