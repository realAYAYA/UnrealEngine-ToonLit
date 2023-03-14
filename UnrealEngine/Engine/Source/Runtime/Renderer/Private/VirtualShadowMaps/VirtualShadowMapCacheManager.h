// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VirtualShadowMapArray.h:
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "VirtualShadowMapArray.h"
#include "SceneManagement.h"
#include "InstanceCulling/InstanceCullingLoadBalancer.h"
#include "GPUScene.h"
#include "GPUMessaging.h"

class FRHIGPUBufferReadback;
class FGPUScene;
class FVirtualShadowMapPerLightCacheEntry;

#define VSM_LOG_INVALIDATIONS 0

class FVirtualShadowMapCacheEntry
{
public:
	// True if the cache has been (re)populated, set to false on init and set to true once the cache update process has happened.
	// Also set to false whenever key data was not valid and all cached data is invalidated.
	bool IsValid() { return PrevVirtualShadowMapId != INDEX_NONE; }

	void UpdateLocal(int32 VirtualShadowMapId, const FVirtualShadowMapPerLightCacheEntry &PerLightEntry);

	void UpdateClipmap(int32 VirtualShadowMapId,
		const FMatrix &WorldToLight,
		FInt64Point PageSpaceLocation,
		FInt64Point ClipmapCornerOffset,
		double LevelRadius,
		double ViewCenterZ,
		double ViewRadiusZ, 
		const FVirtualShadowMapPerLightCacheEntry& PerLightEntry);

	void Invalidate();

	// Previous frame data
	FInt64Point PrevPageSpaceLocation = FInt64Point(0, 0);
	int32 PrevVirtualShadowMapId = INDEX_NONE;

	// Current frame data
	FInt64Point CurrentPageSpaceLocation = FInt64Point(0, 0);
	int32 CurrentVirtualShadowMapId = INDEX_NONE;

	struct FClipmapInfo
	{
		FMatrix	WorldToLight;
		double ViewCenterZ;
		double ViewRadiusZ;

		// Previous frame data
		FInt64Point PrevClipmapCornerOffset = FInt64Point(0, 0);

		// Current frame data
		FInt64Point CurrentClipmapCornerOffset = FInt64Point(0, 0);
	};
	FClipmapInfo Clipmap;
};

class FVirtualShadowMapPerLightCacheEntry
{
public:
	FVirtualShadowMapPerLightCacheEntry(int32 MaxPersistentScenePrimitiveIndex)
		: RenderedPrimitives(false, MaxPersistentScenePrimitiveIndex)
		, CachedPrimitives(false, MaxPersistentScenePrimitiveIndex)
	{
	}

	TSharedPtr<FVirtualShadowMapCacheEntry> FindCreateShadowMapEntry(int32 Index);

	void OnPrimitiveRendered(const FPrimitiveSceneInfo* PrimitiveSceneInfo);
	/**
	 * The (local) VSM is fully cached if the previous frame if was distant and is distant this frame also.
	 */
	inline bool IsFullyCached() const { return bCurrentIsDistantLight && bPrevIsDistantLight; }
	void MarkRendered(int32 FrameIndex) { CurrentRenderedFrameNumber = FrameIndex; }
	int32 GetLastScheduledFrameNumber() const { return PrevScheduledFrameNumber; }
	void UpdateClipmap();
	void UpdateLocal(const FProjectedShadowInitializer &InCacheKey, bool bIsDistantLight = false);

	/**
	 * Mark as invalid, i.e., needing rendering.
	 */
	void Invalidate();

	bool bPrevIsDistantLight = false;
	int32 PrevRenderedFrameNumber = -1;
	int32 PrevScheduledFrameNumber = -1;

	bool bCurrentIsDistantLight = false;
	int32 CurrentRenderedFrameNumber = -1;
	int32 CurrenScheduledFrameNumber = -1;
	// Primitives that have been rendered (not culled) the previous frame, when a primitive transitions from being culled to not it must be rendered into the VSM
	// Key culling reasons are small size or distance cutoff.
	TBitArray<> RenderedPrimitives;

	// Primitives that have been rendered (not culled) _some_ previous frame, tracked so we can invalidate when they move/are removed (and not otherwise).
	TBitArray<> CachedPrimitives;

	// One entry represents the cached state of a given shadow map in the set of either a clipmap(N), one cube map(6) or a regular VSM (1)
	TArray< TSharedPtr<FVirtualShadowMapCacheEntry> > ShadowMapEntries;

	// TODO: refactor this to not ne stored in the cache entry when we move (some) invalidaitons to the end of frame rather than in the scene primitive updates.
	struct FInstanceRange
	{
		int32 InstanceSceneDataOffset;
		int32 NumInstanceSceneDataEntries;
		bool bInvalidateStaticPage;
	};

	TArray<FInstanceRange> PrimitiveInstancesToInvalidate;

private:
	FProjectedShadowInitializer LocalCacheKey;
};

class FVirtualShadowMapFeedback
{
public:
	FVirtualShadowMapFeedback();
	~FVirtualShadowMapFeedback();

	struct FReadbackInfo
	{
		FRHIGPUBufferReadback* Buffer = nullptr;
		uint32 Size = 0;
	};

	void SubmitFeedbackBuffer(FRDGBuilder& GraphBuilder, FRDGBufferRef FeedbackBuffer);
	FReadbackInfo GetLatestReadbackBuffer();

private:
	static const int32 MaxBuffers = 3;
	int32 WriteIndex = 0;
	int32 NumPending = 0;
	FReadbackInfo Buffers[MaxBuffers];
};

// Persistent buffers that we ping pong frame by frame
struct FVirtualShadowMapArrayFrameData
{
	TRefCountPtr<FRDGPooledBuffer>				PageTable;
	TRefCountPtr<FRDGPooledBuffer>				PageFlags;

	TRefCountPtr<FRDGPooledBuffer>				ProjectionData;
	TRefCountPtr<FRDGPooledBuffer>				PageRectBounds;

	TRefCountPtr<FRDGPooledBuffer>				PhysicalPageMetaData;

	TRefCountPtr<IPooledRenderTarget>			HZBPhysical;
	TMap<int32, FVirtualShadowMapHZBMetadata>	HZBMetadata;

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

class FVirtualShadowMapArrayCacheManager
{
public:
	FVirtualShadowMapArrayCacheManager(FScene *InScene);
	~FVirtualShadowMapArrayCacheManager();

	// Enough for er lots...
	static constexpr uint32 MaxStatFrames = 512*1024U;

	// Called by VirtualShadowMapArray to potentially resize the physical pool
	// If the requested size is not already the size, all cache data is dropped and the pool is resized.
	TRefCountPtr<IPooledRenderTarget> SetPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, int RequestedArraySize);
	void FreePhysicalPool();

	// Called by VirtualShadowMapArray to potentially resize the HZB physical pool
	TRefCountPtr<IPooledRenderTarget> SetHZBPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, const EPixelFormat Format);
	void FreeHZBPhysicalPool();

	// Invalidate the cache for all shadows, causing any pages to be rerendered
	void Invalidate();

	/**
	 * Call at end of frame to extract resouces from the virtual SM array to preserve to next frame.
	 * If bCachingEnabled is false, all previous frame data is dropped and cache (and HZB!) data will not be available for the next frame.
	 */ 
	void ExtractFrameData(FRDGBuilder& GraphBuilder,
		FVirtualShadowMapArray &VirtualShadowMapArray,
		const FSceneRenderer& SceneRenderer,
		bool bEnableCaching);

	/**
	 * Finds an existing cache entry and moves to the active set or creates a fresh one.
	 */
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> FindCreateLightCacheEntry(int32 LightSceneId, uint32 ViewUniqueID = 0U);

	/*
	 * Returns true if cached data is available.
	 */
	bool IsValid();

	bool IsAccumulatingStats();

	using FInstanceGPULoadBalancer = TInstanceCullingLoadBalancer<SceneRenderingAllocator>;

	/**
	 * Helper to collect primitives that need invalidation, filters out redundant adds and also those that are not yet known to the GPU
	 */
	class FInvalidatingPrimitiveCollector
	{
	public:
		FInvalidatingPrimitiveCollector(FVirtualShadowMapArrayCacheManager* InVirtualShadowMapArrayCacheManager);

		/**
		 * All of these functions filters redundant primitive adds, and thus expects valid IDs (so can't be called for primitives that have not yet been added)
		 * and unchanging IDs (so can't be used over a span that include any scene mutation).
		 */

		// Primitive was removed from the scene
		void Removed(FPrimitiveSceneInfo* PrimitiveSceneInfo)
		{
			AddInvalidation(PrimitiveSceneInfo, true);
		}

		// Primitive instances updated
		void UpdatedInstances(FPrimitiveSceneInfo* PrimitiveSceneInfo)
		{
			AddInvalidation(PrimitiveSceneInfo, false);
		}

		// Primitive moved/transform was updated
		void UpdatedTransform(FPrimitiveSceneInfo* PrimitiveSceneInfo)
		{
			AddInvalidation(PrimitiveSceneInfo, false);
		}

		bool IsEmpty() const { return LoadBalancer.IsEmpty(); }

		const TBitArray<SceneRenderingAllocator>& GetRemovedPrimitives() const
		{
			return RemovedPrimitives;
		}

		FInstanceGPULoadBalancer LoadBalancer;
		int32 TotalInstanceCount = 0;
#if VSM_LOG_INVALIDATIONS
		FString RangesStr;
#endif

	private:
		void AddInvalidation(FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bRemovedPrimitive);

		FScene& Scene;
		FGPUScene& GPUScene;
		FVirtualShadowMapArrayCacheManager& Manager;

		TBitArray<SceneRenderingAllocator> AlreadyAddedPrimitives;
		TBitArray<SceneRenderingAllocator> RemovedPrimitives;
	};

	/**
	 * This must to be executed before the instances are actually removed / updated, otherwise the wrong position will be used. 
	 * In particular, it must be processed before the Scene primitive IDs are updated/compacted as part of the removal.
	 * Invalidate pages that are touched by (the instances of) the removed primitives. 
	 */
	void ProcessRemovedOrUpdatedPrimitives(FRDGBuilder& GraphBuilder, const FGPUScene& GPUScene, FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector);

	/**
	 * Allow the cache manager to track scene changes, in particular track resizing of primitive tracking data.
	 */
	void OnSceneChange();

	/**
	 * Handle light removal, need to clear out cache entries as the ID may be reused after this.
	 */
	void OnLightRemoved(int32 LightId);

	TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> GetPreviousUniformBuffer(FRDGBuilder& GraphBuilder) const;

	FVirtualShadowMapArrayFrameData PrevBuffers;
	FVirtualShadowMapUniformParameters PrevUniformParameters;
			
	void SetHZBViewParams(int32 HZBKey, Nanite::FPackedViewParams& OutParams);

#if WITH_MGPU
	void UpdateGPUMask(FRHIGPUMask GPUMask);
#endif

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

	GPUMessage::FSocket StatusFeedbackSocket;

	FVirtualShadowMapFeedback StaticGPUInvalidationsFeedback;

private:
	void ProcessInvalidations(FRDGBuilder& GraphBuilder, FInstanceGPULoadBalancer& Instances, int32 TotalInstanceCount, const FGPUScene& GPUScene);

	void ExtractStats(FRDGBuilder& GraphBuilder, FVirtualShadowMapArray &VirtualShadowMapArray);

	template<typename Allocator>
	void UpdateRecentlyRemoved(const TBitArray<Allocator>& Removed)
	{
		// Combine into *both* flag arrays
		RecentlyRemovedPrimitives[0].CombineWithBitwiseOR(Removed, EBitwiseOperatorFlags::MaxSize);
		RecentlyRemovedPrimitives[1].CombineWithBitwiseOR(Removed, EBitwiseOperatorFlags::MaxSize);
	}

	bool WasRecentlyRemoved(FPersistentPrimitiveIndex PersistentPrimitiveIndex) const
	{
		return RecentlyRemovedPrimitives[RecentlyRemovedReadIndex].Num() > PersistentPrimitiveIndex.Index &&
			RecentlyRemovedPrimitives[RecentlyRemovedReadIndex][PersistentPrimitiveIndex.Index];
	}

	// The actual physical texture data is stored here rather than in VirtualShadowMapArray (which is recreated each frame)
	// This allows us to (optionally) persist cached pages between frames. Regardless of whether caching is enabled,
	// we store the physical pool here.
	TRefCountPtr<IPooledRenderTarget> PhysicalPagePool;
	TRefCountPtr<IPooledRenderTarget> HZBPhysicalPagePool;

	// Index the Cache entries by the light ID
	TMap< uint64, TSharedPtr<FVirtualShadowMapPerLightCacheEntry> > CacheEntries;
	TMap< uint64, TSharedPtr<FVirtualShadowMapPerLightCacheEntry> > PrevCacheEntries;

	// Tracks primitives (by persistent primitive index) that have been removed recently
	// This allows us to ignore feedback from previous frames in the case of persistent primitive indices being
	// reused after being removed. We mark bits in two bitfields, then zero out one of them and switch
	// to the other each time we loop around the feedback buffers. This is somewhat overly conservative but
	// relatively lightweight.
	TBitArray<> RecentlyRemovedPrimitives[2];
	int32 RecentlyRemovedReadIndex = 0;
	int32 RecentlyRemovedFrameCounter = 0;

	// Stores stats over frames when activated.
	TRefCountPtr<FRDGPooledBuffer> AccumulatedStatsBuffer;
	bool bAccumulatingStats = false;
	FRHIGPUBufferReadback* GPUBufferReadback = nullptr;
#if !UE_BUILD_SHIPPING
	FDelegateHandle ScreenMessageDelegate;
	int32 LastOverflowFrame = -1;
	bool bLoggedPageOverflow = false;
#endif
#if WITH_MGPU
	FRHIGPUMask LastGPUMask;
#endif
	FScene* Scene;
};
