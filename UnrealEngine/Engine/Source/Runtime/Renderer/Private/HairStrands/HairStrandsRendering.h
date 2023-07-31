// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairRendering.h: Hair rendering implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "HairStrandsUtils.h"
#include "HairStrandsCluster.h"
#include "HairStrandsClusters.h"
#include "HairStrandsLUT.h"
#include "HairStrandsDeepShadow.h"
#include "HairStrandsVoxelization.h"
#include "HairStrandsVisibility.h"
#include "HairStrandsTransmittance.h"
#include "HairStrandsEnvironment.h"
#include "HairStrandsComposition.h"
#include "HairStrandsDebug.h"
#include "HairStrandsInterface.h"
#include "HairStrandsData.h"

void RenderHairPrePass(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	TArray<FViewInfo>& Views,
	FInstanceCullingManager& InstanceCullingManager);

void RenderHairBasePass(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	const FSceneTextures& SceneTextures,
	TArray<FViewInfo>& Views,
	FInstanceCullingManager& InstanceCullingManager);

void RunHairStrandsBookmark(
	FRDGBuilder& GraphBuilder, 
	EHairStrandsBookmark Bookmark, 
	FHairStrandsBookmarkParameters& Parameters);

void RunHairStrandsBookmark(
	EHairStrandsBookmark Bookmark,
	FHairStrandsBookmarkParameters& Parameters);

FHairStrandsBookmarkParameters CreateHairStrandsBookmarkParameters(FScene* Scene, FViewInfo& View);
FHairStrandsBookmarkParameters CreateHairStrandsBookmarkParameters(FScene* Scene, TArray<FViewInfo>& Views, TArray<const FSceneView*>& AllFamilyViews);
