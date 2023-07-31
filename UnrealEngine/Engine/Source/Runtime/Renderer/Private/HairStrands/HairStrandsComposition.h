// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsComposition.h: Hair strands pixel composition implementation.
=============================================================================*/

#pragma once

#include "RenderGraph.h"
#include "SceneRendering.h"

void RenderHairComposition(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture, 
	FRDGTextureRef VelocityTexture);

void RenderHairComposition(
	FRDGBuilder& GraphBuilder, 
	const TArray<FViewInfo>& Views,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef VelocityTexture);
