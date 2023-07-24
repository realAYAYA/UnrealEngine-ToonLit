// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserConfig.h"

TObjectPtr<UContentBrowserConfig> UContentBrowserConfig::Instance = nullptr;

void UContentBrowserConfig::Initialize()
{
	if (!Instance)
	{
		Instance = NewObject<UContentBrowserConfig>(); 
		Instance->AddToRoot();
	}
}