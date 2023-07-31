// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Volume.h"
#include "VisualLoggerFilterVolume.generated.h"

/**
 * A volume to be placed in the level while browsing visual logger output.
 */
UCLASS()
class ENGINE_API AVisualLoggerFilterVolume : public AVolume
{
	GENERATED_BODY()
public:
	AVisualLoggerFilterVolume(const FObjectInitializer& ObjectInitializer);
	
};