// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VariableRateShadingImageManager.h"

#include <openxr/openxr.h>

class FRGDBuilder;
class FOpenXRHMD;
class FSceneView;

class FFBFoveationImageGenerator : public IVariableRateShadingImageGenerator
{
public:
	FFBFoveationImageGenerator(bool bIsFoveationExtensionSupported, XrInstance InInstance, FOpenXRHMD* HMD, bool bMobileMultiViewEnabled);
	virtual ~FFBFoveationImageGenerator() override {};

	bool IsFoveationExtensionEnabled() { return bFoveationExtensionSupported;}
	void UpdateFoveationImages();
	void SetCurrentFrameSwapchainIndex(int32 CurrentFrameSwapchainIndex);

	/** IVariableRateShadingImageGenerator interface */
	virtual FRDGTextureRef GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType) override;
	virtual void PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures) override;
	virtual bool IsEnabledForView(const FSceneView& View) const override;
	virtual FRDGTextureRef GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSImageType ImageType) override;
	virtual FVariableRateShadingImageManager::EVRSSourceType GetType() const override
	{
		return FVariableRateShadingImageManager::EVRSSourceType::FixedFoveation;
	}
	/** End of IVariableRateShadingImageGenerator interface */

private:

	FOpenXRHMD* OpenXRHMD;
	bool		bIsMobileMultiViewEnabled;
	int32		CurrentFrameSwapchainIndex;

	// XR_FB_foveation
	bool					bFoveationExtensionSupported;
	XrFoveationLevelFB		FoveationLevel;
	float					VerticalOffset;
	XrFoveationDynamicFB	FoveationDynamic;
	TArray<FTextureRHIRef>	FoveationImages;
	PFN_xrCreateFoveationProfileFB	xrCreateFoveationProfileFB;
	PFN_xrUpdateSwapchainFB			xrUpdateSwapchainFB;
	PFN_xrDestroyFoveationProfileFB xrDestroyFoveationProfileFB;
};

