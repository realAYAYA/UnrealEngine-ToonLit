// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualShadowMapArray.h"
#include "SceneManagement.h"
#include "InstanceCulling/InstanceCullingLoadBalancer.h"
#include "GPUScene.h"
#include "GPUMessaging.h"
#include "SceneRendererInterface.h"

class FRHIGPUBufferReadback;
class FGPUScene;
class FVirtualShadowMapPerLightCacheEntry;
class FInvalidatePagesParameters;

namespace Nanite { struct FPackedViewParams; }

struct FVirtualShadowMapInstanceRange
{
	FPersistentPrimitiveIndex PersistentPrimitiveIndex;
	int32 InstanceSceneDataOffset;
	int32 NumInstanceSceneDataEntries;
};

#define VSM_LOG_INVALIDATIONS 0

class FVirtualShadowMapCacheEntry
{
public:
	// Generic version used for local lights but also inactive lights
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
		double ViewRadiusZ,
		double WPODistanceDisabledThreshold);

	void SetHZBViewParams(Nanite::FPackedViewParams& OutParams);

	// Previous frame data
	FVirtualShadowMapHZBMetadata PrevHZBMetadata;

	// Current frame data
	int32 CurrentVirtualShadowMapId = INDEX_NONE;
	FVirtualShadowMapHZBMetadata CurrentHZBMetadata;

	// Stores the projection shader data. This is needed for cached entries that may be inactive in the current frame/render
	// and also avoids recomputing it every frame.
	FVirtualShadowMapProjectionShaderData ProjectionData;

	// Clipmap-specific information for panning and tracking of cached z-ranges in a given level
	struct FClipmapInfo
	{
		FInt64Point PageSpaceLocation = FInt64Point(0, 0);
		double ViewCenterZ = 0.0;
		double ViewRadiusZ = 0.0;
		double WPODistanceDisableThresholdSquared = 0.0;
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
	 * The (local) VSM is fully cached if it is distant and has been rendered to previously
	 * "Fully" implies that we know all pages are mapped as well as rendered to (ignoring potential CPU-side object culling).
	 */
	inline bool IsFullyCached() const { return bIsDistantLight && Prev.RenderedFrameNumber >= 0; }

	/**
	 */
	inline bool IsUncached() const { return bIsUncached; }

	void MarkRendered(int32 FrameIndex) { Current.RenderedFrameNumber = FrameIndex; }
	int32 GetLastScheduledFrameNumber() const { return Prev.ScheduledFrameNumber; }
	void UpdateClipmap(const FVector& LightDirection, int FirstLevelx);
	/**
	 * Returns true if the cache entry is valid (has previous state).
	 */
	bool UpdateLocal(const FProjectedShadowInitializer &InCacheKey, bool bNewIsDistantLight, bool bCacheEnabled, bool bAllowInvalidation);

	/**
	 * Mark as invalid, i.e., needing rendering.
	 */
	void Invalidate();

	// TODO: We probably don't need the prev/next thing anymore
	struct FFrameState
	{
		int32 RenderedFrameNumber = -1;
		int32 ScheduledFrameNumber = -1;
	};
	FFrameState Prev;
	FFrameState Current;

	bool bIsUncached = false;
	bool bIsDistantLight = false;

	// Tracks if this cache entry is being used "this render", i.e. "active". Note that there may be multiple renders per frame in the case of
	// scene captures or similar, so unlike the RenderedFrameNumber we don't use the scene frame number, but instead mark this
	// when a light is set up, and clear it when extracting frame data.
	bool bReferencedThisRender = false;

	// This tracks the last "rendered frame" the light was active
	uint32 LastReferencedFrameNumber = 0;

	// Primitives that have been rendered (not culled) the previous frame, when a primitive transitions from being culled to not it must be rendered into the VSM
	// Key culling reasons are small size or distance cutoff.
	TBitArray<> RenderedPrimitives;

	// Primitives that have been rendered (not culled) _some_ previous frame, tracked so we can invalidate when they move/are removed (and not otherwise).
	TBitArray<> CachedPrimitives;

	// One entry represents the cached state of a given shadow map in the set of either a clipmap(N), one cube map(6) or a regular VSM (1)
	TArray<FVirtualShadowMapCacheEntry> ShadowMapEntries;

	TArray<FVirtualShadowMapInstanceRange> PrimitiveInstancesToInvalidate;

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
	uint32 LastRequestedSceneFrameNumber;
	uint32 VirtualShadowMapId;
	uint32 MipLevel;
	FUintPoint PageAddress;
};

struct FVirtualShadowMapCacheKey
{
	uint32 ViewUniqueID;
	uint32 LightSceneId;

	inline bool operator==(const FVirtualShadowMapCacheKey& Other) const { return ViewUniqueID == Other.ViewUniqueID && Other.LightSceneId == LightSceneId; }
};

inline uint32 GetTypeHash(FVirtualShadowMapCacheKey Key)
{
	return GetTypeHash(Key.LightSceneId) ^ GetTypeHash(Key.ViewUniqueID);
}

class FVirtualShadowMapArrayCacheManager
{
public:
	using FEntryMap = TMap< FVirtualShadowMapCacheKey, TSharedPtr<FVirtualShadowMapPerLightCacheEntry> >;

	FVirtualShadowMapArrayCacheManager(FScene *InScene);
	~FVirtualShadowMapArrayCacheManager();

	// Enough for er lots...
	static constexpr uint32 MaxStatFrames = 512*1024U;

	// Called by VirtualShadowMapArray to potentially resize the physical pool
	// If the requested size is not already the size, all cache data is dropped and the pool is resized.
	void SetPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, int RequestedArraySize, uint32 MaxPhysicalPages);
	void FreePhysicalPool(FRDGBuilder& GraphBuilder);
	TRefCountPtr<IPooledRenderTarget> GetPhysicalPagePool() const { return PhysicalPagePool; }
	TRefCountPtr<FRDGPooledBuffer> GetPhysicalPageMetaData() const { return PhysicalPageMetaData; }

	// Called by VirtualShadowMapArray to potentially resize the HZB physical pool
	TRefCountPtr<IPooledRenderTarget> SetHZBPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, const EPixelFormat Format);
	void FreeHZBPhysicalPool(FRDGBuilder& GraphBuilder);

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
	* 
	* If bAllowPersistentData is false, all previous frame data is dropped and cache (and HZB!) data will not be available for the next frame.
	* This flag is mostly intended for temporary editor resources like thumbnail rendering that will be used infrequently but often not properly destructed.
	* We need to ensure that the VSM data associated with these renderer instances gets dropped.
	*/ 
	void ExtractFrameData(FRDGBuilder& GraphBuilder,
		FVirtualShadowMapArray &VirtualShadowMapArray,
		const FSceneRenderer& SceneRenderer,
		bool bAllowPersistentData);

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

		void AddPrimitivesToInvalidate();

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

		FInstanceGPULoadBalancer Instances;
		TBitArray<> InvalidatedPrimitives;
		TBitArray<> RemovedPrimitives;

	private:
		void AddInvalidation(FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bRemovedPrimitive);

		FScene& Scene;
		FGPUScene& GPUScene;
		FVirtualShadowMapArrayCacheManager& Manager;
	};

	void ProcessInvalidations(
		FRDGBuilder& GraphBuilder,
		FSceneUniformBuffer &SceneUniformBuffer,
		FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector);

	/**
	 * Allow the cache manager to track scene changes, in particular track resizing of primitive tracking data.
	 */
	void OnSceneChange();

	/**
	 * Handle light removal, need to clear out cache entries as the ID may be reused after this.
	 */
	void OnLightRemoved(int32 LightId);

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

	const FVirtualShadowMapArrayFrameData& GetPrevBuffers() const { return PrevBuffers; }

	uint32 GetStatusFeedbackMessageId() const { return StatusFeedbackSocket.GetMessageId().GetIndex(); }

#if !UE_BUILD_SHIPPING
	uint32 GetStatsFeedbackMessageId() const { return StatsFeedbackSocket.GetMessageId().IsValid() ? StatsFeedbackSocket.GetMessageId().GetIndex() : INDEX_NONE; }
#endif

	float GetGlobalResolutionLodBias() const { return GlobalResolutionLodBias; }

	inline FEntryMap::TIterator CreateEntryIterator()
	{
		return CacheEntries.CreateIterator();
	}

	inline FEntryMap::TConstIterator CreateConstEntryIterator() const
	{
		return CacheEntries.CreateConstIterator();
	}

	UE::Renderer::Private::IShadowInvalidatingInstances *GetInvalidatingInstancesInterface() { return &ShadowInvalidatingInstancesImplementation; }
	FRDGBufferRef UploadCachePrimitiveAsDynamic(FRDGBuilder& GraphBuilder) const;
private:

	/** 
	 */
	class FShadowInvalidatingInstancesImplementation : public UE::Renderer::Private::IShadowInvalidatingInstances
	{
	public:
		FShadowInvalidatingInstancesImplementation(FVirtualShadowMapArrayCacheManager &InCacheManager) : CacheManager(InCacheManager) {}
		virtual void AddPrimitive(const FPrimitiveSceneInfo *PrimitiveSceneInfo);
		virtual void AddInstanceRange(FPersistentPrimitiveIndex PersistentPrimitiveIndex, uint32 InstanceSceneDataOffset, uint32 NumInstanceSceneDataEntries);

		FVirtualShadowMapArrayCacheManager &CacheManager;
		TArray<FVirtualShadowMapInstanceRange> PrimitiveInstancesToInvalidate;
	};

	// Invalidate the cache for all shadows, causing any pages to be rerendered
	void Invalidate(FRDGBuilder& GraphBuilder);

	struct FInvalidationPassCommon
	{
		FVirtualShadowMapUniformParameters* UniformParameters;
		TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> VirtualShadowMapUniformBuffer;
		TRDGUniformBufferRef<FSceneUniformParameters> SceneUniformBuffer;
	};

	FInvalidationPassCommon GetUniformParametersForInvalidation(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer) const;

	void SetInvalidateInstancePagesParameters(
		FRDGBuilder& GraphBuilder,
		const FInvalidationPassCommon& InvalidationPassCommon,
		FInvalidatePagesParameters* PassParameters) const;

	void UpdateCachePrimitiveAsDynamic(FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector);

	// Invalidate instances based on CPU instance ranges. This is used for CPU-based updates like object transform changes, etc.
	void ProcessInvalidations(FRDGBuilder& GraphBuilder, const FInvalidationPassCommon& InvalidationPassCommon, const FInstanceGPULoadBalancer& Instances) const;
	
	void ExtractStats(FRDGBuilder& GraphBuilder, FVirtualShadowMapArray &VirtualShadowMapArray);

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
	uint32 MaxPhysicalPages = 0;

	// Index the Cache entries by the light ID
	FEntryMap CacheEntries;

	// Store the last time a primitive caused an invalidation for dynamic/static caching purposes
	// NOTE: Set bits as dynamic since the container makes it easier to iterate those
	TBitArray<> CachePrimitiveAsDynamic;
	// Indexed by PersistentPrimitiveIndex
	TArray<uint32> LastPrimitiveInvalidatedFrame;

	// Stores stats over frames when activated.
	TRefCountPtr<FRDGPooledBuffer> AccumulatedStatsBuffer;
	bool bAccumulatingStats = false;
	FRHIGPUBufferReadback* GPUBufferReadback = nullptr;

	GPUMessage::FSocket StatusFeedbackSocket;

	// Current global resolution bias (when enabled) based on feedback from page pressure, etc.
	float GlobalResolutionLodBias = 0.0f;
	uint32 LastFrameOverPageAllocationBudget = 0;
	
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

	FScene* Scene;
	FShadowInvalidatingInstancesImplementation ShadowInvalidatingInstancesImplementation;
};
