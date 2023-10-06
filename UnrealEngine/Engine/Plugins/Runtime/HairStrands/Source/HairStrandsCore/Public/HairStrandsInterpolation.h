// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "HAL/Platform.h"
#include "Math/MathFwd.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RenderGraphResources.h"
#include "HairStrandsInterface.h"
#include "Shader.h"
#include "GroomDesc.h"
#endif

class FGlobalShaderMap;
class FRDGBuilder;
class FSkeletalMeshRenderData;
class UGroomBindingAsset;

struct FHairGroupInstance;
struct FHairStrandClusterData;
struct FRWBuffer;
struct FShaderPrintData;

enum class EGroomViewMode : uint8;

// Reset the interpolation data. This needs to be called prior to ComputeHairStrandsInterpolation 
// and prior to the actual hair simulation in order to insure that:
//  1) when hair simulation is enabled, the first frame is correct
//  2) when hair simulation is enabled/disabled (i.e., toggle/change) 
//     we reset to deform buffer to rest state)
void ResetHairStrandsInterpolation(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FHairGroupInstance* Instance,
	int32 LODIndex);

void ComputeHairStrandsInterpolation(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const uint32 ViewUniqueID,
	const uint32 ViewRayTracingMask,
	const EGroomViewMode ViewMode,
	const FVector& TranslatedWorldOffset,
	const FShaderPrintData* ShaderPrintData,
	FHairGroupInstance* Instance,
	int32 LODIndex,
	FHairStrandClusterData* ClusterData);

HAIRSTRANDSCORE_API void ComputeInterpolationWeights(UGroomBindingAsset* BindingAsset, FSkeletalMeshRenderData* TargetRenderData, TArray<FRWBuffer>& TransferedPositions);
