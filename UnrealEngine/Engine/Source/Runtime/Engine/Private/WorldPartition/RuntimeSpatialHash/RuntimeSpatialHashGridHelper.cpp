// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeSpatialHash/RuntimeSpatialHashGridHelper.h"
#include "ActorPartition/PartitionActor.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "Algo/Transform.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"

bool GRuntimeSpatialHashUseAlignedGridLevels = true;
static FAutoConsoleVariableRef CVarRuntimeSpatialHashUseAlignedGridLevels(
	TEXT("wp.Runtime.RuntimeSpatialHashUseAlignedGridLevels"),
	GRuntimeSpatialHashUseAlignedGridLevels,
	TEXT("Set RuntimeSpatialHashUseAlignedGridLevels to false to help break the pattern caused by world partition promotion of actors to upper grid levels that are always aligned on child levels."));

bool GRuntimeSpatialHashSnapNonAlignedGridLevelsToLowerLevels = true;
static FAutoConsoleVariableRef CVarRuntimeSpatialHashSnapNonAlignedGridLevelsToLowerLevels(
	TEXT("wp.Runtime.RuntimeSpatialHashSnapNonAlignedGridLevelsToLowerLevels"),
	GRuntimeSpatialHashSnapNonAlignedGridLevelsToLowerLevels,
	TEXT("Set RuntimeSpatialHashSnapNonAlignedGridLevelsToLowerLevels to false to avoid snapping higher levels cells to child cells. Only used when GRuntimeSpatialHashUseAlignedGridLevels is false."));

bool GRuntimeSpatialHashPlaceSmallActorsUsingLocation = false;
static FAutoConsoleVariableRef CVarRuntimeSpatialHashPlaceSmallActorsUsingLocation(
	TEXT("wp.Runtime.RuntimeSpatialHashPlaceSmallActorsUsingLocation"),
	GRuntimeSpatialHashPlaceSmallActorsUsingLocation,
	TEXT("Set RuntimeSpatialHashPlaceSmallActorsUsingLocation to true to place actors smaller than a cell size into their corresponding cell using their location instead of their bounding box."));

bool GRuntimeSpatialHashPlacePartitionActorsUsingLocation = true;
static FAutoConsoleVariableRef CVarRuntimeSpatialHashPlacePartitionActorsUsingLocation(
	TEXT("wp.Runtime.RuntimeSpatialHashPlacePartitionActorsUsingLocation"),
	GRuntimeSpatialHashPlacePartitionActorsUsingLocation,
	TEXT("Set RuntimeSpatialHashPlacePartitionActorsUsingLocation to true to place partitioned actors into their corresponding cell using their location instead of their bounding box."));

FSquare2DGridHelper::FSquare2DGridHelper(const FBox& InWorldBounds, const FVector& InOrigin, int64 InCellSize)
	: WorldBounds(InWorldBounds)
	, Origin(InOrigin)
	, CellSize(InCellSize)
{
	// Compute Grid's size and level count based on World bounds
	int64 GridSize = 1;
	int32 GridLevelCount = 1;

	if (WorldBounds.IsValid)
	{
		const FVector2D DistMin = FVector2D(WorldBounds.Min - Origin).GetAbs();
		const FVector2D DistMax = FVector2D(WorldBounds.Max - Origin).GetAbs();
		const double WorldBoundsMaxExtent = FMath::Max(DistMin.GetMax(), DistMax.GetMax());

		if (WorldBoundsMaxExtent > 0)
		{
			GridSize = 2 * FMath::CeilToDouble(WorldBoundsMaxExtent / CellSize); 
			if (!FMath::IsPowerOfTwo(GridSize))
			{
				GridSize = FMath::Pow(2, FMath::CeilToDouble(FMath::Log2(static_cast<double>(GridSize))));
			}
			GridLevelCount = FMath::FloorLog2_64(GridSize) + 1;
		}
	}

	check(FMath::IsPowerOfTwo(GridSize));

	Levels.Reserve(GridLevelCount);
	int64 CurrentCellSize = CellSize;
	int64 CurrentGridSize = GridSize;
	for (int32 Level = 0; Level < GridLevelCount; ++Level)
	{
		int64 LevelGridSize = CurrentGridSize;

		if (!GRuntimeSpatialHashUseAlignedGridLevels)
		{
			// Except for top level, adding 1 to CurrentGridSize (which is always a power of 2) breaks the pattern of perfectly aligned cell edges between grid level cells.
			// This will prevent weird artefact during actor promotion when an actor is placed using its bounds and which overlaps multiple cells.
			// In this situation, the algorithm will try to find a cell that encapsulates completely the actor's bounds by searching in the upper levels, until it finds one.
			// Also note that, the default origin of each level will always be centered at the middle of the bounds of (level's cellsize * level's grid size).
			LevelGridSize = (Level == GridLevelCount - 1) ? CurrentGridSize : CurrentGridSize + 1;
		}

		Levels.Emplace(FVector2D(InOrigin), CurrentCellSize, LevelGridSize, Level);

		CurrentCellSize <<= 1;
		CurrentGridSize >>= 1;
	}

#if WITH_EDITOR
	// Make sure the always loaded cell exists
	GetAlwaysLoadedCell();
#endif
}

#if WITH_EDITOR
void FSquare2DGridHelper::ForEachCells(TFunctionRef<void(const FSquare2DGridHelper::FGridLevel::FGridCell&)> InOperation) const
{
	for (int32 Level = 0; Level < Levels.Num(); Level++)
	{
		for (const FSquare2DGridHelper::FGridLevel::FGridCell& ThisCell : Levels[Level].Cells)
		{
			InOperation(ThisCell);
		}
	}
}
#endif

int32 FSquare2DGridHelper::ForEachIntersectingCells(const FBox& InBox, TFunctionRef<void(const FGridCellCoord&)> InOperation, int32 InStartLevel) const
{
	int32 NumCells = 0;

	for (int32 Level = InStartLevel; Level < Levels.Num(); Level++)
	{
		NumCells += Levels[Level].ForEachIntersectingCells(InBox, [InBox, Level, InOperation](const FGridCellCoord2& Coord) { InOperation(FGridCellCoord(Coord.X, Coord.Y, Level)); });
	}

	return NumCells;
}

int32 FSquare2DGridHelper::ForEachIntersectingCells(const FSphere& InSphere, TFunctionRef<void(const FGridCellCoord&)> InOperation, int32 InStartLevel) const
{
	int32 NumCells = 0;

	for (int32 Level = InStartLevel; Level < Levels.Num(); Level++)
	{
		NumCells += Levels[Level].ForEachIntersectingCells(InSphere, [InSphere, Level, InOperation](const FGridCellCoord2& Coord) { InOperation(FGridCellCoord(Coord.X, Coord.Y, Level)); });
	}

	return NumCells;
}

int32 FSquare2DGridHelper::ForEachIntersectingCells(const FSphericalSector& InShape, TFunctionRef<void(const FGridCellCoord&)> InOperation, int32 InStartLevel) const
{
	int32 NumCells = 0;

	for (int32 Level = InStartLevel; Level < Levels.Num(); Level++)
	{
		NumCells += Levels[Level].ForEachIntersectingCells(InShape, [Level, InOperation](const FGridCellCoord2& Coord) { InOperation(FGridCellCoord(Coord.X, Coord.Y, Level)); });
	}

	return NumCells;
}

#if WITH_EDITOR
FSquare2DGridHelper GetGridHelper(const FBox& WorldBounds, int64 GridCellSize)
{
	// Default grid to a minimum of 1 level and 1 cell, for always loaded actors
	FVector GridOrigin = FVector::ZeroVector;
	return FSquare2DGridHelper(WorldBounds, GridOrigin, GridCellSize);
}

FSquare2DGridHelper GetPartitionedActors(const FBox& WorldBounds, const FSpatialHashRuntimeGrid& Grid, const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GetPartitionedActors);
	UE_SCOPED_TIMER(TEXT("GetPartitionedActors"), LogWorldPartition, Display);

	//
	// Create the hierarchical grids for the game
	//	
	FSquare2DGridHelper PartitionedActors = GetGridHelper(WorldBounds, Grid.CellSize);
	if (ensure(PartitionedActors.Levels.Num()) && WorldBounds.IsValid)
	{
		int32 IntersectingCellCount = 0;
		FSquare2DGridHelper::FGridLevel& LastGridLevel = PartitionedActors.Levels.Last();
		LastGridLevel.ForEachIntersectingCells(WorldBounds, [&IntersectingCellCount](const FGridCellCoord2& Coords) { ++IntersectingCellCount; });
		if (!ensure(IntersectingCellCount == 1))
		{
			UE_LOG(LogWorldPartition, Warning, TEXT("Can't find grid cell that encompasses world bounds."));
		}
	}

	const int32 GridLevelCount = PartitionedActors.Levels.Num();
	const int64 CellSize = PartitionedActors.GetLowestLevel().CellSize;
	const float CellArea = CellSize * CellSize;

	auto ShouldActorUseLocationPlacement = [CellArea, CellSize, GridLevelCount](const IStreamingGenerationContext::FActorSetInstance* InActorSetInstance, const FBox2D& InActorSetInstanceBounds, int32& OutGridLevel)
	{
		OutGridLevel = 0;
		if (GRuntimeSpatialHashPlaceSmallActorsUsingLocation)
		{
			return InActorSetInstanceBounds.GetArea() <= CellArea;
		}

		if (GRuntimeSpatialHashPlacePartitionActorsUsingLocation)
		{
			bool bUseLocation = true;
			for (const FGuid& ActorGuid : InActorSetInstance->ActorSet->Actors)
			{
				const FWorldPartitionActorDescView& ActorDescView = InActorSetInstance->ContainerInstance->ActorDescViewMap->FindByGuidChecked(ActorGuid);

				if (!ActorDescView.GetActorNativeClass()->IsChildOf<APartitionActor>())
				{
					bUseLocation = false;
					break;
				}
			}
			if (bUseLocation)
			{
				// Find grid level that best matches actor set bounds
				const float MaxLength = InActorSetInstanceBounds.GetExtent().GetMax() * 2.0;
				OutGridLevel = FMath::Min(FMath::CeilToInt(FMath::Max<float>(FMath::Log2(MaxLength / CellSize), 0)), GridLevelCount - 1);
			}
			return bUseLocation;
		}

		return false;
	};

	for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : ActorSetInstances)
	{
		check(ActorSetInstance->ActorSet->Actors.Num() > 0);
		FSquare2DGridHelper::FGridLevel::FGridCell* GridCell = nullptr;

		if (ActorSetInstance->bIsSpatiallyLoaded)
		{
			const FBox2D ActorSetInstanceBounds(FVector2D(ActorSetInstance->Bounds.Min), FVector2D(ActorSetInstance->Bounds.Max));
			int32 LocationPlacementGridLevel = 0;
			if (ShouldActorUseLocationPlacement(ActorSetInstance, ActorSetInstanceBounds, LocationPlacementGridLevel))
			{
				// Find grid level cell that contains the actor cluster pivot and put actors in it.
				FGridCellCoord2 CellCoords;
				if (PartitionedActors.Levels[LocationPlacementGridLevel].GetCellCoords(ActorSetInstanceBounds.GetCenter(), CellCoords))
				{
					GridCell = &PartitionedActors.Levels[LocationPlacementGridLevel].GetCell(CellCoords);
				}
			}
			else
			{
				// Find grid level cell that encompasses the actor cluster bounding box and put actors in it.
				const FVector2D ClusterSize = ActorSetInstanceBounds.GetSize();
				const double MinRequiredCellExtent = FMath::Max(ClusterSize.X, ClusterSize.Y);
				const int32 FirstPotentialGridLevel = FMath::Max(FMath::CeilToDouble(FMath::Log2(MinRequiredCellExtent / (double)PartitionedActors.CellSize)), 0);

				for (int32 GridLevelIndex = FirstPotentialGridLevel; GridLevelIndex < PartitionedActors.Levels.Num(); GridLevelIndex++)
				{
					FSquare2DGridHelper::FGridLevel& GridLevel = PartitionedActors.Levels[GridLevelIndex];

					if (GridLevel.GetNumIntersectingCells(ActorSetInstance->Bounds) == 1)
					{
						GridLevel.ForEachIntersectingCells(ActorSetInstance->Bounds, [&GridLevel, &GridCell](const FGridCellCoord2& Coords)
						{
							check(!GridCell);
							GridCell = &GridLevel.GetCell(Coords);
						});

						break;
					}
				}
			}
		}
		
		if (!GridCell)
		{
			GridCell = &PartitionedActors.GetAlwaysLoadedCell();
		}

		GridCell->AddActorSetInstance(ActorSetInstance);
	}

	return PartitionedActors;
}

#endif // #if WITH_EDITOR

FORCEINLINE static bool IsClockWise(const FVector2D& V1, const FVector2D& V2) 
{
	return (V1 ^ V2) < 0;
}

FORCEINLINE static bool IsVectorInsideVectorPair(const FVector2D& TestVector, const FVector2D& V1, const FVector2D& V2)
{
	return IsClockWise(V1, TestVector) && !IsClockWise(V2, TestVector);
}

FORCEINLINE static bool IsPointInsideSector(const FVector2D& TestPoint, const FVector2D& SectorCenter, float SectorRadiusSquared, const FVector2D& SectorStart, const FVector2D& SectorEnd, float SectorAngle)
{
	const FVector2D TestVector = TestPoint - SectorCenter;
	if (TestVector.SizeSquared() > SectorRadiusSquared)
	{
		return false;
	}
	
	if (SectorAngle <= 180.0f)
	{
		return IsVectorInsideVectorPair(TestVector, SectorStart, SectorEnd);
	}
	else
	{
		return !IsVectorInsideVectorPair(TestVector, SectorEnd, SectorStart);
	}
}

bool FSquare2DGridHelper::FGrid2D::DoesCircleSectorIntersectsCell(const FGridCellCoord2& Coords, const FVector2D& SectorCenter, float SectorRadiusSquared, const FVector2D& SectorStartVector, const FVector2D& SectorEndVector, float SectorAngle) const
{
	const int64 CellIndex = Coords.Y * GridSize + Coords.X;
	FBox2D CellBounds;
	GetCellBounds(CellIndex, CellBounds);

	// Test whether any cell corners are inside sector
	if (IsPointInsideSector(FVector2D(CellBounds.Min.X, CellBounds.Min.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, SectorAngle) ||
		IsPointInsideSector(FVector2D(CellBounds.Max.X, CellBounds.Min.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, SectorAngle) ||
		IsPointInsideSector(FVector2D(CellBounds.Max.X, CellBounds.Max.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, SectorAngle) ||
		IsPointInsideSector(FVector2D(CellBounds.Min.X, CellBounds.Max.Y), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, SectorAngle))
	{
		return true;
	}

	// Test whether any sector point lies inside the cell bounds
	if (CellBounds.IsInside(SectorCenter) ||
		CellBounds.IsInside(SectorCenter + SectorStartVector) ||
		CellBounds.IsInside(SectorCenter + SectorEndVector))
	{
		return true;
	}

	// Test whether closest point on cell from center is inside sector
	if (IsPointInsideSector(CellBounds.GetClosestPointTo(SectorCenter), SectorCenter, SectorRadiusSquared, SectorStartVector, SectorEndVector, SectorAngle))
	{
		return true;
	}

	return false;
}

int32 FSquare2DGridHelper::FGrid2D::ForEachIntersectingCells(const FSphericalSector& InShape, TFunctionRef<void(const FGridCellCoord2&)> InOperation) const
{
	check(!InShape.IsNearlyZero());
	const bool bIsSphere = InShape.IsSphere();
	const FVector ScaledAxis = FVector(FVector2D(InShape.GetAxis()), 0).GetSafeNormal() * InShape.GetRadius();
	const FVector SectorStart = FRotator(0, 0.5f * InShape.GetAngle(), 0).RotateVector(ScaledAxis);
	const FVector SectorEnd = FRotator(0, -0.5f * InShape.GetAngle(), 0).RotateVector(ScaledAxis);

	const FVector2D Center2D(InShape.GetCenter());
	const FVector2D SectorStart2D = FVector2D(SectorStart);
	const FVector2D SectorEnd2D = FVector2D(SectorEnd);
	const float SectorRadiusSquared = InShape.GetRadius() * InShape.GetRadius();

	int32 NumCells = 0;
	const FSphere Sphere(InShape.GetCenter(), InShape.GetRadius());
	ForEachIntersectingCells(Sphere, [this, bIsSphere, Center2D, SectorRadiusSquared, SectorStart2D, SectorEnd2D, SectorAngle = InShape.GetAngle(), InOperation, &NumCells](const FGridCellCoord2& Coords)
	{
		// If sector, filter coords that intersect sector
		if (bIsSphere || DoesCircleSectorIntersectsCell(Coords, Center2D, SectorRadiusSquared, SectorStart2D, SectorEnd2D, SectorAngle))
		{
			InOperation(Coords);
			NumCells++;
		}
	});

	return NumCells;
}

