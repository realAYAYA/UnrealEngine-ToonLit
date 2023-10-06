// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "ActorPartition/ActorPartitionSubsystem.h"

#if WITH_EDITOR
#include "UObject/UE5MainStreamObjectVersion.h"

FPartitionActorDesc::FPartitionActorDesc()
	: GridSize(0)
	, GridIndexX(0)
	, GridIndexY(0)
	, GridIndexZ(0)
{
}

void FPartitionActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const APartitionActor* PartitionActor = CastChecked<APartitionActor>(InActor);

	GridSize = PartitionActor->GetGridSize();

	if (!bIsDefaultActorDesc)
	{
		GridGuid = PartitionActor->GetGridGuid();
	
		const FVector ActorLocation = InActor->GetActorLocation();
		SetGridIndices(ActorLocation.X, ActorLocation.Y, ActorLocation.Z);
	}
}

void FPartitionActorDesc::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	FWorldPartitionActorDesc::Serialize(Ar);
	
	if (!bIsDefaultActorDesc)
	{
		Ar << GridSize << GridIndexX << GridIndexY << GridIndexZ;

		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::PartitionActorDescSerializeGridGuid)
		{
			Ar << GridGuid;
		}
	}
}

bool FPartitionActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FWorldPartitionActorDesc::Equals(Other))
	{
		const FPartitionActorDesc* PartitionActorDesc = (FPartitionActorDesc*)Other;

		return
			GridSize == PartitionActorDesc->GridSize &&
			GridIndexX == PartitionActorDesc->GridIndexX &&
			GridIndexY == PartitionActorDesc->GridIndexY &&
			GridIndexZ == PartitionActorDesc->GridIndexZ &&
			GridGuid == PartitionActorDesc->GridGuid;		
	}

	return false;
}

FBox FPartitionActorDesc::GetEditorBounds() const
{
	const UActorPartitionSubsystem::FCellCoord CellCoord(GridIndexX, GridIndexY, GridIndexZ, 0);
	return UActorPartitionSubsystem::FCellCoord::GetCellBounds(CellCoord, GridSize);
}

void FPartitionActorDesc::TransferWorldData(const FWorldPartitionActorDesc* From)
{
	FWorldPartitionActorDesc::TransferWorldData(From);

	// TransferWorldData is called for actors that are not added to the world (not registered)
	// Transfer properties that depend on actor being added to the world (components being registered)
	const FPartitionActorDesc* FromPartitionActorDesc = (FPartitionActorDesc*)From;
	GridIndexX = FromPartitionActorDesc->GridIndexX;
	GridIndexY = FromPartitionActorDesc->GridIndexY;
	GridIndexZ = FromPartitionActorDesc->GridIndexZ;
}

void FPartitionActorDesc::SetGridIndices(double X, double Y, double Z)
{
	if (GridSize > 0)
	{
		GridIndexX = FMath::FloorToInt(X / GridSize);
		GridIndexY = FMath::FloorToInt(Y / GridSize);
		GridIndexZ = FMath::FloorToInt(Z / GridSize);
	}
	else
	{
		GridIndexX = GridIndexY = GridIndexZ = 0;
	}
}

#endif
