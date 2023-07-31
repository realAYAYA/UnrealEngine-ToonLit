// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsDeepShadow.h: Hair strands deep shadow implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "HairStrandsInterface.h"
#include "RenderGraphResources.h"
#include "SceneTypes.h"
#include "SceneRendering.h"

void RenderHairStrandsDeepShadows(
	FRDGBuilder& GraphBuilder,
	const class FScene* Scene,
	FViewInfo& View,
	FInstanceCullingManager& InstanceCullingManager);
