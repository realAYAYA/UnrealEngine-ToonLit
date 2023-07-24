// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/PlatformSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlatformSettings)

void FPerPlatformSettings::Initialize(TSubclassOf<UPlatformSettings> SettingsClass)
{
#if WITH_EDITOR
	Settings = UPlatformSettingsManager::Get().GetAllPlatformSettings(SettingsClass);
#endif
}

UPlatformSettings::UPlatformSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

