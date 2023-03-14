// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"




#define MS_MODULE_NAME TEXT("MegascansPlugin")


class IMegascansLiveLinkModule : public IModuleInterface
{


	

public:

	static inline IMegascansLiveLinkModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IMegascansLiveLinkModule>(MS_MODULE_NAME);
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(MS_MODULE_NAME);
	}
};

