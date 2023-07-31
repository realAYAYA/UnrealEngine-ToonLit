// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#include <ElectraPlayerPlugin.h>

IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return nullptr;
}

void FElectraPlayerPlugin::PlatformSetupResourceParams(Electra::FParamDict& Params)
{
}
