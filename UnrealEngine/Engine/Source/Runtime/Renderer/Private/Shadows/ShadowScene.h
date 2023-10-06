// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/SparseArray.h"
#include "Containers/ArrayView.h"
#include "Containers/Array.h"
#include "Tasks/Task.h"

class FProjectedShadowInfo;
class FScene;
class FWholeSceneProjectedShadowInitializer;
class FRDGBuilder;
class FVirtualShadowMapPerLightCacheEntry;
struct FLightSceneChangeSet;
class FViewInfo;
class FPrimitiveSceneInfo;
class FScenePreUpdateChangeSet;
class FScenePostUpdateChangeSet;

/**
 * Persistent scene-representation of for shadow rendering.
 */
class FShadowScene
{
public:
	FShadowScene(FScene &InScene);

	// Handle all scene changes wrt lights, attached to the FScene::OnPostLigtSceneInfoUpdate delegate.
	void PostLightsUpdate(FRDGBuilder& GraphBuilder, const FLightSceneChangeSet &LightSceneChangeSet);

	/**
	 * Handle scene changes, notably track all primitives that always invalidate the shadows.
	 */
	void PostSceneUpdate(const FScenePreUpdateChangeSet& ScenePreUpdateChangeSet, const FScenePostUpdateChangeSet& ScenePostUpdateChangeSet);

	/**
	 * Fetch the "mobility factor" for the light, [0,1] where 0.0 means not moving, and 1.0 means was updated this frame.
	 * Does a smooth transition from 1 to 0 over N frames, defined by the cvar.
	 */
	float GetLightMobilityFactor(int32 LightId) const;

	/**
	 * Call once per rendered frame to update state that depend on number of rendered frames.
	 */
	void UpdateForRenderedFrame(FRDGBuilder& GraphBuilder);

	void DebugRender(TArrayView<FViewInfo> Views);
	
	// List of always invalidating primitives, if this gets too popular perhaps a TSet or some such is more appropriate for performance scaling.
	TArrayView<FPrimitiveSceneInfo*> GetAlwaysInvalidatingPrimitives() { return AlwaysInvalidatingPrimitives; }

private:
	//friend class FShadowSceneRenderer;

	inline bool IsActive(int32 LightId) const { return ActiveLights[LightId]; }

	struct FLightData
	{
		/**
		 * Scene rendering frame number of the first frame that the scene was rendered after being modified (moved/added).
		 */
		int32 FirstActiveFrameNumber;
		float MobilityFactor;
	};
	TSparseArray<FLightData> Lights;

	inline FLightData& GetOrAddLight(int32 LightId)
	{
		if (!Lights.IsValidIndex(LightId))
		{
			Lights.EmplaceAt(LightId, FLightData{});
		}
		return Lights[LightId];
	}
	// Bit-array marking active lights, those we deem active are ones that have been modified in a recent frame and thus need some kind of active update.
	TBitArray<> ActiveLights;

	// Links to other systems etc.
	FScene& Scene;
	mutable UE::Tasks::FTask SceneChangeUpdateTask;

	// List of always invalidating primitives, if this gets too popular perhaps a TSet or some such is more appropriate for performance scaling.
	TArray<FPrimitiveSceneInfo*> AlwaysInvalidatingPrimitives;
};
