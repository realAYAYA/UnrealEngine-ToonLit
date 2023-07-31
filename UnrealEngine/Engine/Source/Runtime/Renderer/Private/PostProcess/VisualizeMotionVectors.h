// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"


struct FVisualizeMotionVectorsInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	FScreenPassTexture SceneColor;
	FScreenPassTexture SceneDepth;
	FScreenPassTexture SceneVelocity;
};

FScreenPassTexture AddVisualizeMotionVectorsPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVisualizeMotionVectorsInputs& Inputs);