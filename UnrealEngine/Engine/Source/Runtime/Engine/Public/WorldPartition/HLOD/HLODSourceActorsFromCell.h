// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/HLODSourceActors.h"
#include "WorldPartition/HLOD/HLODSubActor.h"
#include "HLODSourceActorsFromCell.generated.h"


UCLASS(MinimalAPI)
class UWorldPartitionHLODSourceActorsFromCell : public UWorldPartitionHLODSourceActors
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	ENGINE_API virtual ULevelStreaming* LoadSourceActors(bool& bOutDirty) const override;
	ENGINE_API virtual uint32 GetHLODHash() const override;

	ENGINE_API void SetActors(const TArray<FHLODSubActor>& InSourceActors);
	ENGINE_API const TArray<FHLODSubActor>& GetActors() const;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FHLODSubActor> Actors;
#endif
};
