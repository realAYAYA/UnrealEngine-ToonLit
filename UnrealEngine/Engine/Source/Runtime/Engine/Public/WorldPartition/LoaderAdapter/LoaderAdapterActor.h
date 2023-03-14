// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterSpatial.h"

#if WITH_EDITOR
/** Actor loader adapter that will do a spatial query based on the actor's brush  */
class ENGINE_API FLoaderAdapterActor : public ILoaderAdapterSpatial
{
public:
	FLoaderAdapterActor(AActor* InActor);

	//~ Begin IWorldPartitionActorLoaderInterface::ILoader interface
	virtual TOptional<FBox> GetBoundingBox() const override;
	virtual TOptional<FString> GetLabel() const override;
	//~ End IWorldPartitionActorLoaderInterface::ILoader interface

protected:
	//~ Begin IWorldPartitionActorLoaderInterface::ILoaderAdapterSpatial interface
	virtual bool Intersect(const FBox& Box) const override;
	//~ End IWorldPartitionActorLoaderInterface::ILoaderAdapterSpatial interface

	AActor* Actor;
};
#endif
