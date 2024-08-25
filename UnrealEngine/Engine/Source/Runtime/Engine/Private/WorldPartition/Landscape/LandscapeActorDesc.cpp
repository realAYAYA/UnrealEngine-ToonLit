// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/Landscape/LandscapeActorDesc.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

void FLandscapeActorDesc::Init(const AActor* InActor)
{
	FPartitionActorDesc::Init(InActor);

	const ALandscapeProxy* LandscapeProxy = CastChecked<ALandscapeProxy>(InActor);
	check(LandscapeProxy);
	SetGridIndices(LandscapeProxy->LandscapeSectionOffset.X, LandscapeProxy->LandscapeSectionOffset.Y, 0);

	if (!bIsDefaultActorDesc)
	{
		const ALandscape* LandscapeActor = LandscapeProxy->GetLandscapeActor();
		if (LandscapeActor)
		{
			LandscapeActorGuid = LandscapeActor->GetActorGuid();
		}
	}
}

void FLandscapeActorDesc::Serialize(FArchive& Ar)
{
	FPartitionActorDesc::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	if (!bIsDefaultActorDesc)
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::FLandscapeActorDescFixupGridIndices)
		{
			SetGridIndices(GridIndexX * GridSize, GridIndexY * GridSize, 0);
		}

		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::WorldPartitionLandscapeActorDescSerializeLandscapeActorGuid)
		{
			Ar << LandscapeActorGuid;
		}
	}
}

FBox FLandscapeActorDesc::GetEditorBounds() const
{
	// We need to skip super class since we aren't using grid indices as it should be (it's in Landscape space).
	return FWorldPartitionActorDesc::GetEditorBounds();
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FLandscapeActorDesc::OnUnloadingInstance(const FWorldPartitionActorDescInstance* InActorDescInstance) const
{
	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActorDescInstance->GetActor()))
	{
		LandscapeProxy->ActorDescReferences.Empty();
	}

	FPartitionActorDesc::OnUnloadingInstance(InActorDescInstance);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

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
