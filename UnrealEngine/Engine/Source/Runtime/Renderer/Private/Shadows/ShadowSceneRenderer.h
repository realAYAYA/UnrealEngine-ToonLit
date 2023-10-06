// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/SparseArray.h"
#include "Containers/ArrayView.h"
#include "Containers/BinaryHeap.h"
#include "Containers/Array.h"

#include "VirtualShadowMaps/VirtualShadowMapArray.h"

class FProjectedShadowInfo;
class FDeferredShadingSceneRenderer;
class FWholeSceneProjectedShadowInitializer;
class FRDGBuilder;
class FVirtualShadowMapPerLightCacheEntry;
struct FNaniteVisibilityQuery;
class FShadowScene;

/**
 * Transient scope for per-frame rendering resources for the shadow rendering.
 */
class FShadowSceneRenderer
{
public:
	FShadowSceneRenderer(FDeferredShadingSceneRenderer& InSceneRenderer);

	/**
	 * Multiply PackedView.LODScale by return value when rendering Nanite shadows.
	 */
	static float ComputeNaniteShadowsLODScaleFactor();

	/**
	 * Add a cube/spot light for processing this frame.
	 * TODO: Don't use legacy FProjectedShadowInfo or other params, instead info should flow from persistent setup & update.
	 * TODO: Return reference to FLocalLightShadowFrameSetup ?
	 */
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> AddLocalLightShadow(const FWholeSceneProjectedShadowInitializer& Initializer, FProjectedShadowInfo* ProjectedShadowInfo, FLightSceneInfo* LightSceneInfo, float MaxScreenRadius);
	/**
	 * Add a directional light for processing this frame.
	 * TODO: Don't use legacy FProjectedShadowInfo or other params, instead info should flow from persistent setup & update.
	 * TODO: Return reference to FLocalLightShadowFrameSetup ?
	 */
	void AddDirectionalLightShadow(FProjectedShadowInfo* ProjectedShadowInfo);

	/**
	 * Call after view-dependent setup has been processed (InitView etc) but before any rendering activity has been kicked off.
	 */
	void PostInitDynamicShadowsSetup();

	/**
	 * Call to kick off culling tasks for VSMs & prepare views for rendering.
	 */
	void DispatchVirtualShadowMapViewAndCullingSetup(FRDGBuilder& GraphBuilder, TConstArrayView<FProjectedShadowInfo*> VirtualShadowMapShadows);

	void PostSetupDebugRender();

	/**
	 */
	void RenderVirtualShadowMaps(FRDGBuilder& GraphBuilder, bool bNaniteEnabled, bool bUpdateNaniteStreaming);

	/* Does any one pass shadow projection and generates screen space shadow mask bits
	 * Call before beginning light loop/shadow projection, but after shadow map rendering
	 */
	void RenderVirtualShadowMapProjectionMaskBits(
		FRDGBuilder& GraphBuilder,
		FMinimalSceneTextures& SceneTextures);

	/**
	 * Renders virtual shadow map projection for a given light into the shadow mask.
	 * If one pass projection is enabled, this may be a simple composite from the shadow mask bits.
	 */
	void ApplyVirtualShadowMapProjectionForLight(
		FRDGBuilder& GraphBuilder,
		const FMinimalSceneTextures& SceneTextures,
		const FLightSceneInfo* LightSceneInfo,
		FRDGTextureRef OutputScreenShadowMaskTexture,
		FRDGTextureRef OutputScreenShadowMaskSubPixelTexture);

	// One pass projection stuff. Set up in RenderVitualShadowMapProjectionMaskBits
	FRDGTextureRef VirtualShadowMapMaskBits = nullptr;
	FRDGTextureRef VirtualShadowMapMaskBitsHairStrands = nullptr;

	bool UsePackedShadowMaskBits() const
	{
		return VirtualShadowMapMaskBits != nullptr;
	}

private:
	FVirtualShadowMapProjectionShaderData GetLocalLightProjectionShaderData(float ResolutionLODBiasLocal, const FProjectedShadowInfo* ProjectedShadowInfo, int32 MapIndex) const;

	/**
	 * Select the budgeted set of distant lights to update this frame.
	 */
	void UpdateDistantLightPriorityRender();

	struct FLocalLightShadowFrameSetup
	{
		TSharedPtr<FVirtualShadowMapPerLightCacheEntry> PerLightCacheEntry;
		// link to legacy system stuff, to be removed in due time
		FProjectedShadowInfo* ProjectedShadowInfo = nullptr;
		FLightSceneInfo* LightSceneInfo = nullptr;
	};

	// TODO: maybe we want to keep these in a 1:1 sparse array wrt the light scene infos, for easy crossreference & GPU access (maybe)?
	//       tradeoff is easy to look up (given light ID) but not compact, but OTOH can keep compact lists of indices for various purposes
	TArray<FLocalLightShadowFrameSetup, SceneRenderingAllocator> LocalLights;

	struct FDirectionalLightShadowFrameSetup
	{
		FProjectedShadowInfo* ProjectedShadowInfo = nullptr;
	};
	TArray<FDirectionalLightShadowFrameSetup, SceneRenderingAllocator> DirectionalLights;

	// Priority queue of distant lights to update.
	FBinaryHeap<int32, uint32> DistantLightUpdateQueue;

	// One pass projection stuff. Set up in RenderVitualShadowMapProjectionMaskBits
	bool bShouldUseVirtualShadowMapOnePassProjection = false;

	// Links to other systems etc.
	FDeferredShadingSceneRenderer& SceneRenderer;
	FScene& Scene;
	FShadowScene& ShadowScene;
	FVirtualShadowMapArray& VirtualShadowMapArray;

	FNaniteVisibilityQuery* NaniteVisibilityQuery = nullptr;
	Nanite::FPackedViewArray* VirtualShadowMapViews = nullptr;
	FSceneInstanceCullingQuery *SceneInstanceCullingQuery = nullptr;
};
