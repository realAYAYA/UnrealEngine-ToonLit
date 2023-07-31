// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

class FViewInfo;

// Counterpart of FDeferredMaterialPayload declared in RayTracingCommon.ush
struct FDeferredMaterialPayload
{
	uint32 SortKey;
	uint32 PixelCoordinates;
	float  HitT;
};

// Counterpart of DEFERRED_MATERIAL_MODE in RayTracingReflections.ush
enum class EDeferredMaterialMode
{
	None,
	Gather,
	Shade,

	MAX
};

void SortDeferredMaterials(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	uint32 SortSize, // 0: disabled, 1: 256 elements, 2: 512 elements, 3: 1024 elements
	uint32 NumElements,
	FRDGBufferRef MaterialBuffer // buffer of FDeferredMaterialPayload
);

