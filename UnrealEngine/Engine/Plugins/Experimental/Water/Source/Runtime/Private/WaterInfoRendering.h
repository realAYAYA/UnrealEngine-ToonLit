// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtrTemplates.h"

class AWaterZone;
class UWaterBodyComponent;
class FSceneInterface;
class UTextureRenderTarget2D;
class UPrimitiveComponent;

namespace UE::WaterInfo
{
	struct FRenderingContext
	{
		AWaterZone* ZoneToRender = nullptr;
		UTextureRenderTarget2D* TextureRenderTarget;
		TArray<UWaterBodyComponent*> WaterBodies;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> GroundPrimitiveComponents;
		float CaptureZ;
	};
	
void UpdateWaterInfoRendering(
	FSceneInterface* Scene,
	const FRenderingContext& Context);
}
