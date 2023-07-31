// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

struct FStreamingAccuracyLegendInputs
{
	FScreenPassRenderTarget OverrideOutput;
	FScreenPassTexture SceneColor;
	TArrayView<const FLinearColor> Colors;
};

FScreenPassTexture AddStreamingAccuracyLegendPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FStreamingAccuracyLegendInputs& Inputs);