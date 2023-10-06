// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneCulling.h"
#include "SceneCullingRenderer.h"
#include "ScenePrivate.h"
#include "ComponentRecreateRenderStateContext.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemStats.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "DynamicPrimitiveDrawing.h"
#endif

#define OLA_TODO 0

#define SC_ENABLE_DETAILED_LOGGING 0 //(UE_BUILD_DEBUG)
#define SC_ENABLE_GPU_DATA_VALIDATION (DO_CHECK)
#define SC_ALLOW_ASYNC_TASKS 1

// TODO: this might be adding too much overhead in Development as well...
#define SC_ENABLE_DETAILED_BUILDER_STATS (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))

DECLARE_STATS_GROUP(TEXT("SceneCulling"), STATGROUP_SceneCulling, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Test"), STAT_SceneCulling_Test, STATGROUP_SceneCulling);
DECLARE_CYCLE_STAT(TEXT("Test Sphere"), STAT_SceneCulling_Test_Sphere, STATGROUP_SceneCulling);
DECLARE_CYCLE_STAT(TEXT("Test Convex"), STAT_SceneCulling_Test_Convex, STATGROUP_SceneCulling);

DECLARE_DWORD_COUNTER_STAT(TEXT("Test Sphere Blocks"), STAT_SceneCulling_TestSphereBlocks, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Test Sphere Cells"), STAT_SceneCulling_TestSphereCells, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Test Sphere Bounds"), STAT_SceneCulling_TestSphereBounds, STATGROUP_SceneCulling);

DECLARE_CYCLE_STAT(TEXT("Update Pre"), STAT_SceneCulling_Update_Pre, STATGROUP_SceneCulling);
DECLARE_CYCLE_STAT(TEXT("Update Post"), STAT_SceneCulling_Update_Post, STATGROUP_SceneCulling);
DECLARE_CYCLE_STAT(TEXT("Update Finalize"), STAT_SceneCulling_Update_FinalizeAndClear, STATGROUP_SceneCulling);

DECLARE_DWORD_COUNTER_STAT(TEXT("Removed Instances"),  STAT_SceneCulling_RemovedInstanceCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Updated Instances"),  STAT_SceneCulling_UpdatedInstanceCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Added Instances"),  STAT_SceneCulling_AddedInstanceCount, STATGROUP_SceneCulling);

DECLARE_DWORD_COUNTER_STAT(TEXT("Update Uploaded Chunks"), STAT_SceneCulling_UploadedChunks, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Update Uploaded Cells"), STAT_SceneCulling_UploadedCells, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Update Uploaded Items"), STAT_SceneCulling_UploadedItems, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Update Uploaded Blocks"), STAT_SceneCulling_UploadedBlocks, STATGROUP_SceneCulling);

DECLARE_DWORD_COUNTER_STAT(TEXT("Block Count"), STAT_SceneCulling_BlockCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Cell Count"), STAT_SceneCulling_CellCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Item Chunk Count"), STAT_SceneCulling_ItemChunkCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Item Count"), STAT_SceneCulling_ItemCount, STATGROUP_SceneCulling);

DECLARE_DWORD_COUNTER_STAT(TEXT("Total Id Cache Size"), STAT_SceneCulling_IdCacheSize, STATGROUP_SceneCulling);

#if SC_ENABLE_DETAILED_BUILDER_STATS

// Detailed stat?
DECLARE_DWORD_COUNTER_STAT(TEXT("Non-Empty Cell Count"), STAT_SceneCulling_NonEmptyCellCount, STATGROUP_SceneCulling);

DECLARE_DWORD_COUNTER_STAT(TEXT("Ranges Added"), STAT_SceneCulling_RangeCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Comp. Ranges"), STAT_SceneCulling_CompRangeCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Update Visited Id Count"), STAT_SceneCulling_VisitedIdCount, STATGROUP_SceneCulling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Update Copied Id Count"), STAT_SceneCulling_CopiedIdCount, STATGROUP_SceneCulling);

#endif

#if SC_ENABLE_DETAILED_BUILDER_STATS
	#define UPDATE_BUILDER_STAT(Builder, StatId, Delta) (Builder).Stats.StatId += Delta
#else
	#define UPDATE_BUILDER_STAT(Builder, StatId, Num) 
#endif

CSV_DEFINE_CATEGORY(SceneCulling, true);


LLM_DECLARE_TAG_API(SceneCulling, RENDERER_API);
DECLARE_LLM_MEMORY_STAT(TEXT("SceneCulling"), STAT_SceneCullingLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("SceneCulling"), STAT_SceneCullingSummaryLLM, STATGROUP_LLM);
LLM_DEFINE_TAG(SceneCulling, NAME_None, NAME_None, GET_STATFNAME(STAT_SceneCullingLLM), GET_STATFNAME(STAT_SceneCullingLLM));

static TAutoConsoleVariable<int32> CVarSceneCulling(
	TEXT("r.SceneCulling"), 
	0, 
	TEXT("Enable/Disable scene culling.\n")
	TEXT("  Forces a recreate of all render state since (at present) there is only an incremental update path."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarSceneCullingAsyncUpdate(
	TEXT("r.SceneCulling.Async.Update"), 
	1, 
	TEXT("Enable/Disable async culling scene update."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSceneCullingAsyncQuery(
	TEXT("r.SceneCulling.Async.Query"), 
	1, 
	TEXT("Enable/Disable async culling scene queries."),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<float> CVarSceneCullingMinCellSize(
	TEXT("r.SceneCulling.MinCellSize"), 
	4096.0f, 
	TEXT("Set the minimum cell size & level in the hierarchy, rounded to nearest POT. Clamps the level for object footprints.\n")
	TEXT("  This trades culling effectiveness for construction cost and memory use.\n")
	TEXT("  The minimum cell size is (will be) used for precomputing the key for static ISMs.\n")
	TEXT("  Currently read-only as there is implementation to rebuild the whole structure on a change.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarSceneCullingMaxCellSize(
	TEXT("r.SceneCulling.MaxCellSize"), 
	UE_OLD_HALF_WORLD_MAX, 
	TEXT("Hierarchy max cell size. Objects with larger bounds will be classified as uncullable."), 
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarSmallFootprintCellCountThreshold(
	TEXT("r.SceneCulling.SmallFootprintCellCountThreshold"), 
	512, 
	TEXT("Queries with a smaller footprint (in number of cells in the lowest level) go down the footprint based path."), 
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarValidateAllInstanceAllocations(
	TEXT("r.SceneCulling.ValidateAllInstanceAllocations"), 
	0, 
	TEXT("Perform validation of all instance IDs stored in the grid. This is very slow."), 
	ECVF_RenderThreadSafe);

#if SC_ENABLE_GPU_DATA_VALIDATION

static TAutoConsoleVariable<int32> CVarValidateGPUData(
	TEXT("r.SceneCulling.ValidateGPUData"), 
	0, 
	TEXT("Perform readback and validation of uploaded GPU-data against CPU copy. This is quite slow and forces CPU/GPU syncs."), 
	ECVF_RenderThreadSafe);


#endif

#if SC_ENABLE_DETAILED_LOGGING
static FSceneCullingBuilder *GBuilderForLogging = nullptr;
#define BUILDER_LOG(Fmt, ...) GBuilderForLogging->AddLog(FString::Printf(TEXT(Fmt), __VA_ARGS__))

struct FIHLoggerScopeHelper
{
	FIHLoggerScopeHelper();
	~FIHLoggerScopeHelper();
};
struct FIHLoggerListScopeHelper
{
	FIHLoggerListScopeHelper(const FString &InListName);

	~FIHLoggerListScopeHelper();

	void Add(const FString &Item)
	{
		if(bEnabled)
		{
			LogStr.Appendf(TEXT("%s%s"), bFirst ? TEXT("") : TEXT(", "), *Item);
		}
		bFirst = false;
	}
	bool bEnabled = true;
	bool bFirst = true;
	FString LogStr;
};

#define BUILDER_LOG_SCOPE(Fmt, ...) GBuilderForLogging->AddLog(FString::Printf(TEXT(Fmt), __VA_ARGS__)); FIHLoggerScopeHelper IHLoggerScopeHelper
#define BUILDER_LOG_LIST(Fmt, ...) FIHLoggerListScopeHelper IHLoggerListScopeHelper(FString::Printf(TEXT(Fmt), __VA_ARGS__))
#define BUILDER_LOG_LIST_APPEND(Fmt, ...) IHLoggerListScopeHelper.Add(FString::Printf(TEXT(Fmt), __VA_ARGS__))

static TAutoConsoleVariable<int32> CVarSceneCullingLogBuild(
	TEXT("r.SceneCulling.LogBuild"), 
	0, 
	TEXT("."), 
	ECVF_RenderThreadSafe);

#else
#define BUILDER_LOG(...)
#define BUILDER_LOG_SCOPE(...)
#define BUILDER_LOG_LIST(...)
#define BUILDER_LOG_LIST_APPEND(...)
#endif

// doesn't exist in the global definitions for some reason
using FInt8Vector3 = UE::Math::TIntVector3<int8>;


FString GSceneCullingDbgPattern;// = "Cube*";
FAutoConsoleVariableRef CVarSceneCullingDbgPattern(
	TEXT("r.SceneCulling.DbgPattern"),
	GSceneCullingDbgPattern,
	TEXT(""),
	ECVF_RenderThreadSafe
);


const FString &FSceneCulling::FPrimitiveState::ToString() const
{
	static FString Result;
	Result = TEXT("{ ");
	switch(State)
	{
	case Unknown:
		Result.Append(TEXT("Unknown"));
		break;
	case SinglePrim:
		Result.Append(TEXT("SinglePrim"));
		break;
#if SCENE_CULLING_USE_PRECOMPUTED
	case Precomputed:
		Result.Append(TEXT("Precomputed"));
		break;
#endif
	case Dynamic:
		Result.Append(TEXT("Dynamic"));
		break;
	case Cached:
		Result.Append(TEXT("Cached"));
		break;
	case UnCullable:
		Result.Append(TEXT("UnCullable"));
		break;
	};
	Result.Appendf(TEXT(", InstanceDataOffset %d, NumInstances %d, bDynamic %d, Payload %d }"), InstanceDataOffset, NumInstances, bDynamic, Payload);
	return Result;
}

#if SCENE_CULLING_USE_PRECOMPUTED
inline bool operator==(const FPrimitiveSceneProxy::FCompressedSpatialHashItem A, const FPrimitiveSceneProxy::FCompressedSpatialHashItem B)
{
	return A.Location == B.Location && A.NumInstances == B.NumInstances;
}
#endif

struct FSpatialHashNullDebugDrawer
{
	inline void OnBlockBegin(RenderingSpatialHash::FLocation64 BlockLoc) {}
	inline void DrawCell(bool bShouldDraw, bool bHighlight, const FVector3d& CellCenter, const FVector3d& CellBoundsExtent) {}
	inline void DrawBlock(bool bHighlight, const FBox& BlockBounds) {}

};
#if 0

struct FSpatialHashDebugDrawer
{
	FLinearColor LevelColor;
	FLinearColor CellLevelColor;
	FViewElementPDI* DebugPDI;
	bool bDebugDrawCells;
	bool bDebugDrawBlocks;

	void OnBlockBegin(RenderingSpatialHash::FLocation64 BlockLoc)
	{
		LevelColor = FLinearColor::MakeRandomSeededColor(BlockLoc.Level);
		CellLevelColor = FLinearColor::MakeRandomSeededColor(BlockLoc.Level - FSceneCulling::FSpatialHash::CellBlockDimLog2);
	}
	void DrawCell(bool bShouldDraw, bool bHighlight, const FVector3d& CellCenter, const FVector3d& CellBoundsExtent)
	{
		if (bDebugDrawCells)
		{
			if (bShouldDraw)
			{
				if (bDebugDrawCells)
				{
					DrawWireBox(DebugPDI, FBox3d(CellCenter - CellBoundsExtent, CellCenter + CellBoundsExtent), CellLevelColor * (bHighlight ? 1.0f : 0.2f), SDPG_World);
				}
			}
		}
	}

	void DrawBlock(bool bHighlight, const FBox& BlockBounds)
	{
		if (bDebugDrawBlocks)
		{
			DrawWireBox(DebugPDI, BlockBounds, LevelColor * (bHighlight ? 1.0f : 0.2f), SDPG_World);
		}
	}
};
#endif

inline bool IsValid(const FCellHeader& CellHeader)
{
	return (CellHeader.NumItemChunks & FSceneCulling::InvalidCellFlag) == 0;
}

inline bool IsTempCell(const FCellHeader& CellHeader)
{
	return (CellHeader.NumItemChunks & FSceneCulling::TempCellFlag) != 0;
}

template <typename ScalarType>
inline UE::Math::TIntVector3<ScalarType> ClampDim(const UE::Math::TIntVector3<ScalarType>& Vec, ScalarType MinValueInc, ScalarType MaxValueInc)
{
	return UE::Math::TIntVector3<ScalarType>(
		FMath::Clamp(Vec.X, MinValueInc, MaxValueInc),
		FMath::Clamp(Vec.Y, MinValueInc, MaxValueInc),
		FMath::Clamp(Vec.Z, MinValueInc, MaxValueInc));
};

inline FSceneCulling::FFootprint8 ToBlockLocal(const FSceneCulling::FFootprint64& ObjFootprint, const FSceneCulling::FLocation64& BlockLoc)
{
	FInt64Vector3 BlockMin = BlockLoc.Coord * FSceneCulling::FSpatialHash::CellBlockDim;
	FInt64Vector3 BlockMax = BlockMin + FInt64Vector3(FSceneCulling::FSpatialHash::CellBlockDim - 1);

	// This can be packed to very few bits if need be.
	FSceneCulling::FFootprint8 LocalFp = {
		ClampDim(FInt8Vector3(ObjFootprint.Min - BlockMin), int8(0), int8(FSceneCulling::FSpatialHash::CellBlockDim - 1)),
		ClampDim(FInt8Vector3(ObjFootprint.Max - BlockMin), int8(0), int8(FSceneCulling::FSpatialHash::CellBlockDim - 1)),
		ObjFootprint.Level
	};
	return LocalFp;
};

inline FSceneCulling::FLocation8 ToBlockLocal(const FSceneCulling::FLocation64& ItemLoc, const FSceneCulling::FLocation64& BlockLoc)
{
	checkSlow(ItemLoc.Level - BlockLoc.Level);
	FInt64Vector3 BlockMin = BlockLoc.Coord << FSceneCulling::FSpatialHash::CellBlockDimLog2;

	// This can be packed to very few bits if need be.
	FSceneCulling::FLocation8 LocalLoc;
	LocalLoc.Coord = FInt8Vector3(ItemLoc.Coord - BlockMin);
	LocalLoc.Level = ItemLoc.Level;

	checkSlow(LocalLoc.Coord.X >= 0 && LocalLoc.Coord.X < FSceneCulling::FSpatialHash::CellBlockDim);
	checkSlow(LocalLoc.Coord.Y >= 0 && LocalLoc.Coord.Y < FSceneCulling::FSpatialHash::CellBlockDim);
	checkSlow(LocalLoc.Coord.Z >= 0 && LocalLoc.Coord.Z < FSceneCulling::FSpatialHash::CellBlockDim);

	return LocalLoc;
};

inline FInt64Vector3 ToLevelRelative(const FInt64Vector3& Coord, int32 LevelDelta)
{
	if (LevelDelta > 0)
	{
		return Coord >> LevelDelta;
	}
	else if (LevelDelta < 0)
	{
		return Coord << -LevelDelta;
	}
	return Coord;
}

inline FSceneCulling::FLocation64 ToLevelRelative(const FSceneCulling::FLocation64& Loc, int32 LevelDelta)
{
	if (LevelDelta == 0)
	{
		return Loc;
	}
	FSceneCulling::FLocation64 ResLoc = Loc;
	ResLoc.Level += LevelDelta;
	ResLoc.Coord = ToLevelRelative(Loc.Coord, LevelDelta);
	return ResLoc;
};

inline FSceneCulling::FFootprint64 ToLevelRelative(const FSceneCulling::FFootprint64& Footprint, int32 LevelDelta)
{
	FSceneCulling::FFootprint64 Result;
	Result.Min = ToLevelRelative(Footprint.Min, LevelDelta);
	Result.Max = ToLevelRelative(Footprint.Max, LevelDelta);
	Result.Level = Footprint.Level + LevelDelta;
	return Result;
};

void FSceneCulling::TestConvexVolume(const FConvexVolume& ViewCullVolume, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups)
{
	LLM_SCOPE_BYTAG(SceneCulling);

	SCOPE_CYCLE_COUNTER(STAT_SceneCulling_Test_Convex);
	FSpatialHashNullDebugDrawer DebugDrawer;

	OutNumInstanceGroups += UncullableNumItemChunks;

	for (auto It = SpatialHash.GetHashMap().begin(); It != SpatialHash.GetHashMap().end(); ++It)
	{
		const auto& BlockItem = *It;
		int32 BlockIndex = It.GetElementId().GetIndex();
		const FSpatialHash::FCellBlock& Block = BlockItem.Value;
		FLocation64 BlockLoc = BlockItem.Key;

		DebugDrawer.OnBlockBegin(BlockLoc);

		const double BlockLevelSize = SpatialHash.GetCellSize(BlockLoc.Level);
		FVector3d BlockBoundsCenter = FVector3d(BlockLoc.Coord) * BlockLevelSize + BlockLevelSize * 0.5;
		const double LevelCellSize = SpatialHash.GetCellSize(BlockLoc.Level - FSpatialHash::CellBlockDimLog2);
		// Extend extent by half a cell size in all directions
		FVector3d BlockBoundsExtent = FVector((BlockLevelSize + LevelCellSize) * 0.5);
		FOutcode BlockCullResult = ViewCullVolume.GetBoxIntersectionOutcode(BlockBoundsCenter, BlockBoundsExtent);

		if (BlockCullResult.GetInside())
		{
			const bool bIsContained = !BlockCullResult.GetOutside();
			if (bIsContained)
			{
				// TODO: collect this in a per-block summary so we don't need to hit cell headers in this path
				OutNumInstanceGroups += Block.NumItemChunks;

				// Fully inside, just append non-empty cells
				int32 BitRangeEnd = Block.GridOffset + FSpatialHash::CellBlockSize;
				for (TConstSetBitIterator<> BitIt(CellOccupancyMask, Block.GridOffset); BitIt && BitIt.GetIndex() < BitRangeEnd; ++BitIt)
				{
					uint32 CellId = uint32(BitIt.GetIndex());
					OutCellDraws.Add(FCellDraw{ CellId, ViewGroupId });
				}
			}
			else
			{
				FVector3d CellBoundsExtent = FVector3d(LevelCellSize);
				FVector3d MinCellCenter = FVector3d(BlockLoc.Coord) * BlockLevelSize + LevelCellSize * 0.5;

				int32 BitRangeEnd = Block.GridOffset + FSpatialHash::CellBlockSize;
				for (TConstSetBitIterator<> BitIt(CellOccupancyMask, Block.GridOffset); BitIt && BitIt.GetIndex() < BitRangeEnd; ++BitIt)
				{
					uint32 CellId = uint32(BitIt.GetIndex());
					FInt8Vector3 CellCoord;
					{
						uint32 BlockCellIndex = CellId - Block.GridOffset;
						CellCoord.X = BlockCellIndex & FSpatialHash::LocalCellCoordMask;
						BlockCellIndex = BlockCellIndex >> FSpatialHash::CellBlockDimLog2;
						CellCoord.Y = BlockCellIndex & FSpatialHash::LocalCellCoordMask;
						BlockCellIndex = BlockCellIndex >> FSpatialHash::CellBlockDimLog2;
						CellCoord.Z = BlockCellIndex;
					}
					// world-space double precision test, can equally translate to view local and do single precision test (with epsilons perhaps)
					FVector3d CellCenter = FVector3d(CellCoord) * LevelCellSize + MinCellCenter;

					// emit for intersecting cells
					bool bCellIntersects = ViewCullVolume.IntersectBox(CellCenter, CellBoundsExtent);
					if (bCellIntersects)
					{
						FCellHeader CellHeader = CellHeaders[CellId];
						check(IsValid(CellHeader));
						OutNumInstanceGroups += CellHeader.NumItemChunks;
						OutCellDraws.Add(FCellDraw{ CellId, ViewGroupId });
					}
					//DebugDrawer.DrawCell(bCellIntersects && !bIsContained, bCellIntersects, CellCenter, CellBoundsExtent);
				}
				//DebugDrawer.DrawBlock(bIsContained, BlockBounds);
			}
		}
	}
}

void FSceneCulling::TestSphere(const FSphere& Sphere, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups)
{
	LLM_SCOPE_BYTAG(SceneCulling);

	SCOPE_CYCLE_COUNTER(STAT_SceneCulling_Test_Sphere);
	FSpatialHashNullDebugDrawer DebugDrawer;

	OutNumInstanceGroups += UncullableNumItemChunks;

	const FSpatialHash::FSpatialHashMap &GlobalSpatialHash = SpatialHash.GetHashMap();

	// TODO[Opt]: Maybe specialized bit set since we have a fixed size & alignment guaranteed (64-bit words all in use)
	// TODO[Opt]: Add a per-view grid / cache that works like the VSM page table and allows skipping within the footprint?
	for (TConstSetBitIterator<> BitIt(BlockLevelOccupancyMask); BitIt; ++BitIt)
	{
		int32 BlockLevel = BitIt.GetIndex();
		int32 Level = BlockLevel - FSpatialHash::CellBlockDimLog2;
		// Note float size, this is intentional, the idea should be to never have cell sizes of unusual size 
		const float LevelCellSize = SpatialHash.GetCellSize(Level);

		// TODO[Opt]: may be computed as a relative from the previous level, needs to be adjusted for skipping levels:
		//    Expand by 1 (half a cell on the next level) before dividing to maintain looseness
		//      LightFootprint.Min -= FInt64Vector3(1);
		//      LightFootprint.Max += FInt64Vector3(1);
		//      LightFootprint = ToLevelRelative(LightFootprint, 1);
		FFootprint64 LightFootprint = SpatialHash.CalcFootprintSphere(Level, Sphere.Center, Sphere.W + (LevelCellSize * 0.5f));

		FFootprint64 BlockFootprint = SpatialHash.CalcCellBlockFootprint(LightFootprint);
		check(BlockFootprint.Level == BlockLevel);
		const float BlockSize = SpatialHash.GetCellSize(BlockFootprint.Level);

		// Loop over footprint
		BlockFootprint.ForEach([&](const FLocation64& BlockLoc)
		{
			INC_DWORD_STAT(STAT_SceneCulling_TestSphereBlocks);
			// TODO[Opt]: Add cache for block ID lookups? The hash lookup is somewhat costly and we hit it quite a bit due to the loose footprint.
			//       Could be a 3d grid/level (or not?) with modulo and use the BlockLoc as key. Getting very similar to just using a cheaper hash...
			FSpatialHash::FBlockId BlockId = GlobalSpatialHash.FindId(BlockLoc);
			if (BlockId.IsValid())
			{
				DebugDrawer.OnBlockBegin(BlockLoc);

				const FSpatialHash::FCellBlock& Block = GlobalSpatialHash.GetByElementId(BlockId).Value;
				FVector3d BlockWorldPos = FVector3d(BlockLoc.Coord) * double(BlockSize);

				// relative query offset, float precision.
				// This is probably not important on PC, but on GPU the block world pos can be precomputed on host and this gets us out of large precision work
				// Expand by 1/2 cell size for loose
				FSphere3f BlockLocalSphere(FVector3f(Sphere.Center - BlockWorldPos), float(Sphere.W) + (LevelCellSize * 0.5f));

				FFootprint8 LightFootprintInBlock = ToBlockLocal(LightFootprint, BlockLoc);

				// Calc block mask 
				// TODO[Opt]: We can make a table of this and potentially save a bit of work here
				uint64 LightCellMask = FSpatialHash::FCellBlock::BuildFootPrintMask(LightFootprintInBlock);

				if ((Block.CoarseCellMask & LightCellMask) != 0ULL)
				{
					LightFootprintInBlock.ForEach([&](const FLocation8& CellSubLoc)
					{
						INC_DWORD_STAT(STAT_SceneCulling_TestSphereCells);

						if ((Block.CoarseCellMask & FSpatialHash::FCellBlock::CalcCellMask(CellSubLoc.Coord)) != 0ULL)
						{
							// optionally test the cell bounds against the query
							// 1. Make local bounding box (we could do a global one but this is more GPU friendly)
							// Note: not expanded because the query is:
							FBox3f Box;
							Box.Min = FVector3f(CellSubLoc.Coord) * LevelCellSize;
							Box.Max = Box.Min + LevelCellSize;

							bool bIntersects = true;

							if (bTestCellVsQueryBounds)
							{
								INC_DWORD_STAT(STAT_SceneCulling_TestSphereBounds);
								if (!FMath::SphereAABBIntersection(BlockLocalSphere, Box))
								{
									bIntersects = false;
								}
							}
							if (bIntersects)
							{
								uint32 CellId = Block.GetCellGridOffset(CellSubLoc.Coord);
								FCellHeader CellHeader = CellHeaders[CellId];
								if (IsValid(CellHeader))
								{
									OutNumInstanceGroups += CellHeader.NumItemChunks;
									OutCellDraws.Add(FCellDraw{ CellId, ViewGroupId });
								}
							}

							DebugDrawer.DrawCell(bIntersects, bIntersects, BlockWorldPos + FVector3d(Box.GetCenter()), FVector3d(Box.GetExtent()));
						}
					});
				}
				DebugDrawer.DrawBlock(true, SpatialHash.CalcBlockBounds(BlockLoc));
			}
		});
	}
}

void FSceneCulling::Test(const FCullingVolume& CullingVolume, TArray<FCellDraw, SceneRenderingAllocator>& OutCellDraws, uint32 ViewGroupId, uint32 MaxNumViews, uint32& OutNumInstanceGroups)
{
	LLM_SCOPE_BYTAG(SceneCulling);

	SCOPED_NAMED_EVENT(SceneCulling_Test, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_SceneCulling_Test);

	if (CullingVolume.Sphere.W > 0.0f)
	{
		const float Level0CellSize = SpatialHash.GetCellSize(0);
		FFootprint64 LightFootprint = SpatialHash.CalcFootprintSphere(0, CullingVolume.Sphere.Center, CullingVolume.Sphere.W + (Level0CellSize * 0.5f));

		// Diagonal length
		if ((LightFootprint.Max - LightFootprint.Min).Size() < SmallFootprintCellCountThreshold)
		{
			TestSphere(CullingVolume.Sphere, OutCellDraws, ViewGroupId, MaxNumViews, OutNumInstanceGroups);
			return;
		}
	}
	TestConvexVolume(CullingVolume.ConvexVolume, OutCellDraws, ViewGroupId, MaxNumViews, OutNumInstanceGroups);
}

void FSceneCulling::Empty()
{
	LLM_SCOPE_BYTAG(SceneCulling);

	SpatialHash.Empty();
	PackedCellChunkData.Empty();
	CellChunkIdAllocator.Empty();
	PackedCellData.Empty();
	FreeChunks.Empty();
	CellHeaders.Empty();
	CellOccupancyMask.Empty();
	BlockLevelOccupancyMask.Empty();

	CellBlockData.Empty();
	UnCullablePrimitives.Empty();
	
	UncullableItemChunksOffset = INDEX_NONE;
	UncullableNumItemChunks = 0;

	PrimitiveStates.Empty();
	CellIndexCache.Empty();
	TotalCellIndexCacheItems = 0;

	CellHeadersBuffer.Empty();
	ItemChunksBuffer.Empty();
	ItemsBuffer.Empty();
	CellBlockDataBuffer.Empty();
}


/**
 * Produce a world-space bounding sphere for an instance given local bounds and transforms.
 */
FORCEINLINE_DEBUGGABLE FVector4d TransformBounds(VectorRegister4f VecOrigin, VectorRegister4f VecExtent, const FRenderTransform& LocalToPrimitive, const FMatrix44f& PrimitiveToWorldRotation, VectorRegister4Double PrimitiveToWorldTranslationVec)
{
	// 1. Matrix Concat and bounds all in one
	const VectorRegister4f B0 = VectorLoadAligned(PrimitiveToWorldRotation.M[0]);
	const VectorRegister4f B1 = VectorLoadAligned(PrimitiveToWorldRotation.M[1]);
	const VectorRegister4f B2 = VectorLoadAligned(PrimitiveToWorldRotation.M[2]);

	VectorRegister4f NewOrigin;
	VectorRegister4f NewExtent;

	// First row of result (Matrix1[0] * Matrix2).
	{
		const VectorRegister4Float ARow = VectorLoadFloat3(&LocalToPrimitive.TransformRows[0]);
		VectorRegister4Float R0 = VectorMultiply(VectorReplicate(ARow, 0), B0);
		R0 = VectorMultiplyAdd(VectorReplicate(ARow, 1), B1, R0);
		R0 = VectorMultiplyAdd(VectorReplicate(ARow, 2), B2, R0);
		NewOrigin = VectorMultiply(VectorReplicate(VecOrigin, 0), R0);
		NewExtent = VectorAbs(VectorMultiply(VectorReplicate(VecExtent, 0), R0));
	}

	// Second row of result (Matrix1[1] * Matrix2).
	{
		const VectorRegister4Float ARow = VectorLoadFloat3(&LocalToPrimitive.TransformRows[1]);
		VectorRegister4Float R1 = VectorMultiply(VectorReplicate(ARow, 0), B0);
		R1 = VectorMultiplyAdd(VectorReplicate(ARow, 1), B1, R1);
		R1 = VectorMultiplyAdd(VectorReplicate(ARow, 2), B2, R1);
		NewOrigin = VectorMultiplyAdd(VectorReplicate(VecOrigin, 1), R1, NewOrigin);
		NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(VecExtent, 1), R1)));
	}

	// Third row of result (Matrix1[2] * Matrix2).
	{
		const VectorRegister4Float ARow = VectorLoadFloat3(&LocalToPrimitive.TransformRows[2]);
		VectorRegister4Float R2 = VectorMultiply(VectorReplicate(ARow, 0), B0);
		R2 = VectorMultiplyAdd(VectorReplicate(ARow, 1), B1, R2);
		R2 = VectorMultiplyAdd(VectorReplicate(ARow, 2), B2, R2);
		NewOrigin = VectorMultiplyAdd(VectorReplicate(VecOrigin, 2), R2, NewOrigin);
		NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(VecExtent, 2), R2)));
	}

	// Fourth row of result (Matrix1[3] * Matrix2).
	{
		const VectorRegister4Float ARow = VectorLoadFloat3(&LocalToPrimitive.Origin);
		VectorRegister4Float R3 = VectorMultiply(VectorReplicate(ARow, 0), B0);
		R3 = VectorMultiplyAdd(VectorReplicate(ARow, 1), B1, R3);
		R3 = VectorMultiplyAdd(VectorReplicate(ARow, 2), B2, R3);
		NewOrigin = VectorAdd(NewOrigin, R3);
	}

	// Offset sphere and return
	float Radius = FMath::Sqrt(VectorDot3Scalar(NewExtent, NewExtent));
	const VectorRegister4Double VecCenterOffset = VectorAdd(PrimitiveToWorldTranslationVec, NewOrigin);

	FVector4d Result;
	VectorStoreAligned(VecCenterOffset, &Result);
	Result.W = Radius;
	return Result;
}

struct FBoundsTransformerUniqueBounds
{
	FORCEINLINE_DEBUGGABLE FBoundsTransformerUniqueBounds(const FMatrix44d& PrimitiveToWorld, FPrimitiveSceneProxy* InSceneProxy)
		: SceneProxy(InSceneProxy)
	{
		PrimitiveToWorldRotation = FMatrix44f(PrimitiveToWorld.RemoveTranslation());
		PrimitiveToWorldTranslationVec = VectorLoadAligned(PrimitiveToWorld.M[3]);
	}

	FORCEINLINE_DEBUGGABLE FVector4d TransformBounds(int32 InstanceIndex, const FInstanceSceneData& PrimitiveInstance)
	{
		const FRenderBounds InstanceBounds = SceneProxy->GetInstanceLocalBounds(InstanceIndex);
		const VectorRegister4f VecMin = VectorLoadFloat3(&InstanceBounds.Min);
		const VectorRegister4f VecMax = VectorLoadFloat3(&InstanceBounds.Max);
		const VectorRegister4f Half = VectorSetFloat1(0.5f); // VectorSetFloat1() can be faster than SetFloat3(0.5, 0.5, 0.5, 0.0). Okay if 4th element is 0.5, it's multiplied by 0.0 below and we discard W anyway.
		const VectorRegister4f VecOrigin = VectorMultiply(VectorAdd(VecMax, VecMin), Half);
		const VectorRegister4f VecExtent = VectorMultiply(VectorSubtract(VecMax, VecMin), Half);

		return ::TransformBounds(VecOrigin, VecExtent, PrimitiveInstance.LocalToPrimitive, PrimitiveToWorldRotation, PrimitiveToWorldTranslationVec);
	}

	FMatrix44f PrimitiveToWorldRotation;
	VectorRegister4Double PrimitiveToWorldTranslationVec;
	FPrimitiveSceneProxy* SceneProxy;
};

struct FBoundsTransformerSharedBounds
{
	FORCEINLINE_DEBUGGABLE FBoundsTransformerSharedBounds(const FMatrix44d& PrimitiveToWorld, FPrimitiveSceneProxy* SceneProxy)
	{
		PrimitiveToWorldRotation = FMatrix44f(PrimitiveToWorld.RemoveTranslation());
		PrimitiveToWorldTranslationVec = VectorLoadAligned(PrimitiveToWorld.M[3]);

		const FRenderBounds InstanceBounds = SceneProxy->GetInstanceLocalBounds(0);
		const VectorRegister4f VecMin = VectorLoadFloat3(&InstanceBounds.Min);
		const VectorRegister4f VecMax = VectorLoadFloat3(&InstanceBounds.Max);
		const VectorRegister4f Half = VectorSetFloat1(0.5f); // VectorSetFloat1() can be faster than SetFloat3(0.5, 0.5, 0.5, 0.0). Okay if 4th element is 0.5, it's multiplied by 0.0 below and we discard W anyway.
		VecOrigin = VectorMultiply(VectorAdd(VecMax, VecMin), Half);
		VecExtent = VectorMultiply(VectorSubtract(VecMax, VecMin), Half);
	}

	FORCEINLINE_DEBUGGABLE FVector4d TransformBounds(int32 InstanceIndex, const FInstanceSceneData& PrimitiveInstance)
	{
		return ::TransformBounds(VecOrigin, VecExtent, PrimitiveInstance.LocalToPrimitive, PrimitiveToWorldRotation, PrimitiveToWorldTranslationVec);
	}

	FMatrix44f PrimitiveToWorldRotation;
	VectorRegister4Double PrimitiveToWorldTranslationVec;
	VectorRegister4f VecOrigin;
	VectorRegister4f VecExtent;
};

template <typename BoundsTransformerType>
struct FHashLocationComputerFromBounds
{
	FORCEINLINE_DEBUGGABLE FHashLocationComputerFromBounds(const TConstArrayView<FInstanceSceneData> InInstanceSceneData, FSceneCulling::FSpatialHash& InSpatialHash, const FMatrix44d& PrimitiveToWorld, FPrimitiveSceneProxy* SceneProxy)
		: BoundsTransformer(PrimitiveToWorld, SceneProxy)
		, InstanceSceneData(InInstanceSceneData)
		, SpatialHash(InSpatialHash)
	{
	}

	FORCEINLINE_DEBUGGABLE FSceneCulling::FLocation64 CalcLoc(int32 InstanceIndex)
	{
		const FInstanceSceneData& PrimitiveInstance = InstanceSceneData[InstanceIndex];
		FVector4d InstanceWorldBound = BoundsTransformer.TransformBounds(InstanceIndex, PrimitiveInstance);
		return SpatialHash.CalcLevelAndLocation(InstanceWorldBound);
	}

	BoundsTransformerType BoundsTransformer;
	const TConstArrayView<FInstanceSceneData> InstanceSceneData;
	FSceneCulling::FSpatialHash& SpatialHash;
};

#if SCENE_CULLING_USE_PRECOMPUTED

struct FHashLocationComputerPrecomputed
{
	FHashLocationComputerPrecomputed(const FSpatialHash& InSpatialHash, const FPrimitiveSceneProxy* SceneProxy)
		: InstanceSpatialHashes(SceneProxy->GetInstanceSpatialHashes())
		, SpatialHash(InSpatialHash)
	{
	}

	FLocation64 CalcLoc(int32 InstanceIndex)
	{
		FLocation64 PreCompLoc = InstanceSpatialHashes[InstanceIndex];
		if (PreCompLoc.Level < SpatialHash.GetFirstLevel())
		{
			PreCompLoc = ToLevelRelative(PreCompLoc, SpatialHash.GetFirstLevel() - PreCompLoc.Level);
		}
		return PreCompLoc;
	}

	const TConstArrayView<RenderingSpatialHash::FLocation64> InstanceSpatialHashes;
	const FSpatialHash& SpatialHash;
};

#endif

class FSceneCullingBuilder
{
public:

	// Alias a bunch of types / definitions from the instance hierarchy
	using FSpatialHash = FSceneCulling::FSpatialHash;
	using FHashElementId = FSpatialHash::FHashElementId;
	static constexpr uint32 InvalidCellFlag = FSceneCulling::InvalidCellFlag;
	static constexpr uint32 TempCellFlag = FSceneCulling::TempCellFlag;

	static constexpr int32 CellBlockSize = FSpatialHash::CellBlockSize;
	static constexpr int32 CellBlockDimLog2 = FSpatialHash::CellBlockDimLog2;

	static constexpr int32 MaxChunkSize = int32(INSTANCE_HIERARCHY_MAX_CHUNK_SIZE);
	using FPrimitiveState = FSceneCulling::FPrimitiveState;
	using FCellIndexCacheEntry = FSceneCulling::FCellIndexCacheEntry;

#if SC_ENABLE_DETAILED_BUILDER_STATS
	// Detail Stats
	struct FStats
	{
		int32 RangeCount = 0;
		int32 CompRangeCount = 0;
		int32 CopiedIdCount = 0;
		int32 VisitedIdCount = 0;
	};
	FStats Stats;
#endif 

	FSceneCullingBuilder(FSceneCulling& InSceneCulling) 
		: SceneCulling(InSceneCulling)
		, SpatialHash(InSceneCulling.SpatialHash)
		, SpatialHashMap(InSceneCulling.SpatialHash.GetHashMap())
	{
		// re-used flag array for instances that are to be removed.
		RemovedInstanceFlags.SetNum(SceneCulling.Scene.GPUScene.GetNumInstances(), false);
		//RemovedInstanceCellFlags.SetNum(SceneCulling.CellHeaders.Num(), false);
		//RemovedInstanceCellInds.Reserve(SceneCulling.CellHeaders.Num());
		// TODO: this is not needed when we also call update for added primitives correctly, remove and replace with a check!
		SceneCulling.PrimitiveStates.SetNum(SceneCulling.Scene.GetMaxPersistentPrimitiveIndex());

#if SC_ENABLE_DETAILED_LOGGING
		bIsLoggingEnabled = CVarSceneCullingLogBuild.GetValueOnRenderThread() != 0;
		check(GBuilderForLogging == nullptr);
		GBuilderForLogging = this;
		SceneTag.Appendf(TEXT("[%s]"), SceneCulling.Scene.IsEditorScene() ? TEXT("EditorScene") : TEXT(""));
		SceneTag.Append(SceneCulling.Scene.GetFullWorldName());
		AddLog(TEXT("Log-Scope-Begin"));
		LogIndent(1);
#endif
	}
	
	struct FChunkBuilder
	{
		int32 CurrentChunkCount = MaxChunkSize;
		int32 CurrentChunkId = INDEX_NONE;
		FORCEINLINE_DEBUGGABLE void EmitCurrentChunk()
		{
			if (CurrentChunkId != INDEX_NONE)
			{
				PackedChunkIds.Add(uint32(CurrentChunkId) | (uint32(CurrentChunkCount) << INSTANCE_HIERARCHY_ITEM_CHUNK_COUNT_SHIFT));
			}
			CurrentChunkId = INDEX_NONE;
		}

		FORCEINLINE_DEBUGGABLE void AddCompressedChunk(FSceneCullingBuilder& Builder, uint32 StartInstanceId)
		{
			Builder.TotalCellChunkDataCount += 1;
			// Add directly to chunk headers to not upset current chunk packing
			PackedChunkIds.Add(uint32(StartInstanceId) | INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_FLAG);

			//Builder->SceneCulling.InstanceIdToCellDataSlot[StartInstanceId] = FCellDataSlot { true, 0 };
		}
		FORCEINLINE_DEBUGGABLE void Add(FSceneCullingBuilder& Builder, uint32 InstanceId)
		{
			TArray<uint32>& PackedCellData = Builder.SceneCulling.PackedCellData;
			if (CurrentChunkCount >= MaxChunkSize)
			{
				EmitCurrentChunk();
				// Allocate a new chunk
				CurrentChunkId = Builder.AllocateChunk();
				CurrentChunkCount = 0;
				Builder.TotalCellChunkDataCount += 1;
			}
			// Emit the instance ID directly to final destination
			int32 ItemOffset = CurrentChunkId * MaxChunkSize + CurrentChunkCount++;
			PackedCellData[ItemOffset] = InstanceId;
		}

		FORCEINLINE_DEBUGGABLE void AddRange(FSceneCullingBuilder& Builder, int32 InInstanceIdOffset, int32 InInstanceIdCount)
		{
			// First add all compressible ranges.
			int32 InstanceIdOffset = InInstanceIdOffset;
			int32 InstanceIdCount = InInstanceIdCount;
			while (InstanceIdCount >= MaxChunkSize)
			{
				UPDATE_BUILDER_STAT(Builder, CompRangeCount, 1);

				AddCompressedChunk(Builder, InstanceIdOffset);
				InstanceIdOffset += MaxChunkSize;
				InstanceIdCount -= MaxChunkSize;
			}

			// add remainder individually
			for (int32 InstanceId = InstanceIdOffset; InstanceId < InstanceIdOffset + InstanceIdCount; ++InstanceId)
			{
				Add(Builder, InstanceId);
			}
		}


		FORCEINLINE_DEBUGGABLE uint32 ReallocateChunkRange(FSceneCullingBuilder &Builder, uint32 PrevItemChunksOffset, uint32 PrevNumItemChunks)
		{
			uint32 ItemChunksOffset = PrevItemChunksOffset;
			// TODO: round allocation size to POT, or some multiple to reduce reallocations & fragmentation?
			if (ItemChunksOffset != INDEX_NONE && PrevNumItemChunks != PackedChunkIds.Num())
			{
				BUILDER_LOG("Free Chunk Range: [%d,%d)", PrevItemChunksOffset, PrevItemChunksOffset + PrevNumItemChunks);
				Builder.SceneCulling.CellChunkIdAllocator.Free(PrevItemChunksOffset, PrevNumItemChunks);
				ItemChunksOffset = INDEX_NONE;
			}

			// Need a new chunk offset allocated
			if (!PackedChunkIds.IsEmpty() && ItemChunksOffset == INDEX_NONE)
			{
				ItemChunksOffset = Builder.SceneCulling.CellChunkIdAllocator.Allocate(PackedChunkIds.Num());
				BUILDER_LOG("Allocate Chunk Range: [%d,%d)", ItemChunksOffset, ItemChunksOffset + PackedChunkIds.Num());
			} 
			return ItemChunksOffset;
		}

		FORCEINLINE_DEBUGGABLE void OutputChunkIds(FSceneCullingBuilder &Builder, uint32 ItemChunksOffset)
		{
			int32 NumIds = PackedChunkIds.Num();
			// 3. Copy the list of chunk IDs
			for (uint32 Index = 0; Index < uint32(NumIds); ++Index)
			{
				uint32 ChunkIndex = ItemChunksOffset + Index;
				uint32 ChunkData = PackedChunkIds[Index];
				Builder.SceneCulling.PackedCellChunkData[ChunkIndex] = ChunkData;
				// needs variable size scatter, this is using 1:1 int to scatter the data (an int).
				Builder.ItemChunkUploader.Add(ChunkData, ChunkIndex);
				BUILDER_LOG("ItemChunkUploader %u -> %d", ChunkData, ChunkIndex);

			}
		}

		FORCEINLINE_DEBUGGABLE bool IsEmpty() const { return PackedChunkIds.IsEmpty(); }

		TArray<uint32, TInlineAllocator<32, SceneRenderingAllocator>> PackedChunkIds;
	};

	/**
	 * The temp cell is used to record information about additions / removals for a grid cell during the update.
	 * An index to a temp cell is stored in the grid instead of the index to cell data when a cell is first accessed during update.
	 * At the end of the update all temp cells are processed and then removed.
	 */
	struct FTempCell : public FChunkBuilder
	{
		struct FTempChunks
		{
			TArray<uint32, TInlineAllocator<32, SceneRenderingAllocator>> PackedIds;
		};

		int32 CellOffset = INDEX_NONE;
		int32 ItemChunksOffset = INDEX_NONE;
		int32 RemovedInstanceCount = 0;
		FCellHeader PrevCellHeader = FCellHeader { InvalidCellFlag, InvalidCellFlag };

		FORCEINLINE_DEBUGGABLE void FinalizeChunks(FSceneCullingBuilder& Builder)
		{
			// TODO: pass in and share/reuse space? 
			// Copied chunk data (only whole chunks)
			FTempChunks RetainedChunks; 

			BUILDER_LOG_SCOPE("FinalizeChunks(Index: %d RemovedInstanceCount %d):", CellOffset, RemovedInstanceCount);
#if OLA_TODO
			// Handle complete removal case efficiently
			if (RemovedInstanceCount == TotalCellInstances)
			{
				ClearCell();
			}
#endif
			// Process removals
			if (RemovedInstanceCount > 0)
			{
				check(IsValid(PrevCellHeader));
				int32 NumRemoved = 0;
				// Iterate chunks in reverse order as the last ones are the ones that are most likely to contain updated items
				for (int32 ChunkIndex = int32(PrevCellHeader.NumItemChunks) - 1; ChunkIndex >= 0; --ChunkIndex)
				{
					uint32 PackedChunkData = Builder.SceneCulling.PackedCellChunkData[PrevCellHeader.ItemChunksOffset + uint32(ChunkIndex)];
					const bool bIsCompressed = (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_FLAG) != 0u;
					const uint32 NumItems = bIsCompressed ? 64u : PackedChunkData >> INSTANCE_HIERARCHY_ITEM_CHUNK_COUNT_SHIFT;
					const bool bIsFullChunk = NumItems == 64u;
					// If nothing is to be removed from this chunk
					const bool bNoneRemoved = NumRemoved == RemovedInstanceCount;

					// We can copy the whole chunk if we're done with removals, or the current chunk has non marked for remove, and it is a full chunk
					bool bCopyExistingChunk = bNoneRemoved && bIsFullChunk;

					if (bCopyExistingChunk)
					{
						BUILDER_LOG("Chunk-Retained( NumItems: %d)", NumItems);
						AddExistingChunk(RetainedChunks, PackedChunkData);
						continue;
					}

					// 1. if it is a compressed chunk and contains any index, we may assume it is to be removed entirely.
					if (bIsCompressed)
					{
						uint32 FirstInstanceDataOffset = PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_PAYLOAD_MASK;
						if (!Builder.IsMarkedForRemove(FirstInstanceDataOffset))
						{
							BUILDER_LOG("Chunk-Retained (FirstInstanceDataOffset: %d)", FirstInstanceDataOffset);
							AddExistingChunk(RetainedChunks, PackedChunkData);
						}
						else
						{
							BUILDER_LOG("Chunk-Removed (FirstInstanceDataOffset: %d)", FirstInstanceDataOffset);
							NumRemoved += 64;
						}
					}
					// 2. elsewise, loop over the items in the chunk and copy not-deleted ones.
					else
					{
						uint32 ChunkId =  (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_ID_MASK);
						uint32 ItemDataOffset = ChunkId * INSTANCE_HIERARCHY_MAX_CHUNK_SIZE;

						UPDATE_BUILDER_STAT(Builder, VisitedIdCount, NumItems);

						// 3.1. scan chunk for deleted items
						uint64 DeletionMask = 0ull;

						for (uint32 ItemIndex = 0u; ItemIndex < NumItems; ++ItemIndex)
						{
							uint32 InstanceId = Builder.SceneCulling.PackedCellData[ItemDataOffset + ItemIndex];
							if (Builder.IsMarkedForRemove(InstanceId))
							{
								DeletionMask |= 1ull << ItemIndex;
								NumRemoved += 1;
							}
						}

						// 3.2 If none were actually deleted, then re-emit the chunk (if it is full - otherwise we may end up with a lot of half filled chunks)
						if (DeletionMask == 0ull && bIsFullChunk)
						{
							AddExistingChunk(RetainedChunks, PackedChunkData);
						}
						else
						{
							BUILDER_LOG_LIST("Processed(%d):", NumItems);
							// 3.3., otherwise, we must copy the surviving IDs
							for (uint32 ItemIndex = 0u; ItemIndex < NumItems; ++ItemIndex)
							{
								uint32 InstanceId = Builder.SceneCulling.PackedCellData[ItemDataOffset + ItemIndex];
								if ((DeletionMask & (1ull << ItemIndex)) == 0ull)
								{
									BUILDER_LOG_LIST_APPEND("K: %d", InstanceId);
									UPDATE_BUILDER_STAT(Builder, CopiedIdCount, 1);
									Add(Builder, InstanceId);
								}
								else
								{
									BUILDER_LOG_LIST_APPEND("D: %d", InstanceId);
								}
							}

							// Mark the chunk as not in use.
							Builder.SceneCulling.FreeChunk(ChunkId);
						}
					}
				}
			}
			else if (IsValid(PrevCellHeader) && PrevCellHeader.NumItemChunks > 0)
			{
				// No removals, do a fast path
				int32 LastChunkIndex = PrevCellHeader.ItemChunksOffset + PrevCellHeader.NumItemChunks - 1;
				uint32 PackedChunkData = Builder.SceneCulling.PackedCellChunkData[LastChunkIndex];
				const bool bIsLastCompressed = (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_FLAG) != 0u;
				const uint32 LastNumItems = bIsLastCompressed ? 64u : PackedChunkData >> INSTANCE_HIERARCHY_ITEM_CHUNK_COUNT_SHIFT;

				bool bLastChunkFull = LastNumItems == 64u;
				int32 NumToBulkCopy = PrevCellHeader.NumItemChunks - (bLastChunkFull ? 0 : 1);
				if (NumToBulkCopy > 0)
				{
					PackedChunkIds.Append(TConstArrayView<uint32>(&Builder.SceneCulling.PackedCellChunkData[PrevCellHeader.ItemChunksOffset], NumToBulkCopy));
				}
				// need to rebuild the last chunk to continue adding to it.
				if (!bLastChunkFull)
				{
					UPDATE_BUILDER_STAT(Builder, VisitedIdCount, LastNumItems);

					uint32 ChunkId =  (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_ID_MASK);
					uint32 ItemDataOffset = ChunkId * INSTANCE_HIERARCHY_MAX_CHUNK_SIZE;

					// 3. remove ids marked for delete
					for (uint32 ItemIndex = 0u; ItemIndex < LastNumItems; ++ItemIndex)
					{
						UPDATE_BUILDER_STAT(Builder, CopiedIdCount, 1);

						uint32 InstanceId = Builder.SceneCulling.PackedCellData[ItemDataOffset + ItemIndex];
						Add(Builder, InstanceId);
					}

					// Mark the chunk as not in use.
					Builder.SceneCulling.FreeChunk(ChunkId);
				}
			}

			// Flush the remainder into the last chunk.
			EmitCurrentChunk();

			// Insert retained chunk info first.
			PackedChunkIds.Insert(RetainedChunks.PackedIds, 0);
			int32 BlockId = Builder.SceneCulling.CellIndexToBlockId(CellOffset);
			FSpatialHash::FCellBlock& Block = Builder.SpatialHashMap.GetByElementId(BlockId).Value;

			// update the delta to track total
			Block.NumItemChunks += PackedChunkIds.Num() - (IsValid(PrevCellHeader) ? PrevCellHeader.NumItemChunks : 0);
			check(Block.NumItemChunks >= 0);

			ItemChunksOffset = ReallocateChunkRange(Builder, IsValid(PrevCellHeader) ? PrevCellHeader.ItemChunksOffset : INDEX_NONE, PrevCellHeader.NumItemChunks);
		}

		FORCEINLINE_DEBUGGABLE void AddExistingChunk(FTempChunks& RetainedChunks, uint32 ExistingPackedChunk)
		{
			// Builder.TotalCellChunkDataCount += 1;
			// Add directly to chunk headers to not upset current chunk packing
			RetainedChunks.PackedIds.Add(ExistingPackedChunk);
		}
	};

	FORCEINLINE_DEBUGGABLE FHashElementId FindOrAddBlock(const FSceneCulling::FLocation64& BlockLoc)
	{
		// Update cached block ID / loc on the assumption that when adding many instances they will often hit the same block
		if (!CachedBlockId.IsValid() || !(CachedBlockLoc == BlockLoc))
		{
			bool bAlreadyInMap = false;
			FHashElementId BlockId = SpatialHashMap.FindOrAddId(BlockLoc, FSpatialHash::FCellBlock{}, bAlreadyInMap);
			if (!bAlreadyInMap)
			{
				BUILDER_LOG("Allocated Block %d", BlockId.GetIndex());
			}
			CachedBlockId = BlockId;
			CachedBlockLoc = BlockLoc;
		}

		return CachedBlockId;
	}

	FORCEINLINE_DEBUGGABLE FTempCell& GetOrAddTempCell(const FSceneCulling::FLocation64& InstanceCellLoc)
	{
		// Address of the cell-block touched
		FSceneCulling::FLocation64 BlockLoc = ToLevelRelative(InstanceCellLoc, CellBlockDimLog2);

		// TODO: queue fine-level work by block(?) and defer, better memory coherency, probably.
		// Possibly store compressed as we would have a fair bit of empty space & bit mask + prefix sum (can we use popc efficiently nowadays on CPU)
		// Doing that would require building the block as a two-pass process such that we know all occupied cells before allocating storage.
		// Also cheaper initialization if we don't add empty cells.
		FHashElementId BlockId = FindOrAddBlock(BlockLoc);
		FSpatialHash::FCellBlock& Block = SpatialHashMap.GetByElementId(BlockId).Value;

		if (Block.GridOffset == INDEX_NONE)
		{
			// Allocate space in the array of cell headers.
			// We keep a 1:1 mapping for now, maybe compact later? Blocks in the hash map can have holes.
			int32 StartIndex = BlockId.GetIndex() * CellBlockSize;
			Block.GridOffset = StartIndex;
			
			// Ensure enough space for the new cells
			int32 NewMinSize = FMath::Max(StartIndex + CellBlockSize, SceneCulling.CellHeaders.Num());
			SceneCulling.CellHeaders.SetNumUninitialized(NewMinSize, false);
			// TODO: store the validity state in bit mask instead of with each item?
			for (int32 Index = 0; Index < CellBlockSize; ++Index)
			{
				SceneCulling.CellHeaders[StartIndex + Index].NumItemChunks = InvalidCellFlag;
			}			
			// No need to set the bits as they are maintained incrementally (or new and cleared)
			SceneCulling.CellOccupancyMask.SetNum(NewMinSize, false);

			BUILDER_LOG("Alloc Cells [%d, %d], Block %d", StartIndex, StartIndex + CellBlockSize, BlockId.GetIndex());
		}

		const FSceneCulling::FLocation8 LocalCellLoc = ToBlockLocal(InstanceCellLoc, BlockLoc);
		const int32 CellOffset = Block.GetCellGridOffset(LocalCellLoc.Coord);
		Block.CoarseCellMask |= FSpatialHash::FCellBlock::CalcCellMask(LocalCellLoc.Coord);

		return GetOrAddTempCell(CellOffset);
	}

	FORCEINLINE_DEBUGGABLE FTempCell& GetOrAddTempCell(int32 CellIndex)
	{
		FCellHeader CellHeader = SceneCulling.CellHeaders[CellIndex];

		if (!IsTempCell(CellHeader))
		{
			int32 TempCellIndex = TempCells.AddDefaulted();
			BUILDER_LOG("Alloc Temp Cell %d / %d", CellIndex, TempCellIndex);
			// Store link back to the cell in question.
			FTempCell& TempCell = TempCells[TempCellIndex];

			TempCell.CellOffset = CellIndex;
			TempCell.PrevCellHeader = CellHeader;

			// Hijack the items offset to store the index to the temp cell so we can add data there during construction.
			CellHeader.ItemChunksOffset = TempCellIndex;
			CellHeader.NumItemChunks = TempCellFlag;

			SceneCulling.CellHeaders[CellIndex] = CellHeader;

			return TempCell;
		}

		return TempCells[CellHeader.ItemChunksOffset];
	}

	FORCEINLINE_DEBUGGABLE int32 AddRange(const FSceneCulling::FLocation64& InstanceCellLoc, int32 InInstanceIdOffset, int32 InInstanceIdCount)
	{
		UPDATE_BUILDER_STAT(*this, RangeCount, 1);

		FTempCell& TempCell = GetOrAddTempCell(InstanceCellLoc);

		TempCell.AddRange(*this, InInstanceIdOffset, InInstanceIdCount);

		return TempCell.CellOffset;
	}

	FORCEINLINE_DEBUGGABLE int32 AddToCell(const FSceneCulling::FLocation64& InstanceCellLoc, int32 InstanceId)
	{
		FTempCell& TempCell = GetOrAddTempCell(InstanceCellLoc);
		TempCell.Add(*this, uint32(InstanceId));
		return TempCell.CellOffset;
	}

	template <typename HashLocationComputerType>
	FORCEINLINE_DEBUGGABLE void BuildInstanceRange(int32 InstanceDataOffset, int32 NumInstances, const TConstArrayView<FInstanceSceneData> InstanceSceneData, HashLocationComputerType HashLocationComputer, FSceneCulling::FCellIndexCacheEntry &CellIndexCacheEntry, bool bCompressRLE)
	{
		FSceneCulling::FLocation64 PrevInstanceCellLoc;
		int32 SameInstanceLocRunCount = 0;
		int32 StartRunInstanceInstanceId = -1;

		for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
		{
			const int32 InstanceId = InstanceDataOffset + InstanceIndex;

			FSceneCulling::FLocation64 InstanceCellLoc = HashLocationComputer.CalcLoc(InstanceIndex);

			bool bSameLoc = bCompressRLE && SameInstanceLocRunCount > 0 && PrevInstanceCellLoc == InstanceCellLoc;

			if (bSameLoc)
			{
				++SameInstanceLocRunCount;
			}
			else
			{
				// If we have any accumulated same-cell instances, bulk-add those
				if (SameInstanceLocRunCount > 0)
				{
					int32 CellIndex = AddRange(PrevInstanceCellLoc, StartRunInstanceInstanceId, SameInstanceLocRunCount);
					CellIndexCacheEntry.Add(CellIndex, SameInstanceLocRunCount);
				}
				// Start a new run
				PrevInstanceCellLoc = InstanceCellLoc;
				SameInstanceLocRunCount = 1;
				StartRunInstanceInstanceId = InstanceId;
			}
		}

		// Flush any outstanding ranges.
		if (SameInstanceLocRunCount > 0)
		{
			int32 CellIndex = AddRange(PrevInstanceCellLoc, StartRunInstanceInstanceId, SameInstanceLocRunCount);
			CellIndexCacheEntry.Add(CellIndex, SameInstanceLocRunCount);
		}
	}

	FORCEINLINE_DEBUGGABLE FHashElementId GetBlockId(const FSceneCulling::FLocation64& BlockLoc)
	{
		// Update cached block ID / loc on the assumption that when adding many instances they will often hit the same block
		if (!CachedBlockId.IsValid() || !(CachedBlockLoc == BlockLoc))
		{
			FHashElementId BlockId = SpatialHashMap.FindId(BlockLoc);
			check(BlockId.IsValid()); // TODO: checkslow?
			CachedBlockId = BlockId;
			CachedBlockLoc = BlockLoc;
		}

		return CachedBlockId;
	}

	FORCEINLINE_DEBUGGABLE int32 GetCellIndex(const FSceneCulling::FLocation64 &CellLoc)
	{
		if (CachedCellIdIndex == INDEX_NONE || !(CachedCellLoc == CellLoc))
		{
			// Address of the cell-block touched
			FSceneCulling::FLocation64 BlockLoc = ToLevelRelative(CellLoc, CellBlockDimLog2);

			FHashElementId BlockId = GetBlockId(BlockLoc);
			FSpatialHash::FCellBlock& Block = SpatialHashMap.GetByElementId(BlockId).Value;

			const FSceneCulling::FLocation8 LocalCellLoc = ToBlockLocal(CellLoc, BlockLoc);
			int32 CellIndex = Block.GetCellGridOffset(LocalCellLoc.Coord);

			CachedCellIdIndex = CellIndex;
			CachedCellLoc = CellLoc;
		}

		return CachedCellIdIndex;
	}

	void FinalizeTempCellsAndUncullable()
	{
		SCOPED_NAMED_EVENT(BuildHierarchy_FinalizeGrid, FColor::Emerald);
		BUILDER_LOG_SCOPE("FinalizeTempCells: %d", TempCells.Num());

		{
			SCOPED_NAMED_EVENT(BuildHierarchy_Consolidate, FColor::Emerald);
			SceneCulling.CellChunkIdAllocator.Consolidate();
		}

		CellHeaderUploader.Reserve(TempCells.Num());

		for (FTempCell& TempCell : TempCells)
		{
			TotalRemovedInstances += TempCell.RemovedInstanceCount;
			TempCell.FinalizeChunks(*this);
		}

		// Free all uncullable chunks
		for (int32 Index = 0; Index < SceneCulling.UncullableNumItemChunks; ++Index)
		{
			check(SceneCulling.UncullableItemChunksOffset != INDEX_NONE);
			uint32 PackedChunkData = SceneCulling.PackedCellChunkData[SceneCulling.UncullableItemChunksOffset + Index];
			const bool bIsCompressed = (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_FLAG) != 0u;
			if (!bIsCompressed)
			{
				uint32 ChunkId =  (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_ID_MASK);
				SceneCulling.FreeChunk(ChunkId);
			}
		}

		FChunkBuilder ChunkBuilder;
		for (FPersistentPrimitiveIndex PersistentPrimitiveIndex : SceneCulling.UnCullablePrimitives)
		{
			const FSceneCulling::FPrimitiveState &PrimitiveState = SceneCulling.PrimitiveStates[PersistentPrimitiveIndex.Index];
			check(PrimitiveState.State == FPrimitiveState::UnCullable);
			if (PrimitiveState.InstanceDataOffset != INDEX_NONE && PrimitiveState.NumInstances > 0)
			{
				ChunkBuilder.AddRange(*this, PrimitiveState.InstanceDataOffset, PrimitiveState.NumInstances);
			}
		}
		ChunkBuilder.EmitCurrentChunk();
		SceneCulling.UncullableItemChunksOffset = ChunkBuilder.ReallocateChunkRange(*this, SceneCulling.UncullableItemChunksOffset, SceneCulling.UncullableNumItemChunks);
		SceneCulling.UncullableNumItemChunks = ChunkBuilder.PackedChunkIds.Num();

		SceneCulling.PackedCellChunkData.SetNum(SceneCulling.CellChunkIdAllocator.GetMaxSize());

		if (!ChunkBuilder.IsEmpty())
		{
			ChunkBuilder.OutputChunkIds(*this, SceneCulling.UncullableItemChunksOffset);
		}

		DirtyBlocks.SetNum(SpatialHash.GetMaxNumBlocks(), false);
		NumDirtyBlocks = 0;
		for (FTempCell& TempCell : TempCells)
		{
			FCellHeader &CellHeader = SceneCulling.CellHeaders[TempCell.CellOffset];
			check(IsTempCell(CellHeader));
			// mark block as dirty
			DirtyBlocks[SceneCulling.CellIndexToBlockId(TempCell.CellOffset)] = true;
			++NumDirtyBlocks;
			if (TempCell.IsEmpty())
			{
				BUILDER_LOG("Mark Empty Cell: %d", TempCell.CellOffset);
				SceneCulling.CellOccupancyMask[TempCell.CellOffset] = false;
				CellHeader.ItemChunksOffset = -1;
				CellHeader.NumItemChunks = InvalidCellFlag;
			}
			else
			{
				SceneCulling.CellOccupancyMask[TempCell.CellOffset] = true;
				// 2. Store final offsets
				CellHeader.ItemChunksOffset = TempCell.ItemChunksOffset;
				CellHeader.NumItemChunks = TempCell.PackedChunkIds.Num();

				// 3. Copy the list of chunk IDs
				TempCell.OutputChunkIds(*this, CellHeader.ItemChunksOffset);
			}
			CellHeaderUploader.Add(CellHeader, TempCell.CellOffset);
			BUILDER_LOG("CellHeaderUploader { %u, %u} -> %d", CellHeader.ItemChunksOffset, CellHeader.NumItemChunks, TempCell.CellOffset);
		}

		TempCells.Empty();
	}

	inline int32 AllocateCacheEntry()
	{
		int32 CacheIndex = SceneCulling.CellIndexCache.EmplaceAtLowestFreeIndex(LowestFreeCacheIndex);
		check(SceneCulling.CellIndexCache.IsValidIndex(CacheIndex));
		BUILDER_LOG("AllocateCacheEntry %d", CacheIndex);
		return CacheIndex;
	}

	inline void FreeCacheEntry(int32 CacheIndex)
	{
		BUILDER_LOG("FreeCacheEntry %d", CacheIndex);
		check(SceneCulling.CellIndexCache.IsValidIndex(CacheIndex));
		SceneCulling.TotalCellIndexCacheItems -= SceneCulling.CellIndexCache[CacheIndex].Items.Num();
		SceneCulling.CellIndexCache.RemoveAt(CacheIndex);
		LowestFreeCacheIndex = 0;
	}

	inline FCellIndexCacheEntry &GetCacheEntry(int32 CacheIndex)
	{
		check(SceneCulling.CellIndexCache.IsValidIndex(CacheIndex));
		return SceneCulling.CellIndexCache[CacheIndex];
	}

	inline uint32 AllocateChunk()
	{
		uint32 ChunkId = SceneCulling.AllocateChunk();
		DirtyChunks.SetNum(FMath::Max(DirtyChunks.Num(), int32(ChunkId) + 1), false);
		// We know (as chunks are lazy allocated) that they need to be reuploaded if allocated during build, also because we double buffer (don't update in-place)
		// track chunk dirty state (could use an array and append ID instead), however, this guarantees that a reused chunk won't be uploaded twice
		DirtyChunks[ChunkId] = true;

		return ChunkId;
	}

	FPrimitiveState ComputePrimitiveState(const FPrimitiveBounds& Bounds, FPrimitiveSceneInfo* PrimitiveSceneInfo, int32 NumInstances, int32 InstanceDataOffset, FPrimitiveSceneProxy* SceneProxy, const FPrimitiveState &PrevState)
	{
		FPrimitiveState NewState;
		NewState.bDynamic = PrevState.bDynamic || SceneProxy->IsOftenMoving();
		NewState.NumInstances = NumInstances;
		NewState.InstanceDataOffset = InstanceDataOffset;
#if SCENE_CULLING_USE_PRECOMPUTED
		const bool bHasPerInstanceSpatialHash = SceneProxy->HasPerInstanceSpatialHash();
#endif

		if (SceneCulling.IsUncullable(Bounds, PrimitiveSceneInfo))
		{
			NewState.State = FPrimitiveState::UnCullable;
		}
		else if (InstanceDataOffset != INDEX_NONE)
		{
			if (NumInstances == 1)
			{
				NewState.State = FPrimitiveState::SinglePrim;
			}
			else
			{
#if SCENE_CULLING_USE_PRECOMPUTED
				// If this primitive has been discovered to be dynamic we fall over to processing it as a dynamic item even though it has precomputed data (no longer trustworthy).
				if (bHasPerInstanceSpatialHash && !NewState.bDynamic)
				{
					NewState.State = FPrimitiveState::Precomputed;
				}
				else 
#endif // SCENE_CULLING_USE_PRECOMPUTED
				{
					// Dynamic case, decide if we are going to cache or not (always for now)
					NewState.State = NewState.bDynamic ? FPrimitiveState::Dynamic : FPrimitiveState::Cached;
				}		
			}
		}
		check(NewState.NumInstances > 0 || NewState.State == FPrimitiveState::Unknown || NewState.State == FPrimitiveState::UnCullable);

		return NewState;
	}

	void AddInstances(FPersistentPrimitiveIndex PersistentId, FPrimitiveSceneInfo* PrimitiveSceneInfo)
	{
		FScene &Scene = SceneCulling.Scene;
		TArray<FPrimitiveState> &PrimitiveStates = SceneCulling.PrimitiveStates;
		const FPrimitiveState &PrevPrimitiveState = PrimitiveStates[PersistentId.Index];
		BUILDER_LOG_SCOPE("AddInstances: %d %s", PersistentId.Index, *PrevPrimitiveState.ToString());

		int32 PrimitiveIndex = Scene.GetPrimitiveIndex(PersistentId);
		check(PrimitiveIndex != INDEX_NONE);

		const int32 NumInstances = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
		const int32 InstanceDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();

		TotalAddedInstances += NumInstances;

		// leave in unknown state
		if (NumInstances == 0)
		{
			check(PrevPrimitiveState.State == FPrimitiveState::Unknown);
			return;
		}
		check(InstanceDataOffset >= 0);
		FPrimitiveSceneProxy* SceneProxy = PrimitiveSceneInfo->Proxy;
		const TConstArrayView<FInstanceSceneData> InstanceSceneData = SceneProxy->GetInstanceSceneData();
		const int32 NumInstanceData = InstanceSceneData.Num();
		check(NumInstanceData == NumInstances || (NumInstances == 1 && NumInstanceData == 0));
		const FPrimitiveBounds& Bounds = Scene.PrimitiveBounds[PrimitiveIndex];
		FPrimitiveState NewPrimitiveState = ComputePrimitiveState(Bounds, PrimitiveSceneInfo, NumInstances, InstanceDataOffset, SceneProxy, PrevPrimitiveState);

		switch(NewPrimitiveState.State)
		{
			case FPrimitiveState::SinglePrim:
			{
				FSceneCulling::FLocation64 PrimitiveCellLoc = SpatialHash.CalcLevelAndLocation(Bounds.BoxSphereBounds);
				int32 CellIndex = AddToCell(PrimitiveCellLoc, InstanceDataOffset);
				NewPrimitiveState.Payload = CellIndex;
			}
			break;
#if SCENE_CULLING_USE_PRECOMPUTED
			case FPrimitiveState::Precomputed:
			{
				BUILDER_LOG_LIST("Add CompressedInstanceSpatialHashes");

				int32 InstanceDataOffsetCurrent = InstanceDataOffset;
				for (FPrimitiveSceneProxy::FCompressedSpatialHashItem Item : SceneProxy->CompressedInstanceSpatialHashes)
				{
					int32 CellIndex = AddRange(Item.Location, InstanceDataOffsetCurrent, Item.NumInstances);
					BUILDER_LOG_LIST_APPEND("(%d, %d, %d)", CellIndex, InstanceDataOffsetCurrent, Item.NumInstances);
					InstanceDataOffsetCurrent += Item.NumInstances;
				}
#if DO_GUARD_SLOW
				NewPrimitiveState.CompressedInstanceSpatialHashes = SceneProxy->CompressedInstanceSpatialHashes;
#endif
			}
			break;
#endif // SCENE_CULLING_USE_PRECOMPUTED
			case FPrimitiveState::UnCullable:
			{
				SceneCulling.UnCullablePrimitives.Add(PersistentId);
			}
			break;
			default:
			{
				check(NewPrimitiveState.State == FPrimitiveState::Dynamic || NewPrimitiveState.State == FPrimitiveState::Cached);
				const bool bIsDynamic = NewPrimitiveState.State == FPrimitiveState::Dynamic;
					
				int32 CacheIndex = AllocateCacheEntry();
				NewPrimitiveState.Payload = CacheIndex;
				FCellIndexCacheEntry &CellIndexCacheEntry = GetCacheEntry(CacheIndex);
				const FMatrix& PrimitiveToWorld = SceneCulling.Scene.PrimitiveTransforms[PrimitiveIndex];
				const bool bHasPerInstanceLocalBounds = SceneProxy->HasPerInstanceLocalBounds();

				if (bHasPerInstanceLocalBounds)
				{
					FHashLocationComputerFromBounds<FBoundsTransformerUniqueBounds> HashLocationComputer(InstanceSceneData, SpatialHash, PrimitiveToWorld, SceneProxy);
					BuildInstanceRange(InstanceDataOffset, NumInstances, InstanceSceneData, HashLocationComputer, CellIndexCacheEntry, !bIsDynamic);
				}
				else
				{
					FHashLocationComputerFromBounds<FBoundsTransformerSharedBounds> HashLocationComputer(InstanceSceneData, SpatialHash, PrimitiveToWorld, SceneProxy);
					BuildInstanceRange(InstanceDataOffset, NumInstances, InstanceSceneData, HashLocationComputer, CellIndexCacheEntry, !bIsDynamic);
				}

				SceneCulling.TotalCellIndexCacheItems += CellIndexCacheEntry.Items.Num();
			}
		};

		check(NewPrimitiveState.NumInstances > 0 || NewPrimitiveState.State == FPrimitiveState::Unknown || NewPrimitiveState.State == FPrimitiveState::UnCullable);
		PrimitiveStates[PersistentId.Index] = NewPrimitiveState;
		BUILDER_LOG("PrimitiveState-end: %s", *NewPrimitiveState.ToString());
	}

	inline void MarkCellForRemove(int32 CellIndex, int32 NumInstances)
	{
		FTempCell &TempCell = GetOrAddTempCell(CellIndex);
		// Should not mark remove for a cell that didn't have anything in it before...
		check(IsValid(TempCell.PrevCellHeader));
		// Track total to be removed.
		TempCell.RemovedInstanceCount += NumInstances;
	}

	inline void MarkForRemove(int32 CellIndex, int32 InstanceDataOffset, int32 NumInstances)
	{
		RemovedInstanceFlags.SetRange(InstanceDataOffset, NumInstances, true);
		MarkCellForRemove(CellIndex, NumInstances);
	}

	inline void MarkInstancesForRemoval(FPersistentPrimitiveIndex PersistentPrimitiveIndex, FPrimitiveSceneInfo *PrimitiveSceneInfo)
	{
		FSceneCulling::FPrimitiveState PrimitiveState = SceneCulling.PrimitiveStates[PersistentPrimitiveIndex.Index];
		BUILDER_LOG_SCOPE("MarkInstancesForRemoval: %d %s", PersistentPrimitiveIndex.Index, *PrimitiveState.ToString());
		check(PrimitiveState.NumInstances > 0 || PrimitiveState.State == FPrimitiveState::Unknown || PrimitiveState.State == FPrimitiveState::UnCullable);

		// Clear tracked state since it is being removed (ID may now be reused so we need to prevent state from surviving).
		SceneCulling.PrimitiveStates[PersistentPrimitiveIndex.Index] = FPrimitiveState();

		// TODO: this is not needed when we also call update for added primitives correctly, remove and replace with a check!
		if (PrimitiveState.State == FPrimitiveState::Unknown)
		{
			return;
		}

		if (PrimitiveState.State == FPrimitiveState::UnCullable)
		{
			// TODO: this could be somewhat more efficient, if there are many uncullables
			SceneCulling.UnCullablePrimitives.Remove(PersistentPrimitiveIndex);
			return;
		}

		INC_DWORD_STAT_BY(STAT_SceneCulling_RemovedInstanceCount, PrimitiveState.NumInstances);

		// 1: mark all instances that need clearing out
		// TODO: need only mark one bit for compressed chunks. Track knowledge of this?
		RemovedInstanceFlags.SetRange(PrimitiveState.InstanceDataOffset, PrimitiveState.NumInstances, true);
		// Handle singular case
		if (PrimitiveState.State == FPrimitiveState::SinglePrim)
		{ 
			checkf(PrimitiveState.NumInstances == 1, TEXT("Invalid PrimitiveState.NumInstances %s"), *PrimitiveState.ToString());

			int32 CellIndex = PrimitiveState.Payload;
			MarkCellForRemove(CellIndex, PrimitiveState.NumInstances);
		}
#if SCENE_CULLING_USE_PRECOMPUTED
		else if (PrimitiveState.State == FPrimitiveState::Precomputed)
		{
			BUILDER_LOG("FPrimitiveState::Precomputed");

			// NOTE: this is only safe because the scene update waits on the task before deleting the proxies.
			FPrimitiveSceneProxy* SceneProxy = PrimitiveSceneInfo->Proxy;

			const bool bHasPerInstanceSpatialHash = SceneProxy->HasPerInstanceSpatialHash();
			check(bHasPerInstanceSpatialHash);
			BUILDER_LOG_LIST("MarkCellForRemove");
			checkSlow(SceneProxy->CompressedInstanceSpatialHashes == PrimitiveState.CompressedInstanceSpatialHashes);
			for (FPrimitiveSceneProxy::FCompressedSpatialHashItem Item : SceneProxy->CompressedInstanceSpatialHashes)
			{
				int32 CellIndex = GetCellIndex(Item.Location);
				BUILDER_LOG_LIST_APPEND("(%d, %d)", CellIndex, Item.NumInstances);
				MarkCellForRemove(CellIndex, Item.NumInstances);
			}
		}
#endif
		else if (PrimitiveState.State == FPrimitiveState::Cached || PrimitiveState.State == FPrimitiveState::Dynamic)
		{
			int32 CacheIndex = PrimitiveState.Payload;
			const FCellIndexCacheEntry &CellIndexCacheEntry = GetCacheEntry(CacheIndex);

			BUILDER_LOG_LIST("MarkForRemove(%d):", CellIndexCacheEntry.Items.Num());
			for (FCellIndexCacheEntry::FItem Item : CellIndexCacheEntry.Items)
			{
				BUILDER_LOG_LIST_APPEND("(%d, %d)", Item.CellIndex, Item.NumInstances);
				MarkCellForRemove(Item.CellIndex, Item.NumInstances);
			}

			FreeCacheEntry(CacheIndex);
		}
		else
		{
			check(false);
		}
		BUILDER_LOG("PrimitiveState-end: %s", *PrimitiveState.ToString());
	}

	template <typename HashLocationComputerType>
	inline void UpdateProcessDynamicInstances(HashLocationComputerType &HashLocationComputer, int32 InstanceDataOffset, int32 NumInstances, int32 PrevNumInstances, FSceneCulling::FCellIndexCacheEntry &CacheEntry)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
		{
			const int32 InstanceId = InstanceDataOffset + InstanceIndex;
			FSceneCulling::FLocation64 InstanceCellLoc = HashLocationComputer.CalcLoc(InstanceIndex);

			bool bNeedAdd = InstanceIndex >= PrevNumInstances;
			if (!bNeedAdd)
			{
				int32 PrevCellIndex =  CacheEntry.Items[InstanceIndex].CellIndex;
				FSceneCulling::FLocation64 PrevCellLoc = SceneCulling.GetCellLoc(PrevCellIndex);
				if (PrevCellLoc != InstanceCellLoc)
				{
					MarkForRemove(PrevCellIndex, InstanceId, 1);
					bNeedAdd = true;
				}
			}
			if (bNeedAdd)
			{
				int32 CellIndex = AddToCell(InstanceCellLoc, InstanceId);
				CacheEntry.Set(InstanceIndex, CellIndex, 1);
			}
		}
	}

	// Mark those that need for remove, queue others for add
	inline void UpdateInstances(FPersistentPrimitiveIndex PersistentPrimitiveIndex, FPrimitiveSceneInfo* PrimitiveSceneInfo)
	{
		FSceneCulling::FPrimitiveState PrevPrimitiveState = SceneCulling.PrimitiveStates[PersistentPrimitiveIndex.Index];
		BUILDER_LOG_SCOPE("UpdateInstances: %d [%s]", PersistentPrimitiveIndex.Index, *PrevPrimitiveState.ToString());
		check(PrevPrimitiveState.NumInstances > 0 || PrevPrimitiveState.State == FPrimitiveState::Unknown || PrevPrimitiveState.State == FPrimitiveState::UnCullable);

		const int32 InstanceDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
		const int32 NumInstances = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();

		TotalUpdatedInstances += NumInstances;
	 
		// Just leave on the list
		if (PrevPrimitiveState.State == FPrimitiveState::UnCullable)
		{
			PrevPrimitiveState.InstanceDataOffset = InstanceDataOffset;
			PrevPrimitiveState.NumInstances = NumInstances;
			SceneCulling.PrimitiveStates[PersistentPrimitiveIndex.Index] = PrevPrimitiveState;
			return;
		}

		int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();
		const FPrimitiveBounds& Bounds = SceneCulling.Scene.PrimitiveBounds[PrimitiveIndex];
		FPrimitiveSceneProxy* SceneProxy = PrimitiveSceneInfo->Proxy;
		const TConstArrayView<FInstanceSceneData> InstanceSceneData = SceneProxy->GetInstanceSceneData();
		const int32 NumInstanceData = InstanceSceneData.Num();
		check(NumInstanceData == NumInstances || (NumInstances == 1 && NumInstanceData == 0));
		FPrimitiveState NewPrimitiveState = ComputePrimitiveState(Bounds, PrimitiveSceneInfo, NumInstances, InstanceDataOffset, SceneProxy, PrevPrimitiveState);
		const bool bStateChanged = NewPrimitiveState.State != PrevPrimitiveState.State;
		const bool bInstanceDataOffsetChanged = PrevPrimitiveState.InstanceDataOffset != NewPrimitiveState.InstanceDataOffset;
		const FMatrix& PrimitiveToWorld = SceneCulling.Scene.PrimitiveTransforms[PrimitiveIndex];
		const bool bHasPerInstanceLocalBounds = SceneProxy->HasPerInstanceLocalBounds();

#if SCENE_CULLING_USE_PRECOMPUTED
		const bool bHasPerInstanceSpatialHash = SceneProxy->HasPerInstanceSpatialHash();
#endif
		INC_DWORD_STAT_BY(STAT_SceneCulling_UpdatedInstanceCount, NumInstances);

		// If it was previously unknown it must now be added
		bool bNeedsAdd = PrevPrimitiveState.State == FPrimitiveState::Unknown && bStateChanged;

		// Handle singular case
		if (PrevPrimitiveState.State == FPrimitiveState::SinglePrim)
		{ 
			int32 PrevCellIndex = PrevPrimitiveState.Payload;
			// If it is changed away from this state, just mark for remove and flag for re-add
			if (bStateChanged)
			{
				MarkForRemove(PrevCellIndex, PrevPrimitiveState.InstanceDataOffset, PrevPrimitiveState.NumInstances);
				bNeedsAdd = true;
			}
			else
			{
				// Otherwise compute new cell location
				FSpatialHash::FLocation64 PrimitiveCellLoc = SpatialHash.CalcLevelAndLocation(Bounds.BoxSphereBounds);
				FSpatialHash::FLocation64 PrevCellLoc = SceneCulling.GetCellLoc(PrevCellIndex);

				// It is different, so need to remove/add
				if (PrevCellLoc != PrimitiveCellLoc || bInstanceDataOffsetChanged)
				{
					// mark for removal
					MarkForRemove(PrevCellIndex, PrevPrimitiveState.InstanceDataOffset, PrevPrimitiveState.NumInstances);

					// It is still a single-instance prim (it might be an ISM which varies) re-add at once, since we have already computed the new PrimitiveCellLoc
					int32 CellIndex = AddToCell(PrimitiveCellLoc, NewPrimitiveState.InstanceDataOffset);
					NewPrimitiveState.Payload = CellIndex;
				}
				else
				{
					// retain previous state.
					NewPrimitiveState.Payload = PrevCellIndex;
				}
			}
		}
		else if (NewPrimitiveState.State == FPrimitiveState::Dynamic && !bStateChanged && !bInstanceDataOffsetChanged)
		{
			// For dynamic instance batches we process individual instances since they can then more often be retained
			// Stored in the same data structure, just guaranteed to be singular instances
			FCellIndexCacheEntry &CellIndexCacheEntry = GetCacheEntry(PrevPrimitiveState.Payload);
			// retain previous state.
			NewPrimitiveState.Payload = PrevPrimitiveState.Payload;

			// Mark overflowing ones for remove (if the number of instances shrank
			for (int32 ItemIndex = NumInstances; ItemIndex < CellIndexCacheEntry.Items.Num(); ++ItemIndex)
			{
				FCellIndexCacheEntry::FItem Item = CellIndexCacheEntry.Items[ItemIndex];
				// Assumes 1:1 between index and ID
				MarkForRemove(Item.CellIndex, PrevPrimitiveState.InstanceDataOffset + ItemIndex, Item.NumInstances);
			}
			// Maintain the total accross all entries
			SceneCulling.TotalCellIndexCacheItems -= CellIndexCacheEntry.Items.Num();
			// Resize the cache entry to fit new IDs or trim excess ones.
			CellIndexCacheEntry.Items.SetNumZeroed(NumInstances, false);
			SceneCulling.TotalCellIndexCacheItems += CellIndexCacheEntry.Items.Num();

			if (bHasPerInstanceLocalBounds)
			{
				FHashLocationComputerFromBounds<FBoundsTransformerUniqueBounds> HashLocationComputer(InstanceSceneData, SpatialHash, PrimitiveToWorld, SceneProxy);
				UpdateProcessDynamicInstances(HashLocationComputer, InstanceDataOffset, NumInstances, PrevPrimitiveState.NumInstances, CellIndexCacheEntry);
			}
			else
			{
				FHashLocationComputerFromBounds<FBoundsTransformerSharedBounds> HashLocationComputer(InstanceSceneData, SpatialHash, PrimitiveToWorld, SceneProxy);
				UpdateProcessDynamicInstances(HashLocationComputer, InstanceDataOffset, NumInstances, PrevPrimitiveState.NumInstances, CellIndexCacheEntry);
			}
		}
#if SCENE_CULLING_USE_PRECOMPUTED
		else if (PrevPrimitiveState.State == FPrimitiveState::Precomputed)
		{
			BUILDER_LOG("FPrimitiveState::Precomputed");
			// This _should_ not happen (but it does!! I.e., we have static ISMs with updating instances), these must be remove/readded to the scene			
			// Check that they are indeed not being re-added as precomputed.
			check(NewPrimitiveState.bDynamic && bStateChanged);

			RemovedInstanceFlags.SetRange(PrevPrimitiveState.InstanceDataOffset, PrevPrimitiveState.NumInstances, true);

			BUILDER_LOG_LIST("MarkCellForRemove");
			for (FPrimitiveSceneProxy::FCompressedSpatialHashItem Item : SceneProxy->CompressedInstanceSpatialHashes)
			{
				int32 CellIndex = GetCellIndex(Item.Location);
				MarkCellForRemove(CellIndex, Item.NumInstances);
				BUILDER_LOG_LIST_APPEND("(%d, %d)", CellIndex, Item.NumInstances);
			}

			bNeedsAdd = true;
		}
#endif
		// In all other cases we have something that must be removed and is in either Cached or Dynamic state which are both removed in the same way.
		else if (PrevPrimitiveState.State != FPrimitiveState::Unknown)
		{
			check(PrevPrimitiveState.State == FPrimitiveState::Cached || (PrevPrimitiveState.State == FPrimitiveState::Dynamic && (InstanceDataOffset != PrevPrimitiveState.InstanceDataOffset || bStateChanged)));
			FCellIndexCacheEntry &CellIndexCacheEntry = GetCacheEntry(PrevPrimitiveState.Payload);

			RemovedInstanceFlags.SetRange(PrevPrimitiveState.InstanceDataOffset, PrevPrimitiveState.NumInstances, true);
			for (FCellIndexCacheEntry::FItem Item : CellIndexCacheEntry.Items)
			{
				MarkCellForRemove(Item.CellIndex, Item.NumInstances);
			}
			
			// Reset the cache entry
			SceneCulling.TotalCellIndexCacheItems -= CellIndexCacheEntry.Items.Num();

			// If the primitive stays cached, we just hang on to the cache entry
			if (NewPrimitiveState.IsCachedState())
			{
				CellIndexCacheEntry.Items.Reset();
			}
			else
			{
				// clean up cache entry if we're transitioning away from cached state
				FreeCacheEntry(PrevPrimitiveState.Payload);
			}

			bNeedsAdd = true;
		}
		
		// TODO: refactor to share more code with AddInstances
		if (bNeedsAdd)
		{
			switch(NewPrimitiveState.State)
			{
				case FPrimitiveState::Unknown:
				break;
				case FPrimitiveState::SinglePrim:
				{
					FSceneCulling::FLocation64 PrimitiveCellLoc = SpatialHash.CalcLevelAndLocation(Bounds.BoxSphereBounds);
					int32 CellIndex = AddToCell(PrimitiveCellLoc, InstanceDataOffset);
					NewPrimitiveState.Payload = CellIndex;
				}
				break;
#if SCENE_CULLING_USE_PRECOMPUTED
				case FPrimitiveState::Precomputed:
				{
					// this is wrong, post-update a precomputed item should be transitioned to dynamic.
					check(false);
				}
				break;
#endif // SCENE_CULLING_USE_PRECOMPUTED
				case FPrimitiveState::UnCullable:
				{
					SceneCulling.UnCullablePrimitives.Add(PersistentPrimitiveIndex);
				}
				break;
				case FPrimitiveState::Dynamic:
				case FPrimitiveState::Cached:
				{
					check(NewPrimitiveState.IsCachedState());
					const bool bIsDynamic = NewPrimitiveState.State == FPrimitiveState::Dynamic;
					// re-use from previous state if it was also cachable
					int32 CacheIndex = PrevPrimitiveState.IsCachedState() ? PrevPrimitiveState.Payload : AllocateCacheEntry();
					NewPrimitiveState.Payload = CacheIndex;

					FCellIndexCacheEntry &CellIndexCacheEntry = GetCacheEntry(CacheIndex);

					if (bHasPerInstanceLocalBounds)
					{
						FHashLocationComputerFromBounds<FBoundsTransformerUniqueBounds> HashLocationComputer(InstanceSceneData, SpatialHash, PrimitiveToWorld, SceneProxy);
						BuildInstanceRange(InstanceDataOffset, NumInstances, InstanceSceneData, HashLocationComputer, CellIndexCacheEntry, !bIsDynamic);
					}
					else
					{
						FHashLocationComputerFromBounds<FBoundsTransformerSharedBounds> HashLocationComputer(InstanceSceneData, SpatialHash, PrimitiveToWorld, SceneProxy);
						BuildInstanceRange(InstanceDataOffset, NumInstances, InstanceSceneData, HashLocationComputer, CellIndexCacheEntry, !bIsDynamic);
					}

					SceneCulling.TotalCellIndexCacheItems += CellIndexCacheEntry.Items.Num();
				}
				break;
			};
		}

		check(NewPrimitiveState.NumInstances > 0 || NewPrimitiveState.State == FPrimitiveState::Unknown || NewPrimitiveState.State == FPrimitiveState::UnCullable);
		// Update tracked state
		SceneCulling.PrimitiveStates[PersistentPrimitiveIndex.Index] = NewPrimitiveState;

		BUILDER_LOG("PrimitiveState-end: %s", *NewPrimitiveState.ToString());
	}

	inline bool IsMarkedForRemove(uint32 InstanceId)
	{
		return RemovedInstanceFlags[InstanceId];
	}

	void PublishStats()
	{
#if SC_ENABLE_DETAILED_BUILDER_STATS
		INC_DWORD_STAT_BY(STAT_SceneCulling_RangeCount, Stats.RangeCount);
		INC_DWORD_STAT_BY(STAT_SceneCulling_CompRangeCount, Stats.CompRangeCount);
		INC_DWORD_STAT_BY(STAT_SceneCulling_CopiedIdCount, Stats.CopiedIdCount);
		INC_DWORD_STAT_BY(STAT_SceneCulling_VisitedIdCount, Stats.VisitedIdCount);
		Stats = FStats();
#endif 
		SceneCulling.PublishStats();
	}

	void UploadToGPU(FRDGBuilder& GraphBuilder)
	{
		BUILDER_LOG("UploadToGPU %d", ItemChunkUploader.GetNumScatters());

		INC_DWORD_STAT_BY(STAT_SceneCulling_UploadedChunks, ItemChunkUploader.GetNumScatters())
		INC_DWORD_STAT_BY(STAT_SceneCulling_UploadedCells, CellHeaderUploader.GetNumScatters());
		INC_DWORD_STAT_BY(STAT_SceneCulling_UploadedItems, ItemChunkDataUploader.GetNumScatters());
		INC_DWORD_STAT_BY(STAT_SceneCulling_UploadedBlocks, BlockDataUploader.GetNumScatters());

		//TODO: capture and return the (returned) registered buffers, probably need to do that elsewhere anyway?
		BlockDataUploader.ResizeAndUploadTo(GraphBuilder, SceneCulling.CellBlockDataBuffer, SceneCulling.CellBlockData.Num());
		ItemChunkDataUploader.ResizeAndUploadTo(GraphBuilder, SceneCulling.ItemsBuffer, SceneCulling.PackedCellData.Num());
		CellHeaderUploader.ResizeAndUploadTo(GraphBuilder, SceneCulling.CellHeadersBuffer, SceneCulling.CellHeaders.Num());
		ItemChunkUploader.ResizeAndUploadTo(GraphBuilder, SceneCulling.ItemChunksBuffer, SceneCulling.PackedCellChunkData.Num());
		
#if SC_ENABLE_GPU_DATA_VALIDATION
		if (CVarValidateGPUData.GetValueOnRenderThread() != 0)
		{
			SceneCulling.CellBlockDataBuffer.ValidateGPUData(GraphBuilder, TConstArrayView<FCellBlockData>(SceneCulling.CellBlockData), 
				[this](int32 Index, const FCellBlockData& HostValue, const FCellBlockData &GPUValue) 
				{
					check(GPUValue.LevelCellSize == HostValue.LevelCellSize); 
					check(GPUValue.WorldPos.GetAbsolute() == HostValue.WorldPos.GetAbsolute()); 
					check(GPUValue.Pad == HostValue.Pad); 
					check(GPUValue.Pad == 0xDeafBead); 				
				});
			SceneCulling.ItemsBuffer.ValidateGPUData(GraphBuilder, TConstArrayView<const uint32>(SceneCulling.PackedCellData), 
				[this](int32 Index, int32 HostValue, int32 GPUValue) 
				{
					check(GPUValue == HostValue); 
				});
			SceneCulling.CellHeadersBuffer.ValidateGPUData(GraphBuilder, TConstArrayView<FCellHeader>(SceneCulling.CellHeaders), 
				[this](int32 Index, const FCellHeader &HostValue, const FCellHeader &GPUValue) 
				{
					// We don't upload unreferenced cells, so they can be just garbage on the GPU.
					// In the future (if we start doing GPU-side traversal that might need to change).
					if (IsValid(HostValue))
					{
						check(GPUValue.ItemChunksOffset == HostValue.ItemChunksOffset); 
						check(GPUValue.NumItemChunks == HostValue.NumItemChunks); 
					}
				});
			SceneCulling.ItemChunksBuffer.ValidateGPUData(GraphBuilder, TConstArrayView<const uint32>(SceneCulling.PackedCellChunkData), 
				[this](int32 Index, int32 HostValue, int32 GPUValue) 
				{
					check(GPUValue == HostValue); 
				});
		}
#endif
	}

	void ProcessPostSceneUpdate(const FScenePostUpdateChangeSet& ScenePostUpdateData)
	{
		LLM_SCOPE_BYTAG(SceneCulling);
		SCOPED_NAMED_EVENT(SceneCulling_Update_Post, FColor::Emerald);
		SCOPE_CYCLE_COUNTER(STAT_SceneCulling_Update_Post);

		BUILDER_LOG_SCOPE("ProcessPostSceneUpdate: %d/%d", ScenePostUpdateData.UpdatedPrimitiveIds.Num(), ScenePostUpdateData.AddedPrimitiveIds.Num());

		SceneCulling.PrimitiveStates.SetNum(SceneCulling.Scene.GetMaxPersistentPrimitiveIndex(), false);

		//// [transform-] updated primitives instances & primitives with updated instances 
		for (int32 Index = 0; Index < ScenePostUpdateData.UpdatedPrimitiveIds.Num(); ++Index)
		{
			UpdateInstances(ScenePostUpdateData.UpdatedPrimitiveIds[Index], ScenePostUpdateData.UpdatedPrimitiveSceneInfos[Index]);
		}	

		// Next process all added and added ones.
		for (int32 Index = 0; Index < ScenePostUpdateData.AddedPrimitiveIds.Num(); ++Index)
		{
			AddInstances(ScenePostUpdateData.AddedPrimitiveIds[Index], ScenePostUpdateData.AddedPrimitiveSceneInfos[Index]);
		}

		FinalizeTempCellsAndUncullable();

		// TODO: count them while marking instead, perhaps.
		ItemChunkDataUploader.Reserve(DirtyChunks.CountSetBits());
		for (TConstSetBitIterator<SceneRenderingAllocator> BitIt(DirtyChunks); BitIt; ++BitIt)
		{
			uint32 ChunkId = BitIt.GetIndex();
			// NOTE: for the chunks we might allocate all new from a common block of memory and use an indirection on CPU to access them.
			//       Then the upload would be able to just reference that chunk.
			ItemChunkDataUploader.Add(TConstArrayView<uint32>(&SceneCulling.PackedCellData[ChunkId * INSTANCE_HIERARCHY_MAX_CHUNK_SIZE], INSTANCE_HIERARCHY_MAX_CHUNK_SIZE), ChunkId);
			BUILDER_LOG("ItemChunkDataUploader %u -> %d", ChunkId, ChunkId);
		}

		SceneCulling.BlockLevelOccupancyMask.Reset();
		SceneCulling.BlockLevelOccupancyMask.SetNum(FSpatialHash::kMaxLevel, false);

		BlockDataUploader.Reserve(NumDirtyBlocks);
		SceneCulling.CellBlockData.SetNum(SpatialHashMap.GetMaxIndex());
		for (TConstSetBitIterator<SceneRenderingAllocator> BitIt(DirtyBlocks); BitIt; ++BitIt)
		{
			int32 BlockIndex = BitIt.GetIndex();
			const auto& BlockItem = SpatialHashMap.GetByElementId(BlockIndex);

			const FSpatialHash::FCellBlock& Block = BlockItem.Value;

			FCellBlockData BlockData;
			BlockData.Pad = 0xDeafBead;
			// Remove if empty
			if (Block.NumItemChunks == 0)
			{
#if DO_CHECK
				int32 BitRangeEnd = Block.GridOffset + FSpatialHash::CellBlockSize;

				// make sure we leave the data in a valid state
				for (int32 CellIndex = Block.GridOffset; CellIndex < Block.GridOffset + FSpatialHash::CellBlockSize; ++CellIndex)
				{
					check(!SceneCulling.CellOccupancyMask[CellIndex]);
					check(!IsValid(SceneCulling.CellHeaders[CellIndex]));
				}
#endif
				SpatialHashMap.RemoveByElementId(BlockIndex);
				BlockData.WorldPos = TLargeWorldRenderPosition<float>();
				BlockData.LevelCellSize = 0.0f;
			}
			else
			{
				FSceneCulling::FLocation64 BlockLoc = BlockItem.Key;
				FVector3d BlockWorldPos = SpatialHash.CalcBlockWorldPosition(BlockLoc);

				SceneCulling.BlockLevelOccupancyMask[BlockLoc.Level] = true;
				BlockData.WorldPos = TLargeWorldRenderPosition<float>{ BlockWorldPos };
				BlockData.LevelCellSize = SpatialHash.GetCellSize(BlockLoc.Level - FSpatialHash::CellBlockDimLog2);
			}
			// Keep host/GPU in sync (mostly for validation/debugging purposes)
			if (SceneCulling.CellBlockData.IsValidIndex(BlockIndex))
			{
				SceneCulling.CellBlockData[BlockIndex] = BlockData;
				BlockDataUploader.Add(BlockData, BlockIndex);
				BUILDER_LOG("BlockDataUploader {%0.0f, %0.0f, %0.0f, %0.0f} -> %d", BlockData.WorldPos.GetAbsolute().X, BlockData.WorldPos.GetAbsolute().Y, BlockData.WorldPos.GetAbsolute().Z, BlockData.LevelCellSize, BlockIndex);
			}
			else
			{
				BUILDER_LOG("Invalid block index no upload or update: %d", BlockIndex);
			}
		}

		// Make sure to release any references to data allocated through SceneRenderingAllocator
		// TODO: maybe change to default allocator? Might make sense to keep the data allocated anyway.
		DirtyChunks.Empty();
		DirtyBlocks.Empty();
		RemovedInstanceFlags.Empty();

		CSV_CUSTOM_STAT(SceneCulling, NumUpdatedInstances,  TotalUpdatedInstances, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(SceneCulling, NumAddedInstances,  TotalAddedInstances, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(SceneCulling, NumRemovedInstances,  TotalRemovedInstances, ECsvCustomStatOp::Accumulate);
	}

	using FRemovedIntanceFlags = TBitArray<SceneRenderingAllocator>;
	FRemovedIntanceFlags RemovedInstanceFlags;

	// Used for allocating new cache slots, important to reset/maintain whenever a cache slot is freed
	int32 LowestFreeCacheIndex = 0;

	FSceneCulling::FLocation64 CachedCellLoc;
	int32 CachedCellIdIndex = -1;

	FHashElementId CachedBlockId;
	FSceneCulling::FLocation64 CachedBlockLoc;

	TArray<FTempCell, SceneRenderingAllocator> TempCells;
	int32 TotalCellChunkDataCount = 0;

	FSceneCulling& SceneCulling;
	FSceneCulling::FSpatialHash &SpatialHash;
	FSceneCulling::FSpatialHash::FSpatialHashMap &SpatialHashMap;

	// Dirty state tracking for upload
	int32 NumDirtyBlocks = 0;
	TBitArray<SceneRenderingAllocator> DirtyBlocks;
	TBitArray<SceneRenderingAllocator> DirtyChunks;

	TStructuredBufferScatterUploader<FCellBlockData> BlockDataUploader;
	TStructuredBufferScatterUploader<uint32, INSTANCE_HIERARCHY_MAX_CHUNK_SIZE> ItemChunkDataUploader;
	TStructuredBufferScatterUploader<FCellHeader> CellHeaderUploader;
	TStructuredBufferScatterUploader<uint32> ItemChunkUploader;

	int32 TotalUpdatedInstances = 0;
	int32 TotalAddedInstances = 0;
	int32 TotalRemovedInstances = 0;

#if SC_ENABLE_DETAILED_LOGGING
	bool bIsLoggingEnabled = false;
	FString SceneTag;
	int32 LogStrIndent = 0;
public:
	inline void LogIndent(int IndentDelta)
	{
		LogStrIndent += IndentDelta;
	}
	inline void AddLog(const FString &Item)
	{
		if (bIsLoggingEnabled)
		{
			FString Indent = FString::ChrN(LogStrIndent, TEXT(' '));
			UE_LOG(LogTemp, Warning, TEXT("[%s]%s%s"), *SceneTag, *Indent, *Item);
		}
	}
	inline void EndLogging()
	{
		check(GBuilderForLogging == this);
		GBuilderForLogging = nullptr;
		if (bIsLoggingEnabled)
		{
			LogIndent(-1);
			AddLog(TEXT("Log-Scope-End"));
		}
		LogStrIndent = 0;
	}
#endif
};

template <typename T>
inline  bool operator<(const RenderingSpatialHash::TLocation<T>& Lhs, const RenderingSpatialHash::TLocation<T>& Rhs)
{
	if (Lhs.Level != Rhs.Level)
	{
		return (Lhs.Level < Rhs.Level);
	}
	if (Lhs.Coord.X != Rhs.Coord.X)
	{
		return Lhs.Coord.X < Rhs.Coord.X;
	}
	if (Lhs.Coord.Y != Rhs.Coord.Y)
	{
		return Lhs.Coord.Y < Rhs.Coord.Y;
	}

	return Lhs.Coord.Z < Rhs.Coord.Z;
}

void FSceneCulling::PublishStats()
{
	SET_DWORD_STAT(STAT_SceneCulling_BlockCount, CellBlockData.Num());
	SET_DWORD_STAT(STAT_SceneCulling_CellCount, CellHeaders.Num());
	SET_DWORD_STAT(STAT_SceneCulling_ItemChunkCount, PackedCellChunkData.Num());
	SET_DWORD_STAT(STAT_SceneCulling_ItemCount, PackedCellData.Num());
	SET_DWORD_STAT(STAT_SceneCulling_IdCacheSize, TotalCellIndexCacheItems);
}

FSceneCulling::FSceneCulling(FScene& InScene)
	: Scene(InScene)
	, bIsEnabled(CVarSceneCulling.GetValueOnAnyThread() != 0)
	, SpatialHash( CVarSceneCullingMinCellSize.GetValueOnAnyThread(), CVarSceneCullingMaxCellSize.GetValueOnAnyThread(), 0.0f)
	, CellHeadersBuffer(16, TEXT("SceneCulling.CellHeaders"))
	, ItemChunksBuffer(16, TEXT("SceneCulling.ItemChunks"))
	, ItemsBuffer(16, TEXT("SceneCulling.Items"))
	, CellBlockDataBuffer(16, TEXT("SceneCulling.CellBlockData"))
{
}

FSceneCulling::FUpdater::~FUpdater()
{
	if (Implementation != nullptr)
	{
		PostUpdateTaskHandle.Wait();
		delete Implementation;
	}
}

FSceneCulling::FUpdater &FSceneCulling::BeginUpdate(FRDGBuilder& GraphBuilder)
{	
	if (Updater.Implementation != nullptr)
	{
		Updater.FinalizeAndClear(GraphBuilder, false);
	}

	check(Updater.Implementation == nullptr);

	// Note: this only works in concert with the FGlobalComponentRecreateRenderStateContext on the CVarSceneCulling callback ensuring all geometry is re-registered
	bIsEnabled = CVarSceneCulling.GetValueOnRenderThread() != 0;
	SmallFootprintCellCountThreshold = CVarSmallFootprintCellCountThreshold.GetValueOnRenderThread();
	bUseAsyncUpdate = CVarSceneCullingAsyncUpdate.GetValueOnRenderThread() != 0;
	bUseAsyncQuery = CVarSceneCullingAsyncQuery.GetValueOnRenderThread() != 0;
	SmallFootprintCellCountThreshold = CVarSmallFootprintCellCountThreshold.GetValueOnRenderThread();

	if (bIsEnabled)
	{
		Updater.Implementation = new FSceneCullingBuilder(*this);
	}
	else
	{
		Empty();
	}
	return Updater;
}

void FSceneCulling::FUpdater::OnPreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ScenePreUpdateData)
{
	LLM_SCOPE_BYTAG(SceneCulling);

	if (Implementation == nullptr)
	{
		return;
	}

	// This can be run async, but we need to either
	//  1. [if SCENE_CULLING_USE_PRECOMPUTED] retain a reference to the compressed data (could ref count this), or 
	//  2. keep the proxy alive and store the pointer to the proxy somewhere before kicking off the async task.
	// The post-callback then needs to queue its work to happen after this task.

	// Handle all removed primitives 
	// Step 1. Mark all removed instances
#if SC_ALLOW_ASYNC_TASKS
	PreUpdateTaskHandle = GraphBuilder.AddSetupTask([this, ScenePreUpdateData]()
#endif
	{
		SCOPED_NAMED_EVENT(SceneCulling_Update, FColor::Emerald);
		SCOPE_CYCLE_COUNTER(STAT_SceneCulling_Update_Pre);
		BUILDER_LOG_SCOPE("OnPreSceneUpdate: %d", ScenePreUpdateData.RemovedPrimitiveIds.Num());

		check(DebugTaskCounter++ == 0);
		for (int32 Index = 0; Index < ScenePreUpdateData.RemovedPrimitiveIds.Num(); ++Index)
		{
			Implementation->MarkInstancesForRemoval(ScenePreUpdateData.RemovedPrimitiveIds[Index], ScenePreUpdateData.RemovedPrimitiveSceneInfos[Index]);
		}
		check(--DebugTaskCounter == 0);
	}
#if SC_ALLOW_ASYNC_TASKS
	, Implementation->SceneCulling.bUseAsyncUpdate);
#endif

	// Updated primitives are not handled here (state-caching allows deferring those until the post update)
}

UE::Tasks::FTask FSceneCulling::FUpdater::GetAsyncProxyUseTaskHandle()
{
#if SCENE_CULLING_USE_PRECOMPUTED
	// Sync is only requiured if using precomputed
	return PreUpdateTaskHandle;
#else 
	return UE::Tasks::FTask();
#endif
}

void FSceneCulling::FUpdater::OnPostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ScenePostUpdateData)
{
	LLM_SCOPE_BYTAG(SceneCulling);

	if (Implementation == nullptr)
	{
		return;
	}

#if SC_ALLOW_ASYNC_TASKS
	PostUpdateTaskHandle = GraphBuilder.AddSetupTask([this, ScenePostUpdateData]()
#endif
	{
		check(DebugTaskCounter++ == 0);
		Implementation->ProcessPostSceneUpdate(ScenePostUpdateData);
		check(--DebugTaskCounter == 0);
	}
#if SC_ALLOW_ASYNC_TASKS
	, nullptr, TArray<UE::Tasks::FTask>{ PreUpdateTaskHandle }, UE::Tasks::ETaskPriority::Normal, Implementation->SceneCulling.bUseAsyncUpdate);
#endif
}

void FSceneCulling::FUpdater::FinalizeAndClear(FRDGBuilder& GraphBuilder, bool bPublishStats)
{
	LLM_SCOPE_BYTAG(SceneCulling);
	if (Implementation != nullptr)
	{
		SCOPED_NAMED_EVENT(SceneCulling_Update_FinalizeAndClear, FColor::Emerald);
		SCOPE_CYCLE_COUNTER(STAT_SceneCulling_Update_FinalizeAndClear);
		BUILDER_LOG_SCOPE("FinalizeAndClear %d", 1);

		PostUpdateTaskHandle.Wait();

		Implementation->UploadToGPU(GraphBuilder);
		
		if (bPublishStats)
		{
			Implementation->PublishStats();
		}

#if SC_ENABLE_DETAILED_LOGGING
		Implementation->EndLogging();
#endif
		delete Implementation;
		Implementation = nullptr;
	}
}

void FSceneCulling::EndUpdate(FRDGBuilder& GraphBuilder, bool bPublishStats)
{
	if (bIsEnabled)
	{
		Updater.FinalizeAndClear(GraphBuilder, bPublishStats);
#if DO_CHECK
		ValidateAllInstanceAllocations();
#endif
	}
}

void FSceneCulling::ValidateAllInstanceAllocations()
{
#if DO_CHECK
	if (CVarValidateAllInstanceAllocations.GetValueOnRenderThread() != 0)
	{
		for (TConstSetBitIterator<> BitIt(CellOccupancyMask); BitIt; ++BitIt)
		{
			uint32 CellId = uint32(BitIt.GetIndex());
			FCellHeader CellHeader = CellHeaders[CellId];
			check(IsValid(CellHeader));

			for (uint32 ChunkIndex = 0; ChunkIndex < CellHeader.NumItemChunks; ++ChunkIndex)
			{
				uint32 PackedChunkData = PackedCellChunkData[CellHeader.ItemChunksOffset + ChunkIndex];
				const bool bIsCompressed = (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_FLAG) != 0u;
				const uint32 NumItems = bIsCompressed ? 64u : PackedChunkData >> INSTANCE_HIERARCHY_ITEM_CHUNK_COUNT_SHIFT;
				const bool bIsFullChunk = NumItems == 64u;

				// 1. if it is a compressed chunk and contains any index, we may assume it is to be removed entirely.
				if (bIsCompressed)
				{
					uint32 InstanceDataOffset = PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_COMPRESSED_PAYLOAD_MASK;
					for (uint32 ItemIndex = 0u; ItemIndex < 64u; ++ItemIndex)
					{
						uint32 InstanceId = InstanceDataOffset + ItemIndex;
						check(!Scene.GPUScene.GetInstanceSceneDataAllocator().IsFree(InstanceId));
					}
				}
				else
				{
					uint32 ChunkId =  (PackedChunkData & INSTANCE_HIERARCHY_ITEM_CHUNK_ID_MASK);
					uint32 ItemDataOffset = ChunkId * INSTANCE_HIERARCHY_MAX_CHUNK_SIZE;

					// 3.1. scan chunk for deleted items
					uint64 DeletionMask = 0ull;

					for (uint32 ItemIndex = 0u; ItemIndex < NumItems; ++ItemIndex)
					{
						uint32 InstanceId = PackedCellData[ItemDataOffset + ItemIndex];
						check(!Scene.GPUScene.GetInstanceSceneDataAllocator().IsFree(InstanceId));
					}
				}
			}
		}
	}
#endif
}

UE::Tasks::FTask FSceneCulling::GetUpdateTaskHandle() const
{
	return Updater.Implementation != nullptr ? Updater.PostUpdateTaskHandle : UE::Tasks::FTask();
}

uint32 FSceneCulling::AllocateChunk()
{
	if (!FreeChunks.IsEmpty())
	{
		return FreeChunks.Pop(false);
	}

	uint32 NewChunkId = PackedCellData.Num() / uint32(INSTANCE_HIERARCHY_MAX_CHUNK_SIZE);
	PackedCellData.SetNumUninitialized(PackedCellData.Num() + int32(INSTANCE_HIERARCHY_MAX_CHUNK_SIZE), false);
	return NewChunkId;
}

void FSceneCulling::FreeChunk(uint32 ChunkId)
{
	FreeChunks.Add(ChunkId);
}

inline int32 FSceneCulling::CellIndexToBlockId(int32 CellIndex)
{
	return CellIndex / FSpatialHash::CellBlockSize;
}

inline FSceneCulling::FLocation64 FSceneCulling::GetCellLoc(int32 CellIndex)
{
	FLocation64 Result;
	Result.Level = INT32_MIN;
	if (CellHeaders.IsValidIndex(CellIndex) && IsValid(CellHeaders[CellIndex]))
	{
		int32 BlockId = CellIndexToBlockId(CellIndex);
		Result = ToLevelRelative(SpatialHash.GetBlockLocById(BlockId), -FSpatialHash::CellBlockDimLog2);
		// Offset by local coord.
		Result.Coord.X += CellIndex & FSpatialHash::LocalCellCoordMask;
		Result.Coord.Y += (CellIndex >> FSpatialHash::CellBlockDimLog2) & FSpatialHash::LocalCellCoordMask;
		Result.Coord.Z += (CellIndex >> (2u * FSpatialHash::CellBlockDimLog2)) & FSpatialHash::LocalCellCoordMask;
	}
	return Result;
}

inline bool FSceneCulling::IsUncullable(const FPrimitiveBounds& Bounds, FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// a primitive cannot be culled if it is too large, OR if it is so far away that it cannot be represented in the precision used in the hierarhcy
	return Bounds.BoxSphereBounds.SphereRadius * 2.0 >= SpatialHash.GetLastLevelCellSize()
		||  Bounds.BoxSphereBounds.Origin.SquaredLength() >= FMath::Square(SpatialHash.GetMaxCullingDistance() - Bounds.BoxSphereBounds.SphereRadius);
}


#if SC_ENABLE_DETAILED_LOGGING

FIHLoggerScopeHelper::FIHLoggerScopeHelper()
{
	if (GBuilderForLogging != nullptr)
	{
		GBuilderForLogging->LogIndent(1);
	}
}

FIHLoggerScopeHelper::~FIHLoggerScopeHelper()
{
	if (GBuilderForLogging != nullptr)
	{
		GBuilderForLogging->LogIndent(-1);
	}
}

FIHLoggerListScopeHelper::FIHLoggerListScopeHelper(const FString &InListName) 
	: bEnabled(GBuilderForLogging != nullptr)
	, LogStr(InListName) 
{
	if (bEnabled)
	{
		LogStr.Append(TEXT(": "));
	}
}

FIHLoggerListScopeHelper::~FIHLoggerListScopeHelper()
{
	if (bEnabled)
	{
		GBuilderForLogging->AddLog(LogStr);
	}
}

#endif
