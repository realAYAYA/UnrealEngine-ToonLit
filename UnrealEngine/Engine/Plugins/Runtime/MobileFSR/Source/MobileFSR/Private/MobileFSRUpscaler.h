// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PostProcess/PostProcessUpscale.h"

class FMobileFSRUpscaler final : public ISpatialUpscaler
{
public:
	FMobileFSRUpscaler(bool bInIsEASUPass);

	// ISpatialUpscaler interface
	const TCHAR* GetDebugName() const override { return TEXT("FMobileFSRUpscaler"); }

	ISpatialUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const override;
	FScreenPassTexture AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const override;

private:
	bool bIsEASUPass = false;
};