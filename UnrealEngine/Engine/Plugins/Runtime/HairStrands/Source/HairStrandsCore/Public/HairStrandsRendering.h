// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RenderGraphResources.h"
#include "HairStrandsInterface.h"
#include "Shader.h"
#include "GroomDesc.h"

class FGlobalShaderMap;

// Reset the interpolation data. This needs to be called prior to ComputeHairStrandsInterpolation 
// and prior to the actual hair simulation in order to insure that:
//  1) when hair simulation is enabled, the first frame is correct
//  2) when hair simulation is enabled/disabled (i.e., toggle/change) 
//     we reset to deform buffer to rest state)
void ResetHairStrandsInterpolation(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	struct FHairGroupInstance* Instance,
	int32 LODIndex);

void ComputeHairStrandsInterpolation(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const uint32 ViewUniqueID,
	const uint32 ViewRayTracingMask,
	const FVector& TranslatedWorldOffset,
	const struct FShaderPrintData* ShaderPrintData,
	struct FHairGroupInstance* Instance,
	int32 LODIndex,
	struct FHairStrandClusterData* ClusterData);

void RegisterClusterData(
	struct FHairGroupInstance* Instance,
	FHairStrandClusterData* InClusterData);

HAIRSTRANDSCORE_API void ComputeInterpolationWeights(class UGroomBindingAsset* BindingAsset, class FSkeletalMeshRenderData* TargetRenderData, TArray<FRWBuffer>& TransferedPositions);
