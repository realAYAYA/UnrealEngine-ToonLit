// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "MassCrowdLaneDataRenderingComponent.generated.h"

/**
 * Primitive component that can be used to render runtime state of zone graph lanes (e.g. Opened|Closed, Density, etc.)
 * The component must be added on a ZoneGraphData actor.
 */
UCLASS(editinlinenew, meta = (BlueprintSpawnableComponent), hidecategories = (Object, LOD, Lighting, VirtualTexture, Transform, HLOD, Collision, TextureStreaming, Mobile, Physics, Tags, AssetUserData, Activation, Cooking, Rendering, Navigation))
class MASSCROWD_API UMassCrowdLaneDataRenderingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UMassCrowdLaneDataRenderingComponent() = default;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
private:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	void DebugDrawOnCanvas(UCanvas* Canvas, APlayerController*) const;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	FDelegateHandle OnLaneStateChangedDelegateHandle;
#if WITH_EDITOR
	FDelegateHandle OnLaneRenderSettingsChangedDelegateHandle;
#endif // WITH_EDITOR
#endif // !UE_BUILD_SHIPPING && !UE_BUILD_TEST
};