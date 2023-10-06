// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OverridePassSequence.h"

class FEyeAdaptationParameters;

struct FVisualizeHDRInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	FScreenPassTexture SceneColor; 
	FScreenPassTexture SceneColorBeforeTonemap;

	FRDGTextureRef HistogramTexture = nullptr;
	FRDGBufferRef EyeAdaptationBuffer = nullptr;

	const FEyeAdaptationParameters* EyeAdaptationParameters = nullptr;
};

FScreenPassTexture AddVisualizeHDRPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVisualizeHDRInputs& Inputs);