// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class IOptimusDeveloperModule : public IModuleInterface
{
public:
	static IOptimusDeveloperModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IOptimusDeveloperModule>(TEXT("OptimusDeveloper"));
	}
};
