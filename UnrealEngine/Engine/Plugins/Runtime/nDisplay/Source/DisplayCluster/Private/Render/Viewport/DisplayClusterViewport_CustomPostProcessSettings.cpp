// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport_CustomPostProcessSettings.h"


void FDisplayClusterViewport_CustomPostProcessSettings::AddCustomPostProcess(const ERenderPass InRenderPass, const FPostProcessSettings& InSettings, float BlendWeight, bool bSingleFrame)
{
	PostprocessAsset.Emplace(InRenderPass, FPostprocessData(InSettings, BlendWeight, bSingleFrame));
}


void FDisplayClusterViewport_CustomPostProcessSettings::RemoveCustomPostProcess(const ERenderPass InRenderPass)
{
	if (PostprocessAsset.Contains(InRenderPass))
	{
		PostprocessAsset.Remove(InRenderPass);
	}
}

bool FDisplayClusterViewport_CustomPostProcessSettings::DoPostProcess(const ERenderPass InRenderPass, FPostProcessSettings* OutSettings, float* OutBlendWeight) const
{
	const FPostprocessData* ExistSettings = PostprocessAsset.Find(InRenderPass);
	if (ExistSettings && ExistSettings->bIsEnabled)
	{
		if (OutSettings != nullptr)
		{
			*OutSettings = ExistSettings->Settings;
		}

		if (OutBlendWeight != nullptr)
		{
			*OutBlendWeight = ExistSettings->BlendWeight;
		}

		return true;
	}

	return false;
}

void FDisplayClusterViewport_CustomPostProcessSettings::FinalizeFrame()
{
	// Safe remove items out of iterator
	for (TPair<ERenderPass, FPostprocessData>& It: PostprocessAsset)
	{
		if (It.Value.bIsSingleFrame)
		{
			It.Value.bIsEnabled = false;
		}
	}
}
