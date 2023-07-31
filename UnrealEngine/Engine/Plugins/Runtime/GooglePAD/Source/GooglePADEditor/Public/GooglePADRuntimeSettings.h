// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "GooglePADRuntimeSettings.generated.h"

/**
* Implements the settings for the GooglePAD plugin.
*/
UCLASS(Config = Engine, DefaultConfig)
class UGooglePADRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// Enable GooglePAD plugin
	UPROPERTY(EditAnywhere, config, Category = Packaging)
	bool bEnablePlugin;

	// Only for distribution builds
	UPROPERTY(EditAnywhere, config, Category = Packaging)
	bool bOnlyDistribution;

	// Only for shipping builds
	UPROPERTY(EditAnywhere, config, Category = Packaging)
	bool bOnlyShipping;
};
