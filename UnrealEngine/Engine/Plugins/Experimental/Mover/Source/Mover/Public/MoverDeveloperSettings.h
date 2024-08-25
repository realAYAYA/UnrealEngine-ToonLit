// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"

#include "MoverDeveloperSettings.generated.h"

/** Developer settings for the Mover plugin */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Mover Settings"))
class MOVER_API UMoverDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
public:
	UMoverDeveloperSettings();
	
	/**
     * This specifies the number of times a movement mode can refund all of the time in a substep before we back out to avoid freezing the game/editor
     */
    UPROPERTY(config, EditAnywhere, Category = "Mover")
    int32 MaxTimesToRefundSubstep;
	
};
