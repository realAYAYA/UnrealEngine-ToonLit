// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/Scene.h"
#include "Render/Viewport/IDisplayClusterViewport_CustomPostProcessSettings.h"


class FDisplayClusterViewport_CustomPostProcessSettings
	: public IDisplayClusterViewport_CustomPostProcessSettings
{
public:
	FDisplayClusterViewport_CustomPostProcessSettings() = default;
	virtual ~FDisplayClusterViewport_CustomPostProcessSettings() = default;

public:
	void AddCustomPostProcess(const ERenderPass InRenderPass, const FPostProcessSettings& InSettings, float BlendWeight, bool bSingleFrame);
	void RemoveCustomPostProcess(const ERenderPass InRenderPass);
	void FinalizeFrame();

	virtual bool DoPostProcess(const ERenderPass InRenderPass, FPostProcessSettings* OutSettings, float* OutBlendWeight = nullptr) const override;

private:
	struct FPostprocessData
	{
		FPostProcessSettings Settings;

		float BlendWeight = 1.f;

		bool bIsEnabled = true;
		bool bIsSingleFrame = false;

		FPostprocessData(const FPostProcessSettings& InSettings, float InBlendWeight, bool bInSingleFrame)
			: Settings(InSettings)
			, BlendWeight(InBlendWeight)
			, bIsSingleFrame(bInSingleFrame)
		{ }
	};

	// Custom post processing settings
	TMap<ERenderPass, FPostprocessData> PostprocessAsset;
};
