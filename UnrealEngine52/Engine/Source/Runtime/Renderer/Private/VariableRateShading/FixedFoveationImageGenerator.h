// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "RenderGraphDefinitions.h"
#include "VariableRateShadingImageManager.h"
#include "Engine/Engine.h"

class FFixedFoveationImageGenerator : public IVariableRateShadingImageGenerator
{
public:
	virtual ~FFixedFoveationImageGenerator() override {};
	virtual FRDGTextureRef GetImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, FVariableRateShadingImageManager::EVRSPassType PassType) override;
	virtual void PrepareImages(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const FMinimalSceneTextures& SceneTextures) override;
	virtual bool IsEnabledForView(const FSceneView& View) const override;
	virtual FVariableRateShadingImageManager::EVRSSourceType GetType() const override
	{
		return FVariableRateShadingImageManager::EVRSSourceType::FixedFoveation;
	}
	virtual FRDGTextureRef GetDebugImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo) override;
private:
	FRDGTextureRef CachedImage = nullptr;
	struct FDynamicVRSData
	{
		float	VRSAmount = 1.0f;
		double	SumBusyTime = 0.0;
		int		NumFramesStored = 0;
		uint32	LastUpdateFrame = 0;
	} DynamicVRSData;
	float GetDynamicVRSAmount();
};