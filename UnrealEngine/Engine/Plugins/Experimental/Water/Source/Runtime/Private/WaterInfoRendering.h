// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AWaterZone;
class UWaterBodyComponent;
class FSceneInterface;
class UTextureRenderTarget2D;
class AActor;

namespace UE::WaterInfo
{
	struct FRenderingContext
	{
		AWaterZone* ZoneToRender = nullptr;
		UTextureRenderTarget2D* TextureRenderTarget;
		TArray<UWaterBodyComponent*> WaterBodies;
		TArray<TWeakObjectPtr<AActor>> GroundActors;
		float CaptureZ;
	};
	
void UpdateWaterInfoRendering(
	FSceneInterface* Scene,
	const FRenderingContext& Context);
}
