// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GerstnerWaterWaves.h"
#include "SceneViewExtension.h"

class AWaterBody;
class UGerstnerWaterWaves;

struct FWaveGPUResources
{
	FBufferRHIRef DataBuffer;
	FShaderResourceViewRHIRef DataSRV;

	FBufferRHIRef IndirectionBuffer;
	FShaderResourceViewRHIRef IndirectionSRV;
};

class FGerstnerWaterWaveViewExtension : public FWorldSceneViewExtension
{
public:
	FGerstnerWaterWaveViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld);
	~FGerstnerWaterWaveViewExtension();

	void Initialize();
	void Deinitialize();

	// FSceneViewExtensionBase implementation : 
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	bool bRebuildGPUData = false;

	TSharedRef<FWaveGPUResources, ESPMode::ThreadSafe> WaveGPUData;
};
