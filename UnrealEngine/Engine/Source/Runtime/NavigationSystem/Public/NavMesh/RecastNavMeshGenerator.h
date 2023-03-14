// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "AI/Navigation/NavigationTypes.h"
#include "EngineDefines.h"
#include "AI/NavigationModifier.h"
#include "NavigationOctree.h"
#include "NavMesh/RecastNavMesh.h"
#include "Async/AsyncWork.h"
#include "UObject/GCObject.h"
#include "AI/NavDataGenerator.h"
#include "NavMesh/RecastHelpers.h"
#include "NavDebugTypes.h"

#if WITH_RECAST

#include "Recast/Recast.h"
#include "Detour/DetourNavMesh.h"

#if RECAST_INTERNAL_DEBUG_DATA
#include "NavMesh/RecastInternalDebugData.h"
#endif

class UBodySetup;
class ARecastNavMesh;
class FNavigationOctree;
class FNavMeshBuildContext;
class FRecastNavMeshGenerator;
struct FTileRasterizationContext;
struct BuildContext;
struct FNavigationRelevantData;
struct dtTileCacheLayer;
struct FKAggregateGeom;
struct FTileCacheCompressor;
struct FTileCacheAllocator;
struct FTileGenerationContext;
class dtNavMesh;
class FNavRegenTimeSliceManager;
class UNavigationSystemV1;

#define MAX_VERTS_PER_POLY	6

PRAGMA_DISABLE_DEPRECATION_WARNINGS
struct FRecastBuildConfig : public rcConfig
{
	/** controls whether voxel filterring will be applied (via FRecastTileGenerator::ApplyVoxelFilter) */
	uint32 bPerformVoxelFiltering:1;
	/** generate detailed mesh (additional tessellation to match heights of geometry) */
	uint32 bGenerateDetailedMesh:1;
	/** generate BV tree (space partitioning for queries) */
	uint32 bGenerateBVTree:1;
	/** if set, mark areas with insufficient free height instead of cutting them out  */
	uint32 bMarkLowHeightAreas : 1;
	/** Expand the top of the area nav modifier's bounds by one cell height when applying to the navmesh.
		If unset, navmesh on top of surfaces might not be marked by marking bounds flush with top surfaces (since navmesh is generated slightly above collision, depending on cell height). */
	uint32 bUseExtraTopCellWhenMarkingAreas : 1;
	/** if set, only single low height span will be allowed under valid one */
	uint32 bFilterLowSpanSequences : 1;
	/** if set, only low height spans with corresponding area modifier will be stored in tile cache (reduces memory, can't modify without full tile rebuild) */
	uint32 bFilterLowSpanFromTileCache : 1;

	/** region partitioning method used by tile cache */
	int32 TileCachePartitionType;
	/** chunk size for ChunkyMonotone partitioning */
	int32 TileCacheChunkSize;

	UE_DEPRECATED(4.24, "FRecastBuildConfig.PolyMaxHeight has been deprecated as it has no use")
	int32 PolyMaxHeight;
	/** indicates what's the limit of navmesh polygons per tile. This value is calculated from other
	 *	factors - DO NOT SET IT TO ARBITRARY VALUE */
	int32 MaxPolysPerTile;

	/** Actual agent height (in uu)*/
	float AgentHeight;
	/** Actual agent climb (in uu)*/
	float AgentMaxClimb;
	/** Actual agent radius (in uu)*/
	float AgentRadius;
	/** Agent index for filtering links */
	int32 AgentIndex;

	FRecastBuildConfig()
	{
		Reset();
	}

	void Reset()
	{
		FMemory::Memzero(*this);
		bPerformVoxelFiltering = true;
		bGenerateDetailedMesh = true;
		bGenerateBVTree = true;
		bMarkLowHeightAreas = false;
		bUseExtraTopCellWhenMarkingAreas = true;
		bFilterLowSpanSequences = false;
		bFilterLowSpanFromTileCache = false;
		// Still initializing, even though the property is deprecated, to avoid static analysis warnings
		PolyMaxHeight = 10;
		MaxPolysPerTile = -1;
		AgentIndex = 0;
	}
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

struct FRecastVoxelCache
{
	struct FTileInfo
	{
		int16 TileX;
		int16 TileY;
		int32 NumSpans;
		FTileInfo* NextTile;
		rcSpanCache* SpanData;
	};

	int32 NumTiles;

	/** tile info */
	FTileInfo* Tiles;

	FRecastVoxelCache() {}
	FRecastVoxelCache(const uint8* Memory);
};

struct FRecastGeometryCache
{
	struct FHeader
	{
		FNavigationRelevantData::FCollisionDataHeader Validation;
		
		int32 NumVerts;
		int32 NumFaces;
		struct FWalkableSlopeOverride SlopeOverride;

		static uint32 StaticMagicNumber;
	};

	FHeader Header;

	/** recast coords of vertices (size: NumVerts * 3) */
	FVector::FReal* Verts;

	/** vert indices for triangles (size: NumFaces * 3) */
	int32* Indices;

	FRecastGeometryCache() {}
	FRecastGeometryCache(const uint8* Memory);

	static bool IsValid(const uint8* Memory, int32 MemorySize);
};

struct FRecastRawGeometryElement
{
	// Instance geometry
	TArray<FVector::FReal>		GeomCoords;
	TArray<int32>				GeomIndices;
	
	// Per instance transformations in unreal coords
	// When empty geometry is in world space
	TArray<FTransform>	PerInstanceTransform;

	rcRasterizationFlags RasterizationFlags;
};

struct FRecastAreaNavModifierElement
{
	TArray<FAreaNavModifier> Areas;
	
	// Per instance transformations in unreal coords
	// When empty areas are in world space
	TArray<FTransform>	PerInstanceTransform;

	bool bMaskFillCollisionUnderneathForNavmesh = false;
};

struct FRcTileBox
{
	int32 XMin, XMax, YMin, YMax;

	FRcTileBox(const FBox& UnrealBounds, const FVector& RcNavMeshOrigin, const FVector::FReal TileSizeInWorldUnits)
	{
		check(TileSizeInWorldUnits > 0);

		auto CalcMaxCoordExclusive = [](const FVector::FReal MaxAsFloat, const int32 MinCoord) -> int32
		{
			FVector::FReal UnusedIntPart;
			// If MaxCoord falls exactly on the boundary of a tile
			if (FMath::Modf(MaxAsFloat, &UnusedIntPart) == 0)
			{
				// Return the lower tile
				return FMath::Max(FMath::FloorToInt(MaxAsFloat) - 1, MinCoord);
			}
			// Otherwise use default behaviour
			return FMath::FloorToInt(MaxAsFloat);
		};

		const FBox RcAreaBounds = Unreal2RecastBox(UnrealBounds);
		XMin = FMath::FloorToInt((RcAreaBounds.Min.X - RcNavMeshOrigin.X) / TileSizeInWorldUnits);
		XMax = CalcMaxCoordExclusive((RcAreaBounds.Max.X - RcNavMeshOrigin.X) / TileSizeInWorldUnits, XMin);
		YMin = FMath::FloorToInt((RcAreaBounds.Min.Z - RcNavMeshOrigin.Z) / TileSizeInWorldUnits);
		YMax = CalcMaxCoordExclusive((RcAreaBounds.Max.Z - RcNavMeshOrigin.Z) / TileSizeInWorldUnits, YMin);
	}

	FORCEINLINE bool Contains(const FIntPoint& Point) const
	{
		return Point.X >= XMin && Point.X <= XMax
			&& Point.Y >= YMin && Point.Y <= YMax;
	}
};

/**
 * TIME SLICING 
 * The general idea is that any function that handles time slicing internally will be named XXXXTimeSliced
 * and returns a ETimeSliceWorkResult. These functions also call TestTimeSliceFinished() internally when required,
 * IsTimeSliceFinishedCached() can be called externally after they have finished. Non time sliced functions are 
 * managed externally and the calling function should call TestTimeSliceFinished() when necessary.
 */

/** Return state of calling time sliced functions */
enum class ETimeSliceWorkResult : uint8
{
	Failed,
	Succeeded,
	CallAgainNextTimeSlice, /** time slice is finished this frame but we need to call this functionality again next frame */
};

/** State representing which area of GenerateCompressedLayersTimeSliced() we are processing */
enum class EGenerateCompressedLayersTimeSliced : uint8
{
	Invalid,
	Init,
	CreateHeightField,
	RasterizeTriangles,
	EmptyLayers,
	VoxelFilter,
	RecastFilter,
	CompactHeightField,
	ErodeWalkable,
	BuildLayers,
	BuildTileCache,
};

enum class ERasterizeGeomRecastTimeSlicedState : uint8 
{
	MarkWalkableTriangles,
	RasterizeTriangles,
};

enum class ERasterizeGeomTimeSlicedState : uint8
{
	RasterizeGeometryTransformCoords,
	RasterizeGeometryRecast,
};

enum class EGenerateRecastFilterTimeSlicedState : uint8
{
	FilterLowHangingWalkableObstacles,
	FilterLedgeSpans,
	FilterWalkableLowHeightSpans,
};

enum class EDoWorkTimeSlicedState : uint8
{
	Invalid,
	GatherGeometryFromSources,
	GenerateTile,
};

enum class EGenerateTileTimeSlicedState : uint8
{
	Invalid,
	GenerateCompressedLayers,
	GenerateNavigationData,
};

enum class EGenerateNavDataTimeSlicedState : uint8
{
	Invalid,
	Init,
	GenerateLayers,
};

struct FRecastTileTimeSliceSettings
{
	int32 FilterLedgeSpansMaxYProcess = 13;
};

/**
 * Class handling generation of a single tile, caching data that can speed up subsequent tile generations
 */
class NAVIGATIONSYSTEM_API FRecastTileGenerator : public FNoncopyable, public FGCObject
{
	friend FRecastNavMeshGenerator;

public:
	FRecastTileGenerator(FRecastNavMeshGenerator& ParentGenerator, const FIntPoint& Location);
	virtual ~FRecastTileGenerator();
		
	/** Does the work involved with regenerating this tile using time slicing.
	 *  The return value determines the result of the time slicing
	 */
	ETimeSliceWorkResult DoWorkTimeSliced();
	/** Does the work involved with regenerating this tile */
	bool DoWork();

	FORCEINLINE int32 GetTileX() const { return TileX; }
	FORCEINLINE int32 GetTileY() const { return TileY; }
	FORCEINLINE const FBox& GetTileBB() const { return TileBB; }
	/** Whether specified layer was updated */
	FORCEINLINE bool IsLayerChanged(int32 LayerIdx) const { return DirtyLayers[LayerIdx]; }
	FORCEINLINE const TBitArray<>& GetDirtyLayersMask() const { return DirtyLayers; }
	/** Whether tile data was fully regenerated */
	FORCEINLINE bool IsFullyRegenerated() const { return bRegenerateCompressedLayers; }
	/** Whether tile task has anything to build */
	bool HasDataToBuild() const;

	const TArray<FNavMeshTileData>& GetCompressedLayers() const { return CompressedLayers; }

	static FBox CalculateTileBounds(int32 X, int32 Y, const FVector& RcNavMeshOrigin, const FBox& TotalNavBounds, FVector::FReal TileSizeInWorldUnits);

protected:
	// to be used solely by FRecastNavMeshGenerator
	TArray<FNavMeshTileData>& GetNavigationData() { return NavigationData; }
	
public:
	uint32 GetUsedMemCount() const;

	// Memory amount used to construct generator 
	uint32 UsedMemoryOnStartup;

	// FGCObject begin
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// FGCObject end

#if RECAST_INTERNAL_DEBUG_DATA
	const FRecastInternalDebugData& GetDebugData() const { return DebugData; }
	FRecastInternalDebugData& GetMutableDebugData() { return DebugData; }
#endif
		
	typedef TArray<int32, TInlineAllocator<4096>> TInlineMaskArray;

protected:
	/** Does the actual TimeSliced tile generation. 
	 *	@note always trigger tile generation only via DoWorkTimeSliced(). This is a worker function
	 *  The return value determines the result of the time slicing
	 *	@return Suceeded if new tile navigation data has been generated and is ready to be added to navmesh instance,
	 *	@return Failed if failed or no need to generate (still valid).
	 *  @return CallAgainNextTimeSlice, time slice is finished this frame but we need to call this function again next frame
	 */
	ETimeSliceWorkResult GenerateTileTimeSliced();
	/** Does the actual tile generation.
	 *	@note always trigger tile generation only via DoWorkTime(). This is a worker function
	 *  The return value determines the result of the time slicing
	 *	@return true if new tile navigation data has been generated and is ready to be added to navmesh instance,
	 *	@return false if failed or no need to generate (still valid).
	 */
	bool GenerateTile();

	void Setup(const FRecastNavMeshGenerator& ParentGenerator, const TArray<FBox>& DirtyAreas);
	
	/** Gather geometry */
	virtual void GatherGeometry(const FRecastNavMeshGenerator& ParentGenerator, bool bGeometryChanged);
	/** Gather geometry sources to be processed later by the GatherGeometryFromSources */
	virtual void PrepareGeometrySources(const FRecastNavMeshGenerator& ParentGenerator, bool bGeometryChanged);
	/** Gather geometry from the prefetched sources */
	void GatherGeometryFromSources();
	/** Gather geometry from the prefetched sources time sliced version */
	ETimeSliceWorkResult GatherGeometryFromSourcesTimeSliced();
	/** Gather geometry from a specified Navigation Data */
	void GatherNavigationDataGeometry(const TSharedRef<FNavigationRelevantData, ESPMode::ThreadSafe>& ElementData, UNavigationSystemV1& NavSys, const FNavDataConfig& OwnerNavDataConfig, bool bGeometryChanged);

	/** Start functions used by GenerateCompressedLayersTimeSliced / GenerateCompressedLayers */
	bool CreateHeightField(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	ETimeSliceWorkResult RasterizeTrianglesTimeSliced(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	void RasterizeTriangles(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	ETimeSliceWorkResult RasterizeGeometryRecastTimeSliced(FNavMeshBuildContext& BuildContext, const TArray<FVector::FReal>& Coords, const TArray<int32>& Indices, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	UE_DEPRECATED(5.0, "Call the version of this function where Coords are now a TArray of FReals!")
	ETimeSliceWorkResult RasterizeGeometryRecastTimeSliced(FNavMeshBuildContext& BuildContext, const TArray<float>& Coords, const TArray<int32>& Indices, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	void RasterizeGeometryRecast(FNavMeshBuildContext& BuildContext, const TArray<FVector::FReal>& Coords, const TArray<int32>& Indices, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	UE_DEPRECATED(5.0, "Call the version of this function where Coords are now a TArray of FReals!")
	void RasterizeGeometryRecast(FNavMeshBuildContext& BuildContext, const TArray<float>& Coords, const TArray<int32>& Indices, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	void RasterizeGeometryTransformCoords(const TArray<FVector::FReal>& Coords, const FTransform& LocalToWorld);
	UE_DEPRECATED(5.0, "Call the version of this function where Coords are now a TArray of FReals!")
	void RasterizeGeometryTransformCoords(const TArray<float>& Coords, const FTransform& LocalToWorld);
	ETimeSliceWorkResult RasterizeGeometryTimeSliced(FNavMeshBuildContext& BuildContext, const TArray<FVector::FReal>& Coords, const TArray<int32>& Indices, const FTransform& LocalToWorld, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	UE_DEPRECATED(5.0, "Call the version of this function where Coords are now a TArray of FReals!")
	ETimeSliceWorkResult RasterizeGeometryTimeSliced(FNavMeshBuildContext& BuildContext, const TArray<float>& Coords, const TArray<int32>& Indices, const FTransform& LocalToWorld, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	void RasterizeGeometry(FNavMeshBuildContext& BuildContext, const TArray<FVector::FReal>& Coords, const TArray<int32>& Indices, const FTransform& LocalToWorld, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	UE_DEPRECATED(5.0, "Call the version of this function where Coords are now a TArray of FReals!")
	void RasterizeGeometry(FNavMeshBuildContext& BuildContext, const TArray<float>& Coords, const TArray<int32>& Indices, const FTransform& LocalToWorld, const rcRasterizationFlags RasterizationFlags, FTileRasterizationContext& RasterContext);
	void GenerateRecastFilter(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	ETimeSliceWorkResult GenerateRecastFilterTimeSliced(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	bool BuildCompactHeightField(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	bool RecastErodeWalkable(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	bool RecastBuildLayers(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	bool RecastBuildTileCache(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	/** End functions used by GenerateCompressedLayersTimeSliced / GenerateCompressedLayers */

	/** builds CompressedLayers array (geometry + modifiers) time sliced*/
	virtual ETimeSliceWorkResult GenerateCompressedLayersTimeSliced(FNavMeshBuildContext& BuildContext);
	/** builds CompressedLayers array (geometry + modifiers) */
	virtual bool GenerateCompressedLayers(FNavMeshBuildContext& BuildContext);

	/** Builds a navigation data layer */
	bool GenerateNavigationDataLayer(FNavMeshBuildContext& BuildContext, FTileCacheCompressor& TileCompressor, FTileCacheAllocator& GenNavAllocator, FTileGenerationContext& GenerationContext, int32 LayerIdx);

	/** builds NavigationData array (layers + obstacles) time sliced */
	ETimeSliceWorkResult GenerateNavigationDataTimeSliced(FNavMeshBuildContext& BuildContext);

	/** builds NavigationData array (layers + obstacles) */
	bool GenerateNavigationData(FNavMeshBuildContext& BuildContext);

	virtual void ApplyVoxelFilter(struct rcHeightfield* SolidHF, FVector::FReal WalkableRadius);

	/** Compute rasterization mask */
	void InitRasterizationMaskArray(const rcHeightfield* SolidHF, TInlineMaskArray& OutRasterizationMasks);
	void ComputeRasterizationMasks(FNavMeshBuildContext& BuildContext, FTileRasterizationContext& RasterContext);
	void MarkRasterizationMask(rcContext* /*BuildContext*/, rcHeightfield* SolidHF,
		const FAreaNavModifier& Modifier, const FTransform& LocalToWorld, const int32 Mask, TInlineMaskArray& OutMaskArray);

	/** apply areas from DynamicAreas to layer */
	void MarkDynamicAreas(dtTileCacheLayer& Layer);
	void MarkDynamicArea(const FAreaNavModifier& Modifier, const FTransform& LocalToWorld, dtTileCacheLayer& Layer);
	void MarkDynamicArea(const FAreaNavModifier& Modifier, const FTransform& LocalToWorld, dtTileCacheLayer& Layer, const int32 AreaId, const int32* ReplaceAreaIdPtr);

	void AppendModifier(const FCompositeNavModifier& Modifier, const FNavDataPerInstanceTransformDelegate& InTransformsDelegate);
	/** Appends specified geometry to tile's geometry */
	void ValidateAndAppendGeometry(const TSharedRef<FNavigationRelevantData, ESPMode::ThreadSafe>& ElementData, const FCompositeNavModifier& InModifier);
	void AppendGeometry(const FNavigationRelevantData& DataRef, const FCompositeNavModifier& InModifier, const FNavDataPerInstanceTransformDelegate& InTransformsDelegate);
	void AppendVoxels(rcSpanCache* SpanData, int32 NumSpans);
	
	/** prepare voxel cache from collision data */
	void PrepareVoxelCache(const TNavStatArray<uint8>& RawCollisionCache, const FCompositeNavModifier& InModifier, TNavStatArray<rcSpanCache>& SpanData);
	bool HasVoxelCache(const TNavStatArray<uint8>& RawVoxelCache, rcSpanCache*& CachedVoxels, int32& NumCachedVoxels) const;
	void AddVoxelCache(TNavStatArray<uint8>& RawVoxelCache, const rcSpanCache* CachedVoxels, const int32 NumCachedVoxels) const;

	void DumpAsyncData();

#if RECAST_INTERNAL_DEBUG_DATA
	bool IsTileDebugActive();
#endif

protected:
	uint32 bRegenerateCompressedLayers : 1;
	uint32 bFullyEncapsulatedByInclusionBounds : 1;
	uint32 bUpdateGeometry : 1;
	uint32 bHasLowAreaModifiers : 1;

	/** Start time slicing variables */
	ERasterizeGeomRecastTimeSlicedState RasterizeGeomRecastState;
	ERasterizeGeomTimeSlicedState RasterizeGeomState;
	EGenerateRecastFilterTimeSlicedState GenerateRecastFilterState;
	int32 GenRecastFilterLedgeSpansYStart;
	EDoWorkTimeSlicedState DoWorkTimeSlicedState;
	EGenerateTileTimeSlicedState GenerateTileTimeSlicedState;

	EGenerateNavDataTimeSlicedState GenerateNavDataTimeSlicedState;
	int32 GenNavDataLayerTimeSlicedIdx;
	EGenerateCompressedLayersTimeSliced GenCompressedLayersTimeSlicedState;
	int32 RasterizeTrianglesTimeSlicedRawGeomIdx;
	int32 RasterizeTrianglesTimeSlicedInstTransformIdx;
	TNavStatArray<uint8> RasterizeGeomRecastTriAreas;
	const FNavRegenTimeSliceManager* TimeSliceManager;

	TUniquePtr<struct FTileCacheAllocator> GenNavDataTimeSlicedAllocator;
	TUniquePtr<struct FTileGenerationContext> GenNavDataTimeSlicedGenerationContext;
	TUniquePtr<struct FTileRasterizationContext> GenCompressedlayersTimeSlicedRasterContext;

	FRecastTileTimeSliceSettings TileTimeSliceSettings;
	/** End time slicing variables */

	int32 TileX;
	int32 TileY;
	uint32 Version;
	/** Tile's bounding box, Unreal coords */
	FBox TileBB;

	/** Tile's bounding box expanded by Agent's radius *2 + CellSize, Unreal coords */
	FBox TileBBExpandedForAgent;
	
	/** Layers dirty flags */
	TBitArray<> DirtyLayers;
	
	/** Parameters defining navmesh tiles */
	FRecastBuildConfig TileConfig;

	/** Bounding geometry definition. */
	TNavStatArray<FBox> InclusionBounds;

	/** Additional config */
	FRecastNavMeshCachedData AdditionalCachedData;

	// generated tile data
	TArray<FNavMeshTileData> CompressedLayers;
	TArray<FNavMeshTileData> NavigationData;

	/** Result of calling RasterizeGeometryInitVars() */
	TArray<FVector::FReal> RasterizeGeometryWorldRecastCoords;
	
	// tile's geometry: without voxel cache
	TArray<FRecastRawGeometryElement> RawGeometry;
	// areas used for creating navigation data: obstacles
	TArray<FRecastAreaNavModifierElement> Modifiers;
	// navigation links
	TArray<FSimpleLinkNavModifier> OffmeshLinks;

	TWeakPtr<FNavDataGenerator, ESPMode::ThreadSafe> ParentGeneratorWeakPtr;

	TNavStatArray<TSharedRef<FNavigationRelevantData, ESPMode::ThreadSafe> > NavigationRelevantData;
	TWeakObjectPtr<UNavigationSystemV1> NavSystem; 
	FNavDataConfig NavDataConfig;

	FRecastNavMeshTileGenerationDebug TileDebugSettings;

#if RECAST_INTERNAL_DEBUG_DATA
	FRecastInternalDebugData DebugData;
#endif
};

struct NAVIGATIONSYSTEM_API FRecastTileGeneratorWrapper : public FNonAbandonableTask
{
	TSharedRef<FRecastTileGenerator> TileGenerator;

	FRecastTileGeneratorWrapper(TSharedRef<FRecastTileGenerator> InTileGenerator)
		: TileGenerator(InTileGenerator)
	{
	}
	
	void DoWork()
	{
		TileGenerator->DoWork();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRecastTileGenerator, STATGROUP_ThreadPoolAsyncTasks);
	}
};

typedef FAsyncTask<FRecastTileGeneratorWrapper> FRecastTileGeneratorTask;
//typedef FAsyncTask<FRecastTileGenerator> FRecastTileGeneratorTask;

struct FPendingTileElement
{
	/** tile coordinates on a grid in recast space */
	FIntPoint	Coord;
	/** distance to seed, used for sorting pending tiles */
	FVector::FReal SeedDistance;
	/** Whether we need a full rebuild for this tile grid cell */
	bool		bRebuildGeometry;
	/** We need to store dirty area bounds to check which cached layers needs to be regenerated
	 *  In case geometry is changed cached layers data will be fully regenerated without using dirty areas list
	 */
	TArray<FBox> DirtyAreas;

	FPendingTileElement()
		: Coord(FIntPoint::NoneValue)
		, SeedDistance(TNumericLimits<FVector::FReal>::Max())
		, bRebuildGeometry(false)
	{
	}

	bool operator == (const FIntPoint& Location) const
	{
		return Coord == Location;
	}

	bool operator == (const FPendingTileElement& Other) const
	{
		return Coord == Other.Coord;
	}

	bool operator < (const FPendingTileElement& Other) const
	{
		return Other.SeedDistance < SeedDistance;
	}

	friend uint32 GetTypeHash(const FPendingTileElement& Element)
	{
		return GetTypeHash(Element.Coord);
	}
};

struct FRunningTileElement
{
	FRunningTileElement()
		: Coord(FIntPoint::NoneValue)
		, bShouldDiscard(false)
		, AsyncTask(nullptr)
	{
	}
	
	FRunningTileElement(FIntPoint InCoord)
		: Coord(InCoord)
		, bShouldDiscard(false)
		, AsyncTask(nullptr)
	{
	}

	bool operator == (const FRunningTileElement& Other) const
	{
		return Coord == Other.Coord;
	}
	
	/** tile coordinates on a grid in recast space */
	FIntPoint					Coord;
	/** whether generated results should be discarded */
	bool						bShouldDiscard; 
	FRecastTileGeneratorTask*	AsyncTask;
};

struct FTileTimestamp
{
	FNavTileRef NavTileRef;
	double Timestamp;
	
	bool operator == (const FTileTimestamp& Other) const
	{
		return NavTileRef == Other.NavTileRef;
	}
};

enum class EProcessTileTasksSyncTimeSlicedState : uint8
{
	Init,
	DoWork,
	AddGeneratedTiles,
	StoreCompessedTileCacheLayers,
	AppendUpdateTiles,
	Finish,
};

enum class EAddGeneratedTilesTimeSlicedState : uint8
{
	Init,
	AddTiles,
};

/**
 * Class that handles generation of the whole Recast-based navmesh.
 */
class NAVIGATIONSYSTEM_API FRecastNavMeshGenerator : public FNavDataGenerator
{
public:
	FRecastNavMeshGenerator(ARecastNavMesh& InDestNavMesh);
	virtual ~FRecastNavMeshGenerator();

private:
	/** Prevent copying. */
	FRecastNavMeshGenerator(FRecastNavMeshGenerator const& NoCopy) { check(0); };
	FRecastNavMeshGenerator& operator=(FRecastNavMeshGenerator const& NoCopy) { check(0); return *this; }

public:
	virtual bool RebuildAll() override;
	virtual void EnsureBuildCompletion() override;
	virtual void CancelBuild() override;
	virtual void TickAsyncBuild(float DeltaSeconds) override;
	virtual void OnNavigationBoundsChanged() override;

	/** Asks generator to update navigation affected by DirtyAreas */
	virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas) override;

	/** determines whether this generator is performing navigation building actions at the moment, dirty areas are also checked */
	virtual bool IsBuildInProgressCheckDirty() const override;

#if !RECAST_ASYNC_REBUILDING
	/** returns true if we are time slicing and the data is valid to use false otherwise*/
	virtual bool GetTimeSliceData(int32& OutNumRemainingBuildTasks, double& OutCurrentBuildTaskDuration) const override;
#endif

	int32 GetNumRemaningBuildTasksHelper() const { return RunningDirtyTiles.Num() + PendingDirtyTiles.Num() + static_cast<int32>(SyncTimeSlicedData.TileGeneratorSync.IsValid()); }
	virtual int32 GetNumRemaningBuildTasks() const override;
	virtual int32 GetNumRunningBuildTasks() const override;

	/** Checks if a given tile is being build or has just finished building */
	UE_DEPRECATED(5.1, "Use new version with FNavTileRef")
	bool IsTileChanged(int32 TileIdx) const;

	/** Checks if a given tile is being build or has just finished building */
	bool IsTileChanged(const FNavTileRef InTileRef) const;
		
	FORCEINLINE uint32 GetVersion() const { return Version; }

	const ARecastNavMesh* GetOwner() const { return DestNavMesh; }

	/** update area data */
	void OnAreaAdded(const UClass* AreaClass, int32 AreaID);
		
	//--- accessors --- //
	FORCEINLINE class UWorld* GetWorld() const { return DestNavMesh->GetWorld(); }

	const FRecastBuildConfig& GetConfig() const { return Config; }

	const FRecastNavMeshTileGenerationDebug& GetTileDebugSettings() const { return DestNavMesh->TileGenerationDebug; } 

	const TNavStatArray<FBox>& GetInclusionBounds() const { return InclusionBounds; }
	
	FVector GetRcNavMeshOrigin() const { return RcNavMeshOrigin; }

	/** checks if any on InclusionBounds encapsulates given box.
	 *	@return index to first item in InclusionBounds that meets expectations */
	int32 FindInclusionBoundEncapsulatingBox(const FBox& Box) const;

	/** Total navigable area box, sum of all navigation volumes bounding boxes */
	FBox GetTotalBounds() const { return TotalNavBounds; }

	const FRecastNavMeshCachedData& GetAdditionalCachedData() const { return AdditionalCachedData; }

	bool HasDirtyTiles() const;
	bool HasDirtyTiles(const FBox& AreaBounds) const;
	int32 GetDirtyTilesCount(const FBox& AreaBounds) const;

	bool GatherGeometryOnGameThread() const;
	bool IsTimeSliceRegenActive() const;

	FBox GrowBoundingBox(const FBox& BBox, bool bIncludeAgentHeight) const;
	
	/** Returns if the provided Octree Element should generate geometry on the provided NavDataConfig. Can be used to extend the logic to decide what geometry is generated on what Navmesh */
	virtual bool ShouldGenerateGeometryForOctreeElement(const FNavigationOctreeElement& Element, const FNavDataConfig& NavDataConfig) const;
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ENABLE_VISUAL_LOG
	virtual void ExportNavigationData(const FString& FileName) const override;
	virtual void GrabDebugSnapshot(struct FVisualLogEntry* Snapshot, const FBox& BoundingBox, const FName& CategoryName, ELogVerbosity::Type Verbosity) const override;
#endif

	/** 
	 *	@param Actor is a reference to make callee responsible for assuring it's valid
	 */
	static void ExportComponentGeometry(UActorComponent* Component, FNavigationRelevantData& Data);
	static void ExportVertexSoupGeometry(const TArray<FVector>& Verts, FNavigationRelevantData& Data);

	static void ExportRigidBodyGeometry(UBodySetup& BodySetup, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& LocalToWorld = FTransform::Identity);
	static void ExportRigidBodyGeometry(UBodySetup& BodySetup, TNavStatArray<FVector>& OutTriMeshVertexBuffer, TNavStatArray<int32>& OutTriMeshIndexBuffer, TNavStatArray<FVector>& OutConvexVertexBuffer, TNavStatArray<int32>& OutConvexIndexBuffer, TNavStatArray<int32>& OutShapeBuffer, const FTransform& LocalToWorld = FTransform::Identity);
	static void ExportAggregatedGeometry(const FKAggregateGeom& AggGeom, TNavStatArray<FVector>& OutConvexVertexBuffer, TNavStatArray<int32>& OutConvexIndexBuffer, TNavStatArray<int32>& OutShapeBuffer, const FTransform& LocalToWorld = FTransform::Identity);

#if UE_ENABLE_DEBUG_DRAWING
	/** Converts data encoded in EncodedData.CollisionData to FNavDebugMeshData format */
	static void GetDebugGeometry(const FNavigationRelevantData& EncodedData, FNavDebugMeshData& DebugMeshData);
#endif  // UE_ENABLE_DEBUG_DRAWING

	const FNavRegenTimeSliceManager* GetTimeSliceManager() const { return SyncTimeSlicedData.TimeSliceManager; }
	
	void SetNextTimeSliceRegenActive(bool bRegenState) { SyncTimeSlicedData.bNextTimeSliceRegenActive = bRegenState; }

protected:
	// Performs initial setup of member variables so that generator is ready to
	// do its thing from this point on. Called just after construction by ARecastNavMesh
	virtual void Init();

	// Used to configure Config. Override to influence build properties
	virtual void ConfigureBuildProperties(FRecastBuildConfig& OutConfig);

	// Updates cached list of navigation bounds
	void UpdateNavigationBounds();
		
	// Sorts pending build tiles by proximity to player, so tiles closer to player will get generated first
	virtual void SortPendingBuildTiles();

	// Get seed locations used for sorting pending build tiles. Tiles closer to these locations will be prioritized first.
	virtual void GetSeedLocations(UWorld& World, TArray<FVector2D>& OutSeedLocations) const;

	/** Instantiates dtNavMesh and configures it for tiles generation. Returns false if failed */
	bool ConstructTiledNavMesh();

	/** Determine bit masks for poly address */
	void CalcNavMeshProperties(int32& MaxTiles, int32& MaxPolys);
	
	/** Marks grid tiles affected by specified areas as dirty */
	virtual void MarkDirtyTiles(const TArray<FNavigationDirtyArea>& DirtyAreas);

	/** Returns if the provided UObject that requested a navmesh dirtying should dirty this Navmesh. Useful to avoid tiles regeneration from objects that are excluded from the provided NavDataConfig */
	virtual bool ShouldDirtyTilesRequestedByObject(const UNavigationSystemV1& NavSys, const FNavigationOctree& NavOctreeInstance, const UObject& SourceObject, const FNavDataConfig& NavDataConfig) const;

	/** Marks all tiles overlapping with InclusionBounds dirty (via MarkDirtyTiles). */
	bool MarkNavBoundsDirty();

	UE_DEPRECATED(5.1, "Use new version with FNavTileRef")
	void RemoveLayers(const FIntPoint& Tile, TArray<uint32>& UpdatedTiles);

	void RemoveLayers(const FIntPoint& Tile, TArray<FNavTileRef>& UpdatedTiles);
	
	void StoreCompressedTileCacheLayers(const FRecastTileGenerator& TileGenerator, int32 TileX, int32 TileY);

#if RECAST_INTERNAL_DEBUG_DATA
	void StoreDebugData(const FRecastTileGenerator& TileGenerator, int32 TileX, int32 TileY);
#endif

#if RECAST_ASYNC_REBUILDING
	/** Processes pending tile generation tasks Async*/
	UE_DEPRECATED(5.1, "Use ProcessTileTasksAsyncAndGetUpdatedTiles instead")
	TArray<uint32> ProcessTileTasksAsync(const int32 NumTasksToProcess);

	/** Processes pending tile generation tasks Async*/
	TArray<FNavTileRef> ProcessTileTasksAsyncAndGetUpdatedTiles(const int32 NumTasksToProcess);
#else
	TSharedRef<FRecastTileGenerator> CreateTileGeneratorFromPendingElement(FIntPoint &OutTileLocation, const int32 ForcedPendingTileIdx = INDEX_NONE);

	/** Processes pending tile generation tasks Sync with option for time slicing currently an experimental feature. */
	UE_DEPRECATED(5.1, "Use ProcessTileTasksSyncTimeSlicedAndGetUpdatedTiles instead")
	virtual TArray<uint32> ProcessTileTasksSyncTimeSliced();
	UE_DEPRECATED(5.1, "Use ProcessTileTasksSyncAndGetUpdatedTiles instead")
	TArray<uint32> ProcessTileTasksSync(const int32 NumTasksToProcess);

	/** Processes pending tile generation tasks Sync with option for time slicing currently an experimental feature. */
	virtual TArray<FNavTileRef> ProcessTileTasksSyncTimeSlicedAndGetUpdatedTiles();
	TArray<FNavTileRef> ProcessTileTasksSyncAndGetUpdatedTiles(const int32 NumTasksToProcess);

	virtual int32 GetNextPendingDirtyTileToBuild() const;
#endif
	/** Processes pending tile generation tasks */
	UE_DEPRECATED(5.1, "Use ProcessTileTasksAndGetUpdatedTiles instead")
	TArray<uint32> ProcessTileTasks(const int32 NumTasksToProcess);

	/** Processes pending tile generation tasks */
	TArray<FNavTileRef> ProcessTileTasksAndGetUpdatedTiles(const int32 NumTasksToProcess);

	void ResetTimeSlicedTileGeneratorSync();

public:
	/** Adds generated tiles to NavMesh, replacing old ones, uses time slicing returns Failed if any layer failed */
	UE_DEPRECATED(5.1, "Use new version with FNavTileRef")
	ETimeSliceWorkResult AddGeneratedTilesTimeSliced(FRecastTileGenerator& TileGenerator, TArray<uint32>& OutResultTileIndices);

	/** Adds generated tiles to NavMesh, replacing old ones, uses time slicing returns Failed if any layer failed */
	ETimeSliceWorkResult AddGeneratedTilesTimeSliced(FRecastTileGenerator& TileGenerator, TArray<FNavTileRef>& OutResultTileRefs);

	/** Adds generated tiles to NavMesh, replacing old ones */
	UE_DEPRECATED(5.1, "Use AddGeneratedTilesAndGetUpdatedTiles instead")
	TArray<uint32> AddGeneratedTiles(FRecastTileGenerator& TileGenerator);

	/** Adds generated tiles to NavMesh, replacing old ones */
	TArray<FNavTileRef> AddGeneratedTilesAndGetUpdatedTiles(FRecastTileGenerator& TileGenerator);

public:
	/** Removes all tiles at specified grid location */
	UE_DEPRECATED(5.1, "Use RemoveTileLayersAndGetUpdatedTiles instead")
	TArray<uint32> RemoveTileLayers(const int32 TileX, const int32 TileY, TMap<int32, dtPolyRef>* OldLayerTileIdMap = nullptr);

	/** Removes all tiles at specified grid location */
	TArray<FNavTileRef> RemoveTileLayersAndGetUpdatedTiles(const int32 TileX, const int32 TileY, TMap<int32, dtPolyRef>* OldLayerTileIdMap = nullptr);

	void RemoveTiles(const TArray<FIntPoint>& Tiles);
	void ReAddTiles(const TArray<FIntPoint>& Tiles);

	bool IsBuildingRestrictedToActiveTiles() const { return bRestrictBuildingToActiveTiles; }

	/** sets a limit to number of asynchronous tile generators running at one time
	 *	@note if used at runtime will not result in killing tasks above limit count
	 *	@mote function does not validate the input parameter - it's on caller */
	void SetMaxTileGeneratorTasks(int32 NewLimit) { MaxTileGeneratorTasks = NewLimit; }

	static void CalcPolyRefBits(ARecastNavMesh* NavMeshOwner, int32& MaxTileBits, int32& MaxPolyBits);

protected:
	bool IsInActiveSet(const FIntPoint& Tile) const;
	virtual void RestrictBuildingToActiveTiles(bool InRestrictBuildingToActiveTiles);
	
	/** Blocks until build for specified list of tiles is complete and discard results */
	void DiscardCurrentBuildingTasks();

	virtual TSharedRef<FRecastTileGenerator> CreateTileGenerator(const FIntPoint& Coord, const TArray<FBox>& DirtyAreas);

	template <typename T>
	TSharedRef<T> ConstuctTileGeneratorImpl(const FIntPoint& Coord, const TArray<FBox>& DirtyAreas)
	{
		TSharedRef<T> TileGenerator = MakeShareable(new T(*this, Coord));
		TileGenerator->Setup(*this, DirtyAreas);
		return TileGenerator;
	}

	void SetBBoxGrowth(const FVector& InBBox) { BBoxGrowth = InBBox; }

	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	virtual uint32 LogMemUsed() const override;

	UE_DEPRECATED(5.1, "Use new version with FNavTileRef")
	void AddGeneratedTileLayer(int32 LayerIndex, FRecastTileGenerator& TileGenerator, const TMap<int32, dtPolyRef>& OldLayerTileIdMap, TArray<uint32>& OutResultTileIndices);

	bool IsAllowedToAddTileLayers(const FIntPoint Tile) const;
	void AddGeneratedTileLayer(int32 LayerIndex, FRecastTileGenerator& TileGenerator, const TMap<int32, dtPolyRef>& OldLayerTileIdMap, TArray<FNavTileRef>& OutResultTileRefs);

#if !UE_BUILD_SHIPPING
	/** Data struct used by 'LogDirtyAreas' that contains all the information regarding the areas that are being dirtied, per dirtied tile. */
	struct FNavigationDirtyAreaPerTileDebugInformation
	{
		FNavigationDirtyArea DirtyArea;
		bool bTileWasAlreadyAdded = false;
	};

	/** Used internally, when LogNavigationDirtyArea is VeryVerbose, to log the number of tiles a dirty area is requesting. */
	void LogDirtyAreas(const TMap<FPendingTileElement, TArray<FNavigationDirtyAreaPerTileDebugInformation>>& DirtyAreasDebuggingInformation) const; 
#endif
	
protected:
	friend ARecastNavMesh;

	/** Parameters defining navmesh tiles */
	FRecastBuildConfig Config;

	/** Used to grow generic element bounds to match this generator's properties
	 *	(most notably Config.borderSize) */
	FVector BBoxGrowth;
	
	int32 NumActiveTiles;
	/** the limit to number of asynchronous tile generators running at one time,
	 *  this is also used by the sync non time sliced regeneration ProcessTileTasksSync,
	 *  but not by ProcessTileTasksSyncTimeSliced.
	 */
	int32 MaxTileGeneratorTasks;

	FVector::FReal AvgLayersPerTile;

	/** Total bounding box that includes all volumes, in unreal units. */
	FBox TotalNavBounds;

	/** Bounding geometry definition. */
	TNavStatArray<FBox> InclusionBounds;

	/** Navigation mesh that owns this generator */
	ARecastNavMesh*	DestNavMesh;
	
	/** List of dirty tiles that needs to be regenerated */
	TNavStatArray<FPendingTileElement> PendingDirtyTiles;			
	
	/** List of dirty tiles currently being regenerated */
	TNavStatArray<FRunningTileElement> RunningDirtyTiles;

#if WITH_EDITOR
	/** List of tiles that were recently regenerated */
	TNavStatArray<FTileTimestamp> RecentlyBuiltTiles;
#endif// WITH_EDITOR
	
	TArray<FIntPoint> ActiveTiles;

	/** */
	FRecastNavMeshCachedData AdditionalCachedData;

	/** Use this if you don't want your tiles to start at (0,0,0) */
	FVector RcNavMeshOrigin;

	double RebuildAllStartTime = 0;
	
	uint32 bInitialized:1;

	uint32 bRestrictBuildingToActiveTiles:1;

	uint32 bSortTilesWithSeedLocations:1;

	/** Runtime generator's version, increased every time all tile generators get invalidated
	 *	like when navmesh size changes */
	uint32 Version;

	/** Grouping all the member variables used by the time slicing code together for neatness */
	struct FSyncTimeSlicedData
	{
		FSyncTimeSlicedData();

		double CurrentTileRegenDuration;
		/** if we are currently using time sliced regen or not - currently an experimental feature.
		 *  do not manipulate this value directly instead call SetNextTimeSliceRegenActive()
		 */
		bool bTimeSliceRegenActive;
		bool bNextTimeSliceRegenActive;
		
		/** Used by ProcessTileTasksSyncTimeSliced */
		EProcessTileTasksSyncTimeSlicedState ProcessTileTasksSyncState;

		/** Used by ProcessTileTasksSyncTimeSliced */
		TArray<FNavTileRef> UpdatedTilesCache;

		/** Used by AddGeneratedTilesTimeSliced */
		TArray<FNavTileRef> ResultTileRefsCached;

		/** Used by AddGeneratedTilesTimeSliced */
		TMap<int32, dtPolyRef> OldLayerTileIdMapCached;

		/** Used by AddGeneratedTilesTimeSliced */
		EAddGeneratedTilesTimeSlicedState AddGeneratedTilesState;

		/** Used by AddGeneratedTilesTimeSliced */
		int32 AddGenTilesLayerIndex;

		/** Do not null or Reset this directly instead call ResetTimeSlicedTileGeneratorSync(). The only exception currently is in ProcessTileTasksSyncTimeSliced */
		TSharedPtr<FRecastTileGenerator> TileGeneratorSync;
		FNavRegenTimeSliceManager* TimeSliceManager;
	};

	FSyncTimeSlicedData SyncTimeSlicedData;
};

#endif // WITH_RECAST
