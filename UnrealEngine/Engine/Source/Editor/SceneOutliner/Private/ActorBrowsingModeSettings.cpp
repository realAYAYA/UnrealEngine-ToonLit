// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorBrowsingModeSettings.h"

TObjectPtr<UActorBrowserConfig> UActorBrowserConfig::Instance = nullptr;

void UActorBrowserConfig::Initialize()
{
	if(!Instance)
	{
		Instance = NewObject<UActorBrowserConfig>(); 
		Instance->AddToRoot();
	}
}