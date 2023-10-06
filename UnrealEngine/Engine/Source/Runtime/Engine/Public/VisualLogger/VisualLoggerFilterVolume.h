// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Volume.h"
#include "VisualLoggerFilterVolume.generated.h"

/**
 * A volume to be placed in the level while browsing visual logger output.
 */
UCLASS(MinimalAPI)
class AVisualLoggerFilterVolume : public AVolume
{
	GENERATED_BODY()
public:
	ENGINE_API AVisualLoggerFilterVolume(const FObjectInitializer& ObjectInitializer);
	
};
