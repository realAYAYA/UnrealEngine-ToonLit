// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Includes
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "INetcodeUnitTest.h"


class INUTUnrealEngine : public FNUTModuleInterface
{
public:
	INUTUnrealEngine()
		: FNUTModuleInterface(TEXT("NUTUnrealEngine"))
	{
	}

	static inline INUTUnrealEngine& Get()
	{
		return FModuleManager::LoadModuleChecked<INUTUnrealEngine>("NUTUnrealEngine");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NUTUnrealEngine");
	}
};

