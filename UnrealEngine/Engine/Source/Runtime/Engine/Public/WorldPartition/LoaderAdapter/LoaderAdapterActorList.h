// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterList.h"

/** Loader adapter that contains an actor list. */
class ENGINE_API FLoaderAdapterActorList: public FLoaderAdapterList
{
public:
	FLoaderAdapterActorList(UWorld* InWorld);

	void AddActors(const TArray<FGuid>& ActorGuids);
	void AddActors(const TArray<FWorldPartitionHandle>& ActorHandles);

	void RemoveActors(const TArray<FGuid>& ActorGuids);
	void RemoveActors(const TArray<FWorldPartitionHandle>& ActorHandles);

	bool ContainsActor(const FGuid& ActorGuid) const;
	bool ContainsActor(const FWorldPartitionHandle& ActorHandle) const;

protected:
	//~ Begin IWorldPartitionActorLoaderInterface::ILoaderAdapterList interface
	virtual bool ShouldActorBeLoaded(const FWorldPartitionHandle& ActorHandle) const override;
	//~ End IWorldPartitionActorLoaderInterface::ILoaderAdapterList interface

private:
	TSet<FWorldPartitionHandle> ActorsToRemove;
};
#endif