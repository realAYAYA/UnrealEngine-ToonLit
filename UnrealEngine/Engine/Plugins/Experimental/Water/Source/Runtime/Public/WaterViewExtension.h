// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"
#include "WaterInfoRendering.h"


class AWaterZone;

class FWaterViewExtension : public FWorldSceneViewExtension
{
public:
	FWaterViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld);
	~FWaterViewExtension();

	void Initialize();
	void Deinitialize();

	// FSceneViewExtensionBase implementation : 
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) override;
	// End FSceneViewExtensionBase implementation

	void MarkWaterInfoTextureForRebuild(const UE::WaterInfo::FRenderingContext& RenderContext);

	void MarkGPUDataDirty();

	void AddWaterZone(AWaterZone* InWaterZone);
	void RemoveWaterZone(AWaterZone* InWaterZone);

private:
	/** Queued Water Info rendering contexts to submit for rendering on the next SetupView call */
	TMap<AWaterZone*, UE::WaterInfo::FRenderingContext> WaterInfoContextsToRender;

	struct FWaterZoneInfo
	{
		/** 
		 * For each water zone, store the bounds of the tile from which the water zone was last rendered.
		 * When the view location crosses the bounds, submit a new WaterInfo update to reflect the new active area
		 */
		TOptional<FBox2D> UpdateBounds = FBox2D(ForceInit);
	};
	TMap<AWaterZone*, FWaterZoneInfo> WaterZoneInfos;

	struct FWaterGPUResources
	{
		FBufferRHIRef WaterBodyDataBuffer;
		FShaderResourceViewRHIRef WaterBodyDataSRV;

		FBufferRHIRef AuxDataBuffer;
		FShaderResourceViewRHIRef AuxDataSRV;
	};

	TSharedRef<FWaterGPUResources, ESPMode::ThreadSafe> WaterGPUData;

	bool bRebuildGPUData = true;

	void UpdateGPUBuffers();
};

struct FWaterMeshGPUWork
{
	struct FCallback
	{
		class FWaterMeshSceneProxy* Proxy = nullptr;
		TFunction<void(FRDGBuilder&, bool)> Function;
	};
	TArray<FCallback> Callbacks;
};

extern FWaterMeshGPUWork GWaterMeshGPUWork;

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
