// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpanAllocator.h"
#include "ConvexVolume.h"
#include "Rendering/RenderingSpatialHash.h"
#include "Tasks/Task.h"
#include "RendererInterface.h"
#include "RendererPrivateUtils.h"
#include "PrimitiveSceneInfo.h"

#include "SceneCullingDefinitions.h"
#include "HierarchicalSpatialHashGrid.h"

class FScenePreUpdateChangeSet;
class FScenePostUpdateChangeSet;
class FScene;
struct FPrimitiveBounds;

// Note: the needed precomputation is not yet implemented in the ISM proxy / etc.
#define SCENE_CULLING_USE_PRECOMPUTED 0

/**
 * Represents either a set of planes, or a sphere,
 */
struct FCullingVolume
{
	FConvexVolume ConvexVolume;
	FSphere3d Sphere = FSphere(ForceInit);
};


class FSceneCullingBuilder;

class FSceneCulling
{
public:
	static constexpr uint32 InvalidCellFlag = 1U << 31;
	static constexpr uint32 TempCellFlag = 1U << 30;

	friend class FSceneCullingRenderer;

	FSceneCulling(FScene& InScene);

	/** 
	 */
	class FUpdater
	{
	public:
		void OnPreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ScenePreUpdateData);
		// Call to get a task to wait for the pre-update task to finish using scene proxies that were deleted - before actually deleting them
		UE::Tasks::FTask GetAsyncProxyUseTaskHandle();
		void OnPostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ScenePostUpdateData);
		void FinalizeAndClear(FRDGBuilder& GraphBuilder, bool bPublishStats);

		~FUpdater();

	private:
		friend class FSceneCulling;

		UE::Tasks::FTask PreUpdateTaskHandle;
		UE::Tasks::FTask PostUpdateTaskHandle;

		FSceneCullingBuilder *Implementation = nullptr;

#if DO_CHECK
		std::atomic<int32> DebugTaskCounter = 0;
#endif
	};
	/**
	 * Set up update driver that can collect change sets and initiate async update. The updater (internals) has RDG scope.
	 */
	FUpdater &BeginUpdate(FRDGBuilder& GraphBuilder);

	/**
	 * Finalize update of hierarchy, should be done as late as possible, also performs update of RDG resources. 
	 * May be called multiple times, the first call does the work.
	 */
	void EndUpdate(FRDGBuilder& GraphBuilder, bool bPublishStats);

	UE::Tasks::FTask GetUpdateTaskHandle() const;

	bool IsEnabled() const { return bIsEnabled; }

	void PublishStats();

	void TestConvexVolume(const FConvexVolume& ViewCullVolume, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups);

	void TestSphere(const FSphere& Sphere, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups);

	void Test(const FCullingVolume& CullingVolume, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups);

	using FSpatialHash = THierarchicalSpatialHashGrid<3>;

	using FLocation64 = FSpatialHash::FLocation64;
	using FLocation32 = FSpatialHash::FLocation32;
	using FLocation8 = FSpatialHash::FLocation8;

	using FFootprint8 = FSpatialHash::FFootprint8;
	using FFootprint32 = FSpatialHash::FFootprint32;
	using FFootprint64 = FSpatialHash::FFootprint64;	
private:
	void ValidateAllInstanceAllocations();

	void Empty();

	/**
	 * The cache stores info about what cells the instances are inserted into in the grid, such that we can remove/update without needing to recompute the full transformation.
	 */
	struct FCellIndexCacheEntry
	{
		static constexpr int32 InstanceCountNumBits = 12;
		static constexpr int32 InstanceCountMax = (1 << InstanceCountNumBits);
		static constexpr int32 CellIndexMax = 1 << (32 - InstanceCountNumBits);
		struct FItem
		{
			uint32 NumInstances : InstanceCountNumBits;
			uint32 CellIndex : 32 - InstanceCountNumBits;
		};
		inline void Add(int32 CellIndex, int32 NumInstances)
		{
			check(CellIndex < CellIndexMax);
			
			FItem Item;
			Item.CellIndex = CellIndex;
			// break into blocks...
			while (NumInstances >= InstanceCountMax)
			{
				Item.NumInstances = InstanceCountMax - 1;
				NumInstances -= InstanceCountMax - 1;
				Items.Add(Item);
			}

			check(NumInstances < InstanceCountMax);
			Item.NumInstances = NumInstances;
			Items.Add(Item);
		}
		inline void Set(int32 Index, int32 CellIndex, int32 NumInstances)
		{
			check(CellIndex < CellIndexMax);
			check(NumInstances < InstanceCountMax);

			FItem Item;
			Item.CellIndex = CellIndex;
			Item.NumInstances = NumInstances;
			Items[Index] = Item;
		}
		TArray<FItem> Items;
	};

	/** 
	 * Tracking state of each added primitive, needed to be able to transition ones that change category when updated & correctly remove.
	 */
	struct FPrimitiveState
	{
		static constexpr int32 PayloadBits = 28;
		static constexpr uint32 InvalidPayload = (1u << PayloadBits) - 1u;
		FPrimitiveState() 
			: InstanceDataOffset(-1)
			, NumInstances(0)
			, State(Unknown)
			, bDynamic(false)
			, Payload(InvalidPayload)
		{
		}

		enum EState : uint32
		{
			Unknown,
			SinglePrim,
#if SCENE_CULLING_USE_PRECOMPUTED
			Precomputed,
#endif
			UnCullable,
			Dynamic,
			Cached,
		};

		inline bool IsCachedState() const { return State == Cached || State == Dynamic; }

		int32 InstanceDataOffset;
		int32 NumInstances;
		EState State : 3;
		// The bDynamic flag is used to record whether a primitive has been seen to be updated. This can happen, for example for a stationary primitive, if this happens it is transitioned to Dynamic.
		bool bDynamic : 1;
		// For SinglePrim primitives the payload represents the cell index directly, whereas for instance
		uint32 Payload :28;

		const FString &ToString() const;

#if SCENE_CULLING_USE_PRECOMPUTED && DO_GUARD_SLOW
		TArray<FPrimitiveSceneProxy::FCompressedSpatialHashItem> CompressedInstanceSpatialHashes;
#endif
	};

	TArray<FPrimitiveState> PrimitiveStates;
	TSparseArray<FCellIndexCacheEntry> CellIndexCache;
	int32 TotalCellIndexCacheItems = 0;

	friend class FSceneCullingBuilder;
	friend class FSceneInstanceCullingQuery;

	FScene& Scene;
	bool bIsEnabled = false;
	FSpatialHash SpatialHash;

	// Kept in the class for now, since we only want one active at a time anyway.
	FUpdater Updater;

	// A cell stores references to a list of chunks, that, in turn, reference units of 64 instances. 
	// This enables storing compressed chunks directly in the indirection, as well as simplifying allocation and movement of instance data lists.
	TArray<uint32> PackedCellChunkData;
	FSpanAllocator CellChunkIdAllocator;
	TArray<uint32> PackedCellData;
	TArray<uint32> FreeChunks;
	TArray<FCellHeader> CellHeaders;
	TBitArray<> CellOccupancyMask;
	TBitArray<> BlockLevelOccupancyMask;

	TArray<FCellBlockData> CellBlockData;
	TArray<FPersistentPrimitiveIndex> UnCullablePrimitives;
	int32 UncullableItemChunksOffset = INDEX_NONE;
	int32 UncullableNumItemChunks = 0;
	// Number of cells in the finest level under which a footprint is considered "small" and should go down the direct footprint path
	// TODO: Maybe better to use some other metric? A sphere could also be tested for insideness, and that process would be efficient for a large
	//       enough sphere, and able to trim more than a set of planes But perhaps marginal anyway?
	int32 SmallFootprintCellCountThreshold = 0;
	bool bTestCellVsQueryBounds = true;
	bool bUseAsyncUpdate = true;
	bool bUseAsyncQuery = true;

	inline uint32 AllocateChunk();
	inline void FreeChunk(uint32 ChunkId);
	inline int32 CellIndexToBlockId(int32 CellIndex);
	inline FLocation64 GetCellLoc(int32 CellIndex);
	inline bool IsUncullable(const FPrimitiveBounds& Bounds, FPrimitiveSceneInfo* PrimitiveSceneInfo);

	// Persistent GPU-representation
	TPersistentStructuredBuffer<FCellHeader> CellHeadersBuffer;
	TPersistentStructuredBuffer<uint32> ItemChunksBuffer;
	TPersistentStructuredBuffer<uint32> ItemsBuffer;
	TPersistentStructuredBuffer<FCellBlockData> CellBlockDataBuffer;
};
