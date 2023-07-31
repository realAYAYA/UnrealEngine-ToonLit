// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/FilterBarConfig.h"

TObjectPtr<UFilterBarConfig> UFilterBarConfig::Instance = nullptr;

void UFilterBarConfig::Initialize()
{
	if(!Instance)
	{
		Instance = NewObject<UFilterBarConfig>(); 
		Instance->AddToRoot();
	}
}