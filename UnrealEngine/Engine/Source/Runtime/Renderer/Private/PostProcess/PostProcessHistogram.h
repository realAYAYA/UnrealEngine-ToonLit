// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphFwd.h"

class FEyeAdaptationParameters;
class FSceneTextureParameters;
class FViewInfo;
struct FScreenPassTextureSlice;

FRDGTextureRef AddHistogramPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTextureSlice SceneColor,
	const FSceneTextureParameters& SceneTextures,
	FRDGBufferRef EyeAdaptationBuffer);

FRDGTextureRef AddLocalExposurePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FEyeAdaptationParameters& EyeAdaptationParameters,
	FScreenPassTextureSlice SceneColor);