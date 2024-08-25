// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/HLODSourceActors.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "HLODSourceActorsFromCell.generated.h"


UCLASS(MinimalAPI)
class UWorldPartitionHLODSourceActorsFromCell : public UWorldPartitionHLODSourceActors
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	ENGINE_API virtual ULevelStreaming* LoadSourceActors(bool& bOutDirty) const override;
	ENGINE_API virtual uint32 GetHLODHash() const override;

	ENGINE_API static uint32 GetHLODHash(const TArray<FWorldPartitionRuntimeCellObjectMapping>& InSourceActors);

	ENGINE_API void SetActors(const TArray<FWorldPartitionRuntimeCellObjectMapping>&& InSourceActors);
	ENGINE_API const TArray<FWorldPartitionRuntimeCellObjectMapping>& GetActors() const;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FWorldPartitionRuntimeCellObjectMapping> Actors;
#endif
};
