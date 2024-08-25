// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "HarmonixPluginSettings.generated.h"

UCLASS(config = Engine, defaultconfig, Meta = (DisplayName = "Harmonix"))
class HARMONIX_API UHarmonixPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

public:

#if WITH_EDITOR
	//~ UDeveloperSettings interface
	virtual FText GetSectionText() const override;
#endif

	UHarmonixPluginSettings();
};
