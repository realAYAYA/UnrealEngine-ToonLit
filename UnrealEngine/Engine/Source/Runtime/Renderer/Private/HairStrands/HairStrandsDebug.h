// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsDebug.h: Hair strands debug display.
=============================================================================*/

#pragma once

#include "RenderGraph.h"
#include "SceneRendering.h"

void RenderHairStrandsDebugInfo(
	FRDGBuilder& GraphBuilder,
	FScene* Scene, 
	TArrayView<FViewInfo> Views,
	const struct FHairStrandClusterData& HairClusterData,
	FRDGTextureRef SceneColorTexture, 
	FRDGTextureRef SceneDepthTexture);
