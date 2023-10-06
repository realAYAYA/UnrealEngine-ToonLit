// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "ChaosVDEditorSettings.generated.h"

UCLASS(config = Engine)
class UChaosVDEditorSettings : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = EChaosVDParticleDataVisualizationFlags))
	uint8 GlobalParticleDataVisualizationFlags = 0;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = EChaosVDCollisionVisualizationFlags))
	uint8 GlobalCollisionDataVisualizationFlags = 0;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = EChaosVDCollisionVisualizationFlags))
	bool bShowDebugText = false;

	UPROPERTY(Config, EditInstanceOnly, Category = "Editor Options")
	TSoftObjectPtr<UWorld> BasePhysicsVDWorld;
};