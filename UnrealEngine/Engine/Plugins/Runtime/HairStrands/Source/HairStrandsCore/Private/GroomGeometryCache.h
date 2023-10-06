// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HairStrandsMeshProjection.h"

struct FCachedGeometry;
class FSkeletalMeshSceneProxy;
class FGlobalShaderMap;
class FRDGBuilder;
class FGeometryCacheSceneProxy;

FHairStrandsProjectionMeshData::Section ConvertMeshSection(FCachedGeometry const& InCachedGeometry, int32 InSectionIndex);

void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap, 
	const FSkeletalMeshSceneProxy* Proxy,
	const bool bOutputTriangleData,
	FCachedGeometry& OutCachedGeometry);

void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap, 
	const FGeometryCacheSceneProxy* Proxy,
	const bool bOutputTriangleData,
	FCachedGeometry& OutCachedGeometry);