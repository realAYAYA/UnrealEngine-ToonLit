// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RayTracingInstance.h"
#include "MeshPassProcessor.h"
#include "RenderGraphDefinitions.h"

class FRayTracingScene;
class FViewFamilyInfo;
class FGlobalDynamicReadBuffer;

namespace RayTracing
{
	struct FRelevantPrimitiveList;

	void OnRenderBegin(FScene& Scene, TArray<FViewInfo>& Views, const FViewFamilyInfo& ViewFamily);

	FRelevantPrimitiveList* CreateRelevantPrimitiveList(FSceneRenderingBulkObjectAllocator& InAllocator);

	// Iterates over Scene's PrimitiveSceneProxies and extracts ones that are relevant for ray tracing.
	// This function can run on any thread.
	void GatherRelevantPrimitives(FScene& Scene, const FViewInfo& View, FRelevantPrimitiveList& OutRelevantPrimitiveList);

	// Fills RayTracingScene instance list for the given View and adds relevant ray tracing data to the view. Does not reset previous scene contents.
	// This function must run on render thread
	bool GatherWorldInstancesForView(
		FRDGBuilder& GraphBuilder,
		FScene& Scene,
		FViewInfo& View,
		FRayTracingScene& RayTracingScene,
		FGlobalDynamicReadBuffer& InDynamicReadBuffer,
		FSceneRenderingBulkObjectAllocator& InBulkAllocator,
		FRelevantPrimitiveList& RelevantPrimitiveList);

	bool ShouldExcludeDecals();
}

#endif // RHI_RAYTRACING