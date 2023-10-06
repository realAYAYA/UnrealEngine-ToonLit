// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OverridePassSequence.h"

class FEyeAdaptationParameters;
class FLocalExposureParameters;

struct FVisualizeLocalExposureInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	FScreenPassTexture SceneColor;
	FScreenPassTexture HDRSceneColor;

	FRDGBufferRef EyeAdaptationBuffer = nullptr;

	FRDGTextureRef LumBilateralGridTexture = nullptr;
	FRDGTextureRef BlurredLumTexture = nullptr;
	const FLocalExposureParameters* LocalExposureParameters = nullptr;

	const FEyeAdaptationParameters* EyeAdaptationParameters = nullptr;
};

FScreenPassTexture AddVisualizeLocalExposurePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVisualizeLocalExposureInputs& Inputs);