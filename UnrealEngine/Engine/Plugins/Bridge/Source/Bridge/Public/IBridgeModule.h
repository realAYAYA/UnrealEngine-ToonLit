// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"	

#define MS_MODULE_NAME TEXT("Bridge")


class IBridgeModule : public IModuleInterface
{
public:
	static inline IBridgeModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IBridgeModule>(MS_MODULE_NAME);
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(MS_MODULE_NAME);
	}
};
