// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"

#include "SurfaceEffectsSettings.generated.h"

/**
 * Surface Effects Settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Surface Effects"))
class SURFACEEFFECTS_API USurfaceEffectsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** List of data tables to load tags from */
	UPROPERTY(config, EditAnywhere, Category = GameplayTags, meta = (AllowedClasses = "/Script/Engine.DataTable"))
	FSoftObjectPath SurfaceEffectsDataTable;
};