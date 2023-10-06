// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenerateConservativeDepth.h
=============================================================================*/

#pragma once

#include "RenderGraphResources.h"

class FViewInfo;
class FRDGBuilder;


void AddGenerateConservativeDepthBufferPass(FViewInfo& View, FRDGBuilder& GraphBuilder, FRDGTextureRef ConservativeDepthTexture, int32 DestinationPixelSizeAtFullRes);

