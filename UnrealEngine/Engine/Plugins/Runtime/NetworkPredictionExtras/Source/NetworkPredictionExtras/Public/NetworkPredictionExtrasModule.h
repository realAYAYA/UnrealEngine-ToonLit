// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class INetworkPredictionExtrasModule : public IModuleInterface
{

public:
	
	static inline INetworkPredictionExtrasModule& Get()
	{
		return FModuleManager::LoadModuleChecked< INetworkPredictionExtrasModule >( "NetworkPredictionExtras" );
	}
	
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "NetworkPredictionExtras" );
	}
};

