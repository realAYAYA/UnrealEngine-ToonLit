// Copyright Epic Games, Inc. All Rights Reserved.

/*
===============================================================================
	ScreenSpaceShadows.h: Functionality for rendering screen space shadows
===============================================================================
*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

struct FMinimalSceneTextures;
class FLightSceneInfo;

void RenderScreenSpaceShadows(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const TArray<FViewInfo>& Views,
	const FLightSceneInfo* LightSceneInfo,
	bool bProjectingForForwardShading,
	FRDGTextureRef ScreenShadowMaskTexture);