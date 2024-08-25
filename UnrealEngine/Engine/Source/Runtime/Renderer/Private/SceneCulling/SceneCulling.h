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
#include "InstanceDataSceneProxy.h"

class FScenePreUpdateChangeSet;
class FScenePostUpdateChangeSet;
class FScene;
struct FPrimitiveBounds;

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

		void OnPostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ScenePostUpdateData);
		void FinalizeAndClear(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer, bool bPublishStats);

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
	FUpdater &BeginUpdate(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer, bool bAnySceneUpdatesExpected);

	/**
	 * Finalize update of hierarchy, should be done as late as possible, also performs update of RDG resources. 
	 * May be called multiple times, the first call does the work.
	 */
	void EndUpdate(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer, bool bPublishStats);

	UE::Tasks::FTask GetUpdateTaskHandle() const;

	bool IsEnabled() const { return bIsEnabled; }

	void PublishStats();

	void TestConvexVolume(const FConvexVolume& ViewCullVolume, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups);

	void TestSphere(const FSphere& Sphere, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups);

	void Test(const FCullingVolume& CullingVolume, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups);

	struct alignas(16) FBlockLocAligned
	{
		FORCEINLINE FBlockLocAligned() {}

		FORCEINLINE explicit FBlockLocAligned(const RenderingSpatialHash::TLocation<int64> &InLoc)
			: Data(int32(InLoc.Coord.X), int32(InLoc.Coord.Y), int32(InLoc.Coord.Z), int32(InLoc.Level))
		{
		}

		FORCEINLINE bool operator==(const FBlockLocAligned& BlockLocAligned) const
		{
			return Data == BlockLocAligned.Data;
		}

		FORCEINLINE void operator=(const FBlockLocAligned &BlockLocAligned)
		{
			Data = BlockLocAligned.Data;
		}
		FORCEINLINE int32 GetLevel() const { return Data.W; }

		FORCEINLINE FIntVector3 GetCoord() const { return FIntVector3(Data.X, Data.Y, Data.Z); }

		FORCEINLINE FVector3d GetWorldPosition() const
		{
			double LevelSize = RenderingSpatialHash::GetCellSize(Data.W);
			return FVector3d(GetCoord()) * LevelSize;
		}

		FORCEINLINE uint32 GetHash() const
		{
			// TODO: Vectorize? Maybe convert to float vector & use dot product? Maybe not? (mul is easy, dot maybe not?)
			return uint32(Data.X * 1150168907 + Data.Y * 1235029793 + Data.Z * 1282581571 + Data.W * 1264559321);
		}

		FIntVector4 Data;
	};

	using FBlockLoc = FBlockLocAligned;

	struct FBlockTraits
	{
		static constexpr int32 CellBlockDimLog2 = 3; // (8x8x8)
		using FBlockLoc = FBlockLoc;

		// The FBlockLocAligned represents the block locations as 32-bit ints.
		static constexpr int64 MaxCellBlockCoord = MAX_int32;
		// The cell coordinate may be larger by the block dimension and still can fit into a signed 32-bit integer
		static constexpr int64 MaxCellCoord = MaxCellBlockCoord << CellBlockDimLog2;
	};

	using FSpatialHash = THierarchicalSpatialHashGrid<FBlockTraits>;

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

		FORCEINLINE void Reset() 
		{
			Items.Reset();
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
			Precomputed,
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

		TSharedPtr<FInstanceSceneDataImmutable, ESPMode::ThreadSafe> InstanceSceneDataImmutable;
	};

	TArray<FPrimitiveState> PrimitiveStates;
	TSparseArray<FCellIndexCacheEntry> CellIndexCache;
	int32 TotalCellIndexCacheItems = 0;

	int32 NumDynamicInstances = 0;
	int32 NumStaticInstances = 0;
	

	friend class FSceneCullingBuilder;
	friend class FSceneInstanceCullingQuery;

	FScene& Scene;
	bool bIsEnabled = false;
	bool bUseExplictBounds = false;
	FSpatialHash SpatialHash;

	// Kept in the class for now, since we only want one active at a time anyway.
	FUpdater Updater;

	// A cell stores references to a list of chunks, that, in turn, reference units of 64 instances. 
	// This enables storing compressed chunks directly in the indirection, as well as simplifying allocation and movement of instance data lists.
	TArray<uint32> PackedCellChunkData;
	FSpanAllocator CellChunkIdAllocator;
	TArray<uint32> PackedCellData;
	TArray<uint32> FreeChunks;
	TArray<FPackedCellHeader> CellHeaders;
	TBitArray<> CellOccupancyMask;
	TBitArray<> BlockLevelOccupancyMask;

	TArray<FCellBlockData> CellBlockData;
	TArray<FPersistentPrimitiveIndex> UnCullablePrimitives;
	int32 UncullableItemChunksOffset = INDEX_NONE;
	int32 UncullableNumItemChunks = 0;
	// Largest dimension length, in cells, at the finest level under which a footprint is considered "small" and should go down the direct footprint path
	int32 SmallFootprintCellSideThreshold = 16;
	bool bTestCellVsQueryBounds = true;
	bool bUseAsyncUpdate = true;
	bool bUseAsyncQuery = true;
	bool bPackedCellDataLocked = false;

	inline uint32 AllocateChunk();
	inline void FreeChunk(uint32 ChunkId);
	inline uint32* LockChunkCellData(uint32 ChunkId, int32 NumSlackChunksNeeded);
	inline void UnLockChunkCellData(uint32 ChunkId);
	inline int32 CellIndexToBlockId(int32 CellIndex);
	inline FLocation64 GetCellLoc(int32 CellIndex);
	inline bool IsUncullable(const FPrimitiveBounds& Bounds, FPrimitiveSceneInfo* PrimitiveSceneInfo);

	// Persistent GPU-representation
	TPersistentStructuredBuffer<FPackedCellHeader> CellHeadersBuffer;
	TPersistentStructuredBuffer<uint32> ItemChunksBuffer;
	TPersistentStructuredBuffer<uint32> InstanceIdsBuffer;
	TPersistentStructuredBuffer<FCellBlockData> CellBlockDataBuffer;
	TPersistentStructuredBuffer<FVector4f> ExplicitCellBoundsBuffer;
};
