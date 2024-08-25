// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "UObject/ObjectMacros.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "AI/Navigation/NavigationTypes.h"
#endif
#include "AI/Navigation/NavigationDataChunk.h"
#include "NavMesh/RecastNavMesh.h"
#include "RecastNavMeshDataChunk.generated.h"

class FPImplRecastNavMesh;

struct FRecastTileData
{
	struct FRawData
	{
		FRawData(uint8* InData);
		~FRawData();

		uint8* RawData;
	};

	FRecastTileData();
	FRecastTileData(int32 TileDataSize, uint8* TileRawData, int32 TileCacheDataSize, uint8* TileCacheRawData);
	
	// Location of attached tile
	int32					OriginalX;	// Tile X coordinates when gathered
	int32					OriginalY;	// Tile Y coordinates when gathered
	int32					X;					
	int32					Y;					
	int32					Layer;
		
	// Tile data
	int32					TileDataSize;
	TSharedPtr<FRawData>	TileRawData;

	// Compressed tile cache layer 
	int32					TileCacheDataSize;
	TSharedPtr<FRawData>	TileCacheRawData;

	// Whether this tile is attached to NavMesh
	bool					bAttached;	
};

class dtNavMesh;
class FPImplRecastNavMesh;

enum EGatherTilesCopyMode
{
	NoCopy = 0,
	CopyData = 1 << 0,
	CopyCacheData = 1 << 1,
	CopyDataAndCacheData = (CopyData | CopyCacheData)
};

/** 
 * 
 */
UCLASS(MinimalAPI)
class URecastNavMeshDataChunk : public UNavigationDataChunk
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	NAVIGATIONSYSTEM_API virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	NAVIGATIONSYSTEM_API TArray<uint32> AttachTiles(FPImplRecastNavMesh& NavMeshImpl);

	/** Attaches tiles to specified navmesh */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	NAVIGATIONSYSTEM_API TArray<uint32> AttachTiles(FPImplRecastNavMesh& NavMeshImpl, const bool bKeepCopyOfData, const bool bKeepCopyOfCacheData);

	/** Detaches tiles from specified navmesh, taking tile ownership */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	NAVIGATIONSYSTEM_API TArray<uint32> DetachTiles(FPImplRecastNavMesh& NavMeshImpl);

	/** Detaches tiles from specified navmesh */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	NAVIGATIONSYSTEM_API TArray<uint32> DetachTiles(FPImplRecastNavMesh& NavMeshImpl, const bool bTakeDataOwnership, const bool bTakeCacheDataOwnership);

#if WITH_RECAST
	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	NAVIGATIONSYSTEM_API TArray<FNavTileRef> AttachTiles(ARecastNavMesh& NavMesh);

	/** Attaches tiles to specified navmesh */
	NAVIGATIONSYSTEM_API TArray<FNavTileRef> AttachTiles(ARecastNavMesh& NavMesh, const bool bKeepCopyOfData, const bool bKeepCopyOfCacheData);

	/** Detaches tiles from specified navmesh, taking tile ownership */
	NAVIGATIONSYSTEM_API TArray<FNavTileRef> DetachTiles(ARecastNavMesh& NavMesh);

	/** Detaches tiles from specified navmesh */
	NAVIGATIONSYSTEM_API TArray<FNavTileRef> DetachTiles(ARecastNavMesh& NavMesh, const bool bTakeDataOwnership, const bool bTakeCacheDataOwnership);
#endif // WITH_RECAST

	/** 
	 * Experimental: Moves tiles data on the xy plane by the offset (in tile coordinates) and rotation (in degree).
	 * @param NavMeshImpl		Recast navmesh implementation.
	 * @param Offset			Offset in tile coordinates.
	 * @param RotationDeg		Rotation in degrees.
	 * @param RotationCenter	World position
	 */
	NAVIGATIONSYSTEM_API void MoveTiles(FPImplRecastNavMesh& NavMeshImpl, const FIntPoint& Offset, const FVector::FReal RotationDeg, const FVector2D& RotationCenter);
	
	/** Number of tiles in this chunk */
	NAVIGATIONSYSTEM_API int32 GetNumTiles() const;

	/** Const accessor to the list of tiles in the data chunk. */
	const TArray<FRecastTileData>& GetTiles() const { return Tiles; }

	/** Returns the AABB for the given tiles. */
	NAVIGATIONSYSTEM_API void GetTilesBounds(const FPImplRecastNavMesh& NavMeshImpl, const TArray<int32>& TileIndices, FBox& OutBounds) const;

	/** Mutable accessor to the list of tiles in the data chunk. */
	TArray<FRecastTileData>& GetMutableTiles() { return Tiles; }

	/** Releases all tiles that this chunk holds */
	NAVIGATIONSYSTEM_API void ReleaseTiles();

	/** Collect tiles with data and/or cache data from the provided TileIndices. */
	NAVIGATIONSYSTEM_API void GetTiles(const FPImplRecastNavMesh* NavMeshImpl, const TArray<int32>& TileIndices, const EGatherTilesCopyMode CopyMode, const bool bMarkAsAttached = true);

private:
#if WITH_RECAST
	NAVIGATIONSYSTEM_API void SerializeRecastData(FArchive& Ar, int32 NavMeshVersion);
#endif//WITH_RECAST

private:
	TArray<FRecastTileData> Tiles;
};
