// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"

/**
 * ActorDesc for WaterZone actors.
 */
class FWaterZoneActorDesc : public FWorldPartitionActorDesc
{
public:
	virtual void Init(const AActor* InActor) override;
	virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;

	int32 GetOverlapPriority() const { return OverlapPriority; }
protected:
	virtual void Serialize(FArchive& Ar) override;

	int32 OverlapPriority;
};
#endif
