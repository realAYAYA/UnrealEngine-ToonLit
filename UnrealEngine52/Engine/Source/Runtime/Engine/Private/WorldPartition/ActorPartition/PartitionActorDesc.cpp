// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "ActorPartition/ActorPartitionSubsystem.h"

#if WITH_EDITOR
#include "UObject/UE5MainStreamObjectVersion.h"

void FPartitionActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const APartitionActor* PartitionActor = CastChecked<APartitionActor>(InActor);

	GridSize = PartitionActor->GridSize;
	GridGuid = PartitionActor->GetGridGuid();
	
	const FVector ActorLocation = InActor->GetActorLocation();
	GridIndexX = FMath::FloorToInt(ActorLocation.X / GridSize);
	GridIndexY = FMath::FloorToInt(ActorLocation.Y / GridSize);
	GridIndexZ = FMath::FloorToInt(ActorLocation.Z / GridSize);
}

void FPartitionActorDesc::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	FWorldPartitionActorDesc::Serialize(Ar);

	Ar << GridSize << GridIndexX << GridIndexY << GridIndexZ;

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::PartitionActorDescSerializeGridGuid)
	{
		Ar << GridGuid;
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

#endif
