// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Containers/Map.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

class ENGINE_API FPartitionActorDesc : public FWorldPartitionActorDesc
{
	friend class FPartitionActorDescFactory;

public:
	uint32 GridSize;
	int64 GridIndexX;
	int64 GridIndexY;
	int64 GridIndexZ;
	FGuid GridGuid;
protected:
	virtual void Init(const AActor* InActor) override;
	virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;
	virtual void Serialize(FArchive& Ar) override;
};
#endif
