// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"

#if WITH_EDITOR
/** Base class for actor loaders that contains a specific list of actors */
class ENGINE_API FLoaderAdapterList : public IWorldPartitionActorLoaderInterface::ILoaderAdapter
{
public:
	FLoaderAdapterList(UWorld* InWorld);
	virtual ~FLoaderAdapterList() {}

protected:
	//~ Begin ILoaderAdapter interface
	virtual void ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const override;
	//~ End ILoaderAdapter interface

	TSet<FWorldPartitionHandle> Actors;
};
#endif