// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavigationTypes.h"
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
UCLASS()
class NAVIGATIONSYSTEM_API URecastNavMeshDataChunk : public UNavigationDataChunk
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	TArray<uint32> AttachTiles(FPImplRecastNavMesh& NavMeshImpl);

	/** Attaches tiles to specified navmesh */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	TArray<uint32> AttachTiles(FPImplRecastNavMesh& NavMeshImpl, const bool bKeepCopyOfData, const bool bKeepCopyOfCacheData);

	/** Detaches tiles from specified navmesh, taking tile ownership */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	TArray<uint32> DetachTiles(FPImplRecastNavMesh& NavMeshImpl);

	/** Detaches tiles from specified navmesh */
	UE_DEPRECATED(5.1, "Use overload using ARecastNavMesh& instead")
	TArray<uint32> DetachTiles(FPImplRecastNavMesh& NavMeshImpl, const bool bTakeDataOwnership, const bool bTakeCacheDataOwnership);

	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	TArray<FNavTileRef> AttachTiles(ARecastNavMesh& NavMesh);

	/** Attaches tiles to specified navmesh */
	TArray<FNavTileRef> AttachTiles(ARecastNavMesh& NavMesh, const bool bKeepCopyOfData, const bool bKeepCopyOfCacheData);

	/** Detaches tiles from specified navmesh, taking tile ownership */
	TArray<FNavTileRef> DetachTiles(ARecastNavMesh& NavMesh);

	/** Detaches tiles from specified navmesh */
	TArray<FNavTileRef> DetachTiles(ARecastNavMesh& NavMesh, const bool bTakeDataOwnership, const bool bTakeCacheDataOwnership);

	/** 
	 * Experimental: Moves tiles data on the xy plane by the offset (in tile coordinates) and rotation (in degree).
	 * @param NavMeshImpl		Recast navmesh implementation.
	 * @param Offset			Offset in tile coordinates.
	 * @param RotationDeg		Rotation in degrees.
	 * @param RotationCenter	World position
	 */
	void MoveTiles(FPImplRecastNavMesh& NavMeshImpl, const FIntPoint& Offset, const FVector::FReal RotationDeg, const FVector2D& RotationCenter);
	
	/** Number of tiles in this chunk */
	int32 GetNumTiles() const;

	/** Const accessor to the list of tiles in the data chunk. */
	const TArray<FRecastTileData>& GetTiles() const { return Tiles; }

	/** Returns the AABB for the given tiles. */
	void GetTilesBounds(const FPImplRecastNavMesh& NavMeshImpl, const TArray<int32>& TileIndices, FBox& OutBounds) const;

	/** Mutable accessor to the list of tiles in the data chunk. */
	TArray<FRecastTileData>& GetMutableTiles() { return Tiles; }

	/** Releases all tiles that this chunk holds */
	void ReleaseTiles();

	/** Collect tiles with data and/or cache data from the provided TileIndices. */
	void GetTiles(const FPImplRecastNavMesh* NavMeshImpl, const TArray<int32>& TileIndices, const EGatherTilesCopyMode CopyMode, const bool bMarkAsAttached = true);

private:
#if WITH_RECAST
	void SerializeRecastData(FArchive& Ar, int32 NavMeshVersion);
#endif//WITH_RECAST

private:
	TArray<FRecastTileData> Tiles;
};
