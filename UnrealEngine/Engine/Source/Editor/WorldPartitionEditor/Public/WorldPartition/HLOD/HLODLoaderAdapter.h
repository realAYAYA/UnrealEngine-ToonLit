// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/WorldPartitionActorLoaderInterface.h"

class FHLODActorDesc;

class FLoaderAdapterHLOD : public IWorldPartitionActorLoaderInterface::ILoaderAdapter
{
public:
	FLoaderAdapterHLOD(UWorld* InWorld);

protected:
	//~ Begin ILoaderAdapter interface
	virtual void ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const override;
	virtual bool PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const override;
	//~ End ILoaderAdapter interface

private:
	bool ShouldLoadHLOD(const FHLODActorDesc& HLODActorDesc) const;	
};
