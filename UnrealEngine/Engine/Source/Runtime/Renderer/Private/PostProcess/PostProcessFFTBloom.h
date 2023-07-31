// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraph.h"
#include "ScenePrivate.h"
#include "ScreenPass.h"

class FEyeAdaptationParameters;

// Returns whether FFT bloom is enabled for the view.
bool IsFFTBloomEnabled(const FViewInfo& View);

float GetFFTBloomResolutionFraction(const FIntPoint& ViewSize);

struct FFFTBloomOutput
{
	FScreenPassTexture BloomTexture;
	FRDGBufferRef SceneColorApplyParameters = nullptr;
};

FFFTBloomOutput AddFFTBloomPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FScreenPassTexture& InputSceneColor,
	float InputResolutionFraction,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FRDGTextureRef EyeAdaptationTexture,
	FRDGTextureRef LocalExposureTexture,
	FRDGTextureRef BlurredLogLuminanceTexture);
