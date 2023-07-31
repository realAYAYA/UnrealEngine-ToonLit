// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsCulling.h: Hair strands culling implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "HairStrandsInterface.h"
#include "SceneRendering.h"

struct FHairCullingParams
{
	bool bCullingProcessSkipped	= false;
};

void ComputeHairStrandsClustersCulling(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap& ShaderMap,
	const TArray<FViewInfo>& Views,
	const FHairCullingParams& CullingParameters,
	FHairStrandClusterData& ClusterDatas);
