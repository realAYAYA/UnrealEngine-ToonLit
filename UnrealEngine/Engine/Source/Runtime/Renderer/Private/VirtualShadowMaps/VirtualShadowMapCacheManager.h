// Copyright Epic Games, Inc. All Rights Reserved.

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

namespace Nanite { struct FPackedViewParams; }

#define VSM_LOG_INVALIDATIONS 0

class FVirtualShadowMapCacheEntry
{
public:
	// Generic version used for local lights but also unreferenced lights
	// Updates the VSM ID
	void Update(
		FVirtualShadowMapArray& VirtualShadowMapArray,
		const FVirtualShadowMapPerLightCacheEntry &PerLightEntry,
		int32 VirtualShadowMapId);

	// Specific version of the above for clipmap levels, which have additional constraints
	void UpdateClipmapLevel(
		FVirtualShadowMapArray& VirtualShadowMapArray,
		const FVirtualShadowMapPerLightCacheEntry& PerLightEntry,
		int32 VirtualShadowMapId,
		FInt64Point PageSpaceLocation,
		double LevelRadius,
		double ViewCenterZ,
		double ViewRadiusZ);

	void SetHZBViewParams(Nanite::FPackedViewParams& OutParams);

	// Previous frame data
	FVirtualShadowMapHZBMetadata PrevHZBMetadata;

	// Current frame data
	int32 CurrentVirtualShadowMapId = INDEX_NONE;
	FVirtualShadowMapHZBMetadata CurrentHZBMetadata;

	// Stores the projection shader data. This is needed for cached entries that may be unreferenced on the current frame
	// and also avoids recomputing it every frame.
	FVirtualShadowMapProjectionShaderData ProjectionData;

	// Clipmap-specific information for panning and tracking of cached z-ranges in a given level
	struct FClipmapInfo
	{
		FInt64Point PageSpaceLocation = FInt64Point(0, 0);
		double ViewCenterZ = 0.0;
		double ViewRadiusZ = 0.0;
	};
	FClipmapInfo Clipmap;
};

class FVirtualShadowMapPerLightCacheEntry
{
public:
	FVirtualShadowMapPerLightCacheEntry(int32 MaxPersistentScenePrimitiveIndex, uint32 NumShadowMaps)
		: RenderedPrimitives(false, MaxPersistentScenePrimitiveIndex)
		, CachedPrimitives(false, MaxPersistentScenePrimitiveIndex)
	{
		ShadowMapEntries.SetNum(NumShadowMaps);
	}

	void OnPrimitiveRendered(const FPrimitiveSceneInfo* PrimitiveSceneInfo);
	/**
	 * The (local) VSM is fully cached if the previous frame if was distant and is distant this frame also.
	 * "Fully" implies that we know all pages are mapped as well as rendered to (ignoring potential CPU-side object culling).
	 */
	inline bool IsFullyCached() const { return Current.bIsDistantLight && Prev.bIsDistantLight && Prev.RenderedFrameNumber >= 0; }

	/**
	 */
	inline bool IsUncached() const { return Current.bIsUncached; }

	void MarkRendered(int32 FrameIndex) { Current.RenderedFrameNumber = FrameIndex; }
	int32 GetLastScheduledFrameNumber() const { return Prev.ScheduledFrameNumber; }
	void UpdateClipmap(const FVector& LightDirection, int FirstLevelx);
	/**
	 * Returns true if the cache entry is valid (has previous state).
	 */
	bool UpdateLocal(const FProjectedShadowInitializer &InCacheKey, bool bIsDistantLight, bool bCacheEnabled, bool bAllowInvalidation);

	/**
	 * Mark as invalid, i.e., needing rendering.
	 */
	void Invalidate();

	// TODO: We probably don't need the prev/next thing anymore
	struct FFrameState
	{
		bool bIsUncached = false;
		bool bIsDistantLight = false;
		int32 RenderedFrameNumber = -1;
		int32 ScheduledFrameNumber = -1;		

	};
	FFrameState Prev;
	FFrameState Current;

	// Tracks the last time we saw this light referenced.
	// This allows us to keep entries alive even when they are temporarily not visible or similar, but age them out over time
	// NOTE: This is not exactly a frame number since multiple "Render" calls may happen per frame, and we need to track
	// them separately for the purposes of determining whether a given light is actually referenced in the current Render.
	int64 ReferencedRenderSequenceNumber = -1;

	// Primitives that have been rendered (not culled) the previous frame, when a primitive transitions from being culled to not it must be rendered into the VSM
	// Key culling reasons are small size or distance cutoff.
	TBitArray<> RenderedPrimitives;

	// Primitives that have been rendered (not culled) _some_ previous frame, tracked so we can invalidate when they move/are removed (and not otherwise).
	TBitArray<> CachedPrimitives;

	// One entry represents the cached state of a given shadow map in the set of either a clipmap(N), one cube map(6) or a regular VSM (1)
	TArray<FVirtualShadowMapCacheEntry> ShadowMapEntries;

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
	struct FClipmapCacheKey
	{
		FVector LightDirection;
		int FirstLevel;
		int LevelCount;
	};
	FClipmapCacheKey ClipmapCacheKey;
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
	TRefCountPtr<FRDGPooledBuffer>				PageRectBounds;
	TRefCountPtr<FRDGPooledBuffer>				ProjectionData;
	TRefCountPtr<FRDGPooledBuffer>				PhysicalPageLists;

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

struct FPhysicalPageMetaData
{	
	uint32 Flags;
	uint32 Age;
	uint32 VirtualShadowMapId;
	uint32 MipLevel;
	FUintPoint PageAddress;
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
	void SetPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, int RequestedArraySize, uint32 MaxPhysicalPages);
	void FreePhysicalPool();
	TRefCountPtr<IPooledRenderTarget> GetPhysicalPagePool() const { return PhysicalPagePool; }
	TRefCountPtr<FRDGPooledBuffer> GetPhysicalPageMetaData() const { return PhysicalPageMetaData; }

	// Called by VirtualShadowMapArray to potentially resize the HZB physical pool
	TRefCountPtr<IPooledRenderTarget> SetHZBPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, const EPixelFormat Format);
	void FreeHZBPhysicalPool();

	// Set the cache as valid (called after allocation/analysis/metadata update)
	// NOTE: Could be called after rendering instead, but the distinction is not currently meaningful
	void MarkCacheDataValid();
	
	// Invalidate the cache for all shadows, causing any pages to be rerendered
	void Invalidate();

	/**
	 * Called before VSM builds page allocations to reallocate any lights that may not be visible this frame
	 * but that may still have cached physical pages. We reallocate new VSM each frame for these to allow the associated
	 * physical pages to live through short periods of being offscreen or otherwise culled. This function also removes
	 * entries that are too old.
	 */
	void UpdateUnreferencedCacheEntries(FVirtualShadowMapArray& VirtualShadowMapArray);

	// Must be called *after* calling UpdateUnreferencedCacheEntries - TODO: Perhaps merge the two to enforce
	void UploadProjectionData(FRDGScatterUploadBuffer& Uploader) const;

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
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> FindCreateLightCacheEntry(int32 LightSceneId, uint32 ViewUniqueID, uint32 NumShadowMaps);

	bool IsCacheEnabled();
	bool IsCacheDataAvailable();
	bool IsHZBDataAvailable();

	bool IsAccumulatingStats();

	using FInstanceGPULoadBalancer = TInstanceCullingLoadBalancer<SceneRenderingAllocator>;

	/**
	 * Helper to collect primitives that need invalidation, filters out redundant adds and also those that are not yet known to the GPU
	 */
	class FInvalidatingPrimitiveCollector
	{
	public:
		FInvalidatingPrimitiveCollector(FVirtualShadowMapArrayCacheManager* InVirtualShadowMapArrayCacheManager);

		void AddDynamicAndGPUPrimitives();

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

		void Finalize();

		const TBitArray<SceneRenderingAllocator>& GetRemovedPrimitives() const
		{
			return RemovedPrimitives;
		}

		FInstanceGPULoadBalancer Instances;

	private:
		void AddInvalidation(FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bRemovedPrimitive);

		FScene& Scene;
		FGPUScene& GPUScene;
		FVirtualShadowMapArrayCacheManager& Manager;

		TBitArray<SceneRenderingAllocator> RemovedPrimitives;
	};

	void ProcessInvalidations(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer, const FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector);

	/**
	 * Allow the cache manager to track scene changes, in particular track resizing of primitive tracking data.
	 */
	void OnSceneChange();

	/**
	 * Handle light removal, need to clear out cache entries as the ID may be reused after this.
	 */
	void OnLightRemoved(int32 LightId);

	const FVirtualShadowMapUniformParameters& GetPreviousUniformParameters() const { return PrevUniformParameters; }
	TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> GetPreviousUniformBuffer(FRDGBuilder& GraphBuilder) const;

#if WITH_MGPU
	void UpdateGPUMask(FRHIGPUMask GPUMask);
#endif

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

	const FVirtualShadowMapArrayFrameData& GetPrevBuffers() const { return PrevBuffers; }

	uint32 GetStatusFeedbackMessageId() const { return StatusFeedbackSocket.GetMessageId().GetIndex(); }

#if !UE_BUILD_SHIPPING
	uint32 GetStatsFeedbackMessageId() const { return StatsFeedbackSocket.GetMessageId().IsValid() ? StatsFeedbackSocket.GetMessageId().GetIndex() : INDEX_NONE; }
#endif

private:
	void ProcessInvalidations(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer, const FInstanceGPULoadBalancer& Instances) const;

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

	// Remove old info used to track logging.
	void TrimLoggingInfo();

	FVirtualShadowMapArrayFrameData PrevBuffers;
	FVirtualShadowMapUniformParameters PrevUniformParameters;

	// The actual physical texture data is stored here rather than in VirtualShadowMapArray (which is recreated each frame)
	// This allows us to (optionally) persist cached pages between frames. Regardless of whether caching is enabled,
	// we store the physical pool here.
	TRefCountPtr<IPooledRenderTarget> PhysicalPagePool;
	TRefCountPtr<IPooledRenderTarget> HZBPhysicalPagePool;
	ETextureCreateFlags PhysicalPagePoolCreateFlags = TexCreate_None;
	TRefCountPtr<FRDGPooledBuffer> PhysicalPageMetaData;

	// Index the Cache entries by the light ID
	TMap< uint64, TSharedPtr<FVirtualShadowMapPerLightCacheEntry> > CacheEntries;

	// Marked after successfully allocating new physical pages
	// Cleared if any global invalidation happens, in which case the next VSM update will not consider cached pages
	bool bCacheDataValid = false;

	// Tracks primitives (by persistent primitive index) that have been removed recently
	// This allows us to ignore feedback from previous frames in the case of persistent primitive indices being
	// reused after being removed. We mark bits in two bitfields, then zero out one of them and switch
	// to the other each time we loop around the feedback buffers. This is somewhat overly conservative but
	// relatively lightweight.
	TBitArray<> RecentlyRemovedPrimitives[2];
	int32 RecentlyRemovedReadIndex = 0;
	int32 RecentlyRemovedFrameCounter = 0;

	// Used for tracking which cache entries are referenced in a given render call
	int64 RenderSequenceNumber = 0;

	// Stores stats over frames when activated.
	TRefCountPtr<FRDGPooledBuffer> AccumulatedStatsBuffer;
	bool bAccumulatingStats = false;
	FRHIGPUBufferReadback* GPUBufferReadback = nullptr;

	FVirtualShadowMapFeedback StaticGPUInvalidationsFeedback;
	GPUMessage::FSocket StatusFeedbackSocket;

	// Debug stuff
#if !UE_BUILD_SHIPPING
	FDelegateHandle ScreenMessageDelegate;
	float LastOverflowTime = -1.0f;
	bool bLoggedPageOverflow = false;
	
	// Socket for optional stats that are only sent back if enabled
	GPUMessage::FSocket StatsFeedbackSocket;

	// Stores the last time (wall-clock seconds since app-start) that an non-nanite page area message was logged,
	TArray<float> LastLoggedPageOverlapAppTime;

	// Map to track non-nanite page area items that are shown on screen
	struct FLargePageAreaItem
	{
		uint32 PageArea;
		float LastTimeSeen;
	};
	TMap<uint32, FLargePageAreaItem> LargePageAreaItems;
#endif // UE_BUILD_SHIPPING

#if WITH_MGPU
	FRHIGPUMask LastGPUMask;
#endif

	FScene* Scene;
};
