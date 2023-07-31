// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "WaterInfoRendering.h"


class AWaterZone;

class FWaterViewExtension : public FWorldSceneViewExtension
{
public:
	FWaterViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld);
	~FWaterViewExtension();

	// FSceneViewExtensionBase implementation : 
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	// End FSceneViewExtensionBase implementation

	void MarkWaterInfoTextureForRebuild(const UE::WaterInfo::FRenderingContext& RenderContext);

private:
	/** Queued Water Info rendering contexts to submit for rendering on the next SetupView call */
	TMap<AWaterZone*, UE::WaterInfo::FRenderingContext> WaterInfoContextsToRender;

	/** 
	 * For each water zone, store the bounds of the tile from which the water zone was last rendered.
	 * When the view location crosses the bounds, submit a new WaterInfo update to reflect the new active area
	 */
	TMap<AWaterZone*, FBox2D> UpdateBounds;
};
