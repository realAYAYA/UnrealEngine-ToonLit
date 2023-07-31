// Copyright Epic Games, Inc. All Rights Reserved.

#include "AugmentedRealityModule.h"
#include "Modules/ModuleManager.h"

class FAugmentedRealityModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FAugmentedRealityModule, AugmentedReality);

DEFINE_LOG_CATEGORY(LogAR);
