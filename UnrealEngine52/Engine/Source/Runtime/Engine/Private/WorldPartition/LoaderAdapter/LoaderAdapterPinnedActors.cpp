// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/LoaderAdapter/LoaderAdapterPinnedActors.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "Engine/Level.h"
#include "WorldPartition/WorldPartition.h"

#define LOCTEXT_NAMESPACE "FLoaderAdapterPinnedActors"

bool FLoaderAdapterPinnedActors::PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const
{
	// We want to be able to pin any type of actors (HLODs, etc).
	return ActorHandle.IsValid() && !ActorsToRemove.Contains(ActorHandle);
}

FText FLoaderAdapterPinnedActors::GetUnloadedReason(FWorldPartitionActorDesc* InActorDesc)
{
	if (InActorDesc)
	{
		UActorDescContainer* ActorDescContainer = InActorDesc->GetContainer();
		UWorld* World = ActorDescContainer != nullptr ? ActorDescContainer->GetWorld() : nullptr;
		UWorldPartition* WorldPartition = World != nullptr ? World->GetWorldPartition() : nullptr;
		bool bShouldBeLoaded = !InActorDesc->GetIsSpatiallyLoaded() && !InActorDesc->GetActorIsRuntimeOnly();
		if (WorldPartition && (bShouldBeLoaded || WorldPartition->IsActorPinned(InActorDesc->GetGuid())))
		{
			return LOCTEXT("UnloadedDataLayerReason", "Unloaded DataLayer");
		}
	}
	
	return LOCTEXT("UnloadedReason", "Unloaded");
}

bool FLoaderAdapterPinnedActors::SupportsPinning(FWorldPartitionActorDesc* InActorDesc)
{
	if (!InActorDesc)
	{
		return false;
	}

	// Only Spatially loaded actors can be pinned with the exception of non spatially loaded, runtime only actors (ex: HLODs)
	if (!InActorDesc->GetIsSpatiallyLoaded() && !InActorDesc->GetActorIsRuntimeOnly())
	{
		return false;
	}

	if (UActorDescContainer* Container = InActorDesc->GetContainer())
	{
		if (Container->IsMainPartitionContainer())
		{
			return true;
		}
		else if (InActorDesc->GetContentBundleGuid().IsValid())
		{
			const UWorld* ContainerWorld = Container->GetWorld();
			const UWorldPartition* ContainerWorldPartition = ContainerWorld ? ContainerWorld->GetWorldPartition() : nullptr;
			if (ContainerWorldPartition && ContainerWorldPartition->IsMainWorldPartition())
			{
				return InActorDesc->GetActorSoftPath().GetAssetPath().GetPackageName() == ContainerWorld->GetPackage()->GetFName();
			}
		}
	}

	return false;
}

bool FLoaderAdapterPinnedActors::SupportsPinning(AActor* InActor)
{
	if (InActor)
	{
		// Pinning of Actors is only supported on the main world partition
		const ULevel* Level = InActor->GetLevel();
		const UWorld* World = Level->GetWorld();

		// Only Spatially loaded actors can be pinned with the exception of non spatially loaded, runtime only actors (ex: HLODs)
		return World && !World->IsGameWorld() && !!World->GetWorldPartition() && Level->IsPersistentLevel() && InActor->IsPackageExternal() && (InActor->GetIsSpatiallyLoaded() || InActor->IsRuntimeOnly());
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

#endif
