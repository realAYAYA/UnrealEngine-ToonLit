// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterSpatial.h"

#if WITH_EDITOR
/** Actor loader adapter that will do a spatial query based on a shape */
class ENGINE_API FLoaderAdapterShape : public ILoaderAdapterSpatial
{
public:
	FLoaderAdapterShape(UWorld* InWorld, const FBox& InBoundingBox, const FString& InLabel);

	//~ Begin IWorldPartitionActorLoaderInterface::ILoader interface
	virtual TOptional<FBox> GetBoundingBox() const override;
	virtual TOptional<FString> GetLabel() const override;
	//~ End IWorldPartitionActorLoaderInterface::ILoader interface

protected:
	//~ Begin IWorldPartitionActorLoaderInterface::ILoaderAdapterSpatial interface
	virtual bool Intersect(const FBox& Box) const override;
	//~ End IWorldPartitionActorLoaderInterface::ILoaderAdapterSpatial interface

private:
	FBox BoundingBox;
	FString Label;
};
#endif