// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "StereoLayerManager.h"
#include "SceneViewExtension.h"
class FHeadMountedDisplayBase;

/** 
 *	Default implementation of stereo layers for platforms that require emulating layer support.
 *
 *	FHeadmountedDisplayBase subclasses will use this implementation by default unless overridden.
 */
class HEADMOUNTEDDISPLAY_API FDefaultStereoLayers : public FSimpleLayerManager, public FHMDSceneViewExtension
{
public:
	FDefaultStereoLayers(const FAutoRegister& AutoRegister, FHeadMountedDisplayBase* InHMDDevice);

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual bool ShouldCopyDebugLayersToSpectatorScreen() const override
	{
		// Emulated layer support means that the debug layer will be in the 3d scene render that the spectator screen displays.
		return false;
	}

	/** Experimental struct */
	struct FLayerRenderParams
	{
		FIntRect Viewport;
		FMatrix RenderMatrices[3];
	};

	/** Experimental method */
	static void StereoLayerRender(FRHICommandListImmediate& RHICmdList, const TArray<FLayerDesc>& LayersToRender, const FLayerRenderParams& RenderParams);

protected:
	
	/**
	 * Invoked by FHeadMountedDisplayBase to update the HMD position during the late update.
	 */
	void UpdateHmdTransform(const FTransform& InHmdTransform)
	{
		HmdTransform = InHmdTransform;
	}

	FHeadMountedDisplayBase* HMDDevice;
	FTransform HmdTransform;

	TArray<FLayerDesc> SortedSceneLayers;
	TArray<FLayerDesc> SortedOverlayLayers;

	friend class FHeadMountedDisplayBase;
};