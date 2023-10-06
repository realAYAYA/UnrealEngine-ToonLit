// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/HLODSourceActors.h"
#include "HLODSourceActorsFromLevel.generated.h"


UCLASS(MinimalAPI)
class UWorldPartitionHLODSourceActorsFromLevel : public UWorldPartitionHLODSourceActors
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	ENGINE_API virtual ULevelStreaming* LoadSourceActors(bool& bOutDirty) const override;
	ENGINE_API virtual uint32 GetHLODHash() const override;

	ENGINE_API void SetSourceLevel(const UWorld* InSourceLevel);
	ENGINE_API const TSoftObjectPtr<UWorld>& GetSourceLevel() const;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSoftObjectPtr<UWorld> SourceLevel;
#endif
};
