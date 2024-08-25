// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartitionEditorHash.generated.h"

UCLASS(Abstract, Config=Engine, Within = WorldPartition, MinimalAPI)
class UWorldPartitionEditorHash : public UObject
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	ENGINE_API virtual void Initialize() PURE_VIRTUAL(UWorldPartitionEditorHash::Initialize, return;);
	ENGINE_API virtual void SetDefaultValues() PURE_VIRTUAL(UWorldPartitionEditorHash::SetDefaultValues, return;);
	ENGINE_API virtual FName GetWorldPartitionEditorName() const PURE_VIRTUAL(UWorldPartitionEditorHash::GetWorldPartitionEditorName, return FName(NAME_None););
	ENGINE_API virtual FBox GetEditorWorldBounds() const  PURE_VIRTUAL(UWorldPartitionEditorHash::GetEditorWorldBounds, return FBox(ForceInit););
	ENGINE_API virtual FBox GetRuntimeWorldBounds() const  PURE_VIRTUAL(UWorldPartitionEditorHash::GetRuntimeWorldBounds, return FBox(ForceInit););
	ENGINE_API virtual FBox GetNonSpatialBounds() const PURE_VIRTUAL(UWorldPartitionEditorHash::GetNonSpatialBounds, return FBox(ForceInit););
	ENGINE_API virtual void Tick(float DeltaSeconds) PURE_VIRTUAL(UWorldPartitionEditorHash::Tick, return;);

	ENGINE_API virtual void HashActor(FWorldPartitionHandle& InActorHandle) PURE_VIRTUAL(UWorldPartitionEditorHash::HashActor, ;);
	ENGINE_API virtual void UnhashActor(FWorldPartitionHandle& InActorHandle) PURE_VIRTUAL(UWorldPartitionEditorHash::UnhashActor, ;);

	/* Struct of optional parameters passed to ForEachIntersectingActor. */
	struct FForEachIntersectingActorParams
	{
		ENGINE_API FForEachIntersectingActorParams();

		/** Should we include spatially loaded actors in the query? */
		bool bIncludeSpatiallyLoadedActors;

		/** Should we include non-spatially loaded actors in the query? */
		bool bIncludeNonSpatiallyLoadedActors;

		/** Optional minimum box to stop searching for actors */
		TOptional<FBox> MinimumBox;

		FForEachIntersectingActorParams& SetIncludeSpatiallyLoadedActors(bool bInIncludeSpatiallyLoadedActors) { bIncludeSpatiallyLoadedActors = bInIncludeSpatiallyLoadedActors; return *this; };
		FForEachIntersectingActorParams& SetIncludeNonSpatiallyLoadedActors(bool bInIncludeNonSpatiallyLoadedActors) { bIncludeNonSpatiallyLoadedActors = bInIncludeNonSpatiallyLoadedActors; return *this; };
		FForEachIntersectingActorParams& SetMinimumBox(const FBox& InMinimumBox) { MinimumBox = InMinimumBox; return *this; };
	};

	UE_DEPRECATED(5.4, "Use ForEachIntersectingActor with FWorldPartitionActorDescInstance")
	ENGINE_API virtual int32 ForEachIntersectingActor(const FBox& Box, TFunctionRef<void(FWorldPartitionActorDesc*)> InOperation, const FForEachIntersectingActorParams& Params = FForEachIntersectingActorParams()) { return 0; }

	ENGINE_API virtual int32 ForEachIntersectingActor(const FBox& Box, TFunctionRef<void(FWorldPartitionActorDescInstance*)> InOperation, const FForEachIntersectingActorParams& Params = FForEachIntersectingActorParams()) PURE_VIRTUAL(UWorldPartitionEditorHash::ForEachIntersectingActor, return 0;);
#endif
};
