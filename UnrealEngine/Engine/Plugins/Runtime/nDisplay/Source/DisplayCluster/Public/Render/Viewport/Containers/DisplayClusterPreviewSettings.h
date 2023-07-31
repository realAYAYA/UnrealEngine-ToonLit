// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FDisplayClusterPreviewSettings
{
	// Preview RTT size multiplier
	float PreviewRenderTargetRatioMult = 1.f;

	// Enable/Disable preview rendering. When disabled preview image freeze
	bool bFreezePreviewRender = false;

	// Hack preview gamma.
	// In a scene, PostProcess always renders on top of the preview textures.
	// But in it, PostProcess is also rendered with the flag turned off.
	bool bPreviewEnablePostProcess = false;

	// The maximum dimension of any texture for preview
	int32 PreviewMaxTextureDimension = 2048;

	// Allow mGPU in editor mode
	bool bAllowMultiGPURenderingInEditor = false;

	// Special flag for PIE preview rendering
	bool bIsPIE = false;

	int32 MinGPUIndex = 1;
	int32 MaxGPUIndex = 1;
};
