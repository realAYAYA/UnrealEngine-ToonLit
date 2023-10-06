// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolPresetSettings.h"

#include "UObject/ObjectPtr.h"

TObjectPtr<UToolPresetUserSettings> UToolPresetUserSettings::Instance = nullptr;

void UToolPresetUserSettings::Initialize()
{
	if (Instance == nullptr)
	{
		Instance = NewObject<UToolPresetUserSettings>();
		Instance->AddToRoot();
	}
}

UToolPresetUserSettings* UToolPresetUserSettings::Get()
{
	return Instance;
}
