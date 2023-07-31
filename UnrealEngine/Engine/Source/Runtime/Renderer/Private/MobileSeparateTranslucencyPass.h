// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScreenPass.h"

struct FMobileSeparateTranslucencyInputs
{
	FScreenPassTexture SceneColor;
	FScreenPassTexture SceneDepth;
};

void AddMobileSeparateTranslucencyPass(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, const FMobileSeparateTranslucencyInputs& Inputs);

// Returns whether separate translucency is enabled and there primitives to draw in the view
bool IsMobileSeparateTranslucencyActive(const FViewInfo& View);
bool IsMobileSeparateTranslucencyActive(const FViewInfo* Views, int32 NumViews);