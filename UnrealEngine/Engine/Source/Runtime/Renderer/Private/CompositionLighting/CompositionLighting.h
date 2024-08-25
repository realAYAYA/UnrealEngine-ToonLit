// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CompositionLighting.h: The center for all deferred lighting activities.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneRendering.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "DecalRenderingShared.h"

/**
 * The center for all screen space processing activities (e.g. G-buffer manipulation, lighting).
 */
class FCompositionLighting
{
public:
	FCompositionLighting(TArrayView<const FViewInfo> InViews, const FSceneTextures& InSceneTextures, TUniqueFunction<bool(int32)> RequestSSAOFunction);

	void ProcessAfterOcclusion(FRDGBuilder& GraphBuilder);

	void ProcessBeforeBasePass(FRDGBuilder& GraphBuilder, FDBufferTextures& DBufferTextures, FInstanceCullingManager& InstanceCullingManager);

	enum class EProcessAfterBasePassMode
	{
		OnlyBeforeLightingDecals,
		SkipBeforeLightingDecals,
		All
	};

	void ProcessAfterBasePass(FRDGBuilder& GraphBuilder, FInstanceCullingManager& InstanceCullingManager, EProcessAfterBasePassMode Mode);

private:
	void TryInit();

	const TArrayView<const FViewInfo> Views;
	const FSceneViewFamily& ViewFamily;
	const FSceneTextures& SceneTextures;

	const bool bEnableDBuffer;
	const bool bEnableDecals;

	enum class ESSAOLocation
	{
		None,
		BeforeBasePass,
		AfterBasePass
	};

	struct FAOConfig
	{
		uint32 Levels = 0;
		EGTAOType GTAOType = EGTAOType::EOff;
		ESSAOLocation SSAOLocation = ESSAOLocation::None;
		bool bSSAOAsync = false;
		bool bRequested = false;
	};

	TArray<FAOConfig, TInlineAllocator<8>> ViewAOConfigs;
	TArray<FTransientDecalRenderDataList, TInlineAllocator<8>> VisibleDecals;
	FRDGTextureRef HorizonsTexture = nullptr;
	bool bInitialized = false;
};

extern bool ShouldRenderScreenSpaceAmbientOcclusion(const FViewInfo& View, bool bLumenWantsSSAO);
