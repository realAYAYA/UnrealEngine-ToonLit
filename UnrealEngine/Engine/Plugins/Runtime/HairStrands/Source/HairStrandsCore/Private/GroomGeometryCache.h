// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HairStrandsMeshProjection.h"

struct FCachedGeometry;
class UGeometryCacheComponent;
class FGlobalShaderMap;
class FRDGBuilder;
class USkeletalMeshComponent;

FHairStrandsProjectionMeshData::Section ConvertMeshSection(FCachedGeometry const& InCachedGeometry, int32 InSectionIndex);

void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap, 
	const USkeletalMeshComponent* SkeletalMeshComponent, 
	const bool bOutputTriangleData,
	FCachedGeometry& OutCachedGeometry);

void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap, 
	const UGeometryCacheComponent* GeometryCacheComponent,
	const bool bOutputTriangleData,
	FCachedGeometry& OutCachedGeometry);