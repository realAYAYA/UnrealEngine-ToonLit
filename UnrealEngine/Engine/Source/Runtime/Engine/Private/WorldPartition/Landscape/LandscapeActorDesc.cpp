// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/Landscape/LandscapeActorDesc.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"

void FLandscapeActorDesc::Init(const AActor* InActor)
{
	FPartitionActorDesc::Init(InActor);

	const ALandscapeProxy* LandscapeProxy = CastChecked<ALandscapeProxy>(InActor);
	check(LandscapeProxy);
	GridIndexX = LandscapeProxy->LandscapeSectionOffset.X / (int32)LandscapeProxy->GridSize;
	GridIndexY = LandscapeProxy->LandscapeSectionOffset.Y / (int32)LandscapeProxy->GridSize;
	GridIndexZ = 0;

	const ALandscape* LandscapeActor = LandscapeProxy->GetLandscapeActor();
	if (LandscapeActor)
	{
		LandscapeActorGuid = LandscapeActor->GetActorGuid();
	}
}

void FLandscapeActorDesc::Serialize(FArchive& Ar)
{
	FPartitionActorDesc::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::FLandscapeActorDescFixupGridIndices)
	{
		GridIndexX = (int32)(GridIndexX * GridSize) / (int32)GridSize;
		GridIndexY = (int32)(GridIndexY * GridSize) / (int32)GridSize;
	}

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionLandscapeActorDescSerializeLandscapeActorGuid)
	{
		Ar << LandscapeActorGuid;
	}
}

void FLandscapeActorDesc::Unload()
{
	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(GetActor()))
	{
		LandscapeProxy->ActorDescReferences.Empty();
	}

	FPartitionActorDesc::Unload();
}

bool FLandscapeActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FPartitionActorDesc::Equals(Other))
	{
		const FLandscapeActorDesc* LandscapeActorDesc = (FLandscapeActorDesc*)Other;
		return LandscapeActorGuid == LandscapeActorDesc->LandscapeActorGuid;
	}

	return false;
}

const FGuid& FLandscapeActorDesc::GetSceneOutlinerParent() const
{
	// Landscape can't parent itself
	if (LandscapeActorGuid != GetGuid())
	{
		return LandscapeActorGuid;
	}

	static FGuid NoParent;
	return NoParent;
}

#endif
