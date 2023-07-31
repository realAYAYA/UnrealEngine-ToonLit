// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"

#include "Engine/World.h"

#include "WorldPartitionRuntimeCellOwner.generated.h"

class UWorld;
class UWorldPartition;

UINTERFACE()
class ENGINE_API UWorldPartitionRuntimeCellOwner : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IWorldPartitionRuntimeCellOwner
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual UWorld* GetOwningWorld() const = 0;
	virtual UWorld* GetOuterWorld() const = 0;

	UWorldPartition* GetWorldPartition() const { return GetOuterWorld()->GetWorldPartition(); }
};
