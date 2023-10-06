// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "ScreenPass.h"

class FScene;

struct FMobileDistortionAccumulateInputs
{
	FScreenPassTexture SceneColor;
};

struct FMobileDistortionAccumulateOutputs
{
	FScreenPassTexture DistortionAccumulate;
};

FMobileDistortionAccumulateOutputs AddMobileDistortionAccumulatePass(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, const FMobileDistortionAccumulateInputs& Inputs);

struct FMobileDistortionMergeInputs
{
	FScreenPassTexture SceneColor;
	FScreenPassTexture DistortionAccumulate;
};

FScreenPassTexture AddMobileDistortionMergePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FMobileDistortionMergeInputs& Inputs);

// Returns whether distortion is enabled and there primitives to draw
bool IsMobileDistortionActive(const FViewInfo& View);
