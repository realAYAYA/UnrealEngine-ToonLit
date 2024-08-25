// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class IModularGameplayModule : public IModuleInterface
{
public:
	static FORCEINLINE IModularGameplayModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IModularGameplayModule>("ModularGameplay");
	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#endif
