// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"

// Returns whether subsurface scattering is globally enabled.
bool IsSubsurfaceEnabled();

// Returns whether subsurface scattering is required for the provided view.
bool IsSubsurfaceRequiredForView(const FViewInfo& View);

// Returns whether checkerboard rendering is enabled for the provided format.
bool IsSubsurfaceCheckerboardFormat(EPixelFormat SceneColorFormat);

class FSceneTextureParameters;

struct FVisualizeSubsurfaceInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	// [Required] The scene color to composite with the visualization.
	FScreenPassTexture SceneColor;

	// [Required] The scene textures used to visualize shading models.
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures = nullptr;
};

FScreenPassTexture AddVisualizeSubsurfacePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVisualizeSubsurfaceInputs& Inputs);

void AddSubsurfacePass(
	FRDGBuilder& GraphBuilder,
	FSceneTextures& SceneTextures,
	TArrayView<const FViewInfo> Views);