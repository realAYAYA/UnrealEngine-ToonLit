// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"

class UDataLayerInstance;

#if WITH_EDITOR
class ENGINE_API IStreamingGenerationContext
{
public:
	struct ENGINE_API FActorSet
	{
		TArray<FGuid> Actors;
	};

	struct ENGINE_API FActorSetContainer
	{
		const FActorDescViewMap* ActorDescViewMap;
		const UActorDescContainer* ActorDescContainer;
		TArray<FActorSet> ActorSets;
	};

	struct ENGINE_API FActorSetInstance
	{
		FBox Bounds;
		FName RuntimeGrid;
		bool bIsSpatiallyLoaded;
		TArray<const UDataLayerInstance*> DataLayers;
		FGuid ContentBundleID;
		const FActorSetContainer* ContainerInstance;
		FActorContainerID ContainerID;
		FTransform Transform;
		const FActorSet* ActorSet;
	};

	struct ENGINE_API FActorInstance
	{
		FActorInstance(const FGuid& InActorGuid, const FActorSetInstance* InActorSetInstance)
			: ActorGuid(InActorGuid)
			, ActorSetInstance(InActorSetInstance)
		{}

		FGuid ActorGuid;
		const FActorSetInstance* ActorSetInstance;

		const FWorldPartitionActorDescView& GetActorDescView() const;
		const FActorContainerID& GetContainerID() const;
		const FTransform& GetTransform() const;
		const UActorDescContainer* GetActorDescContainer() const;
	};

	virtual FBox GetWorldBounds() const = 0;
	virtual const FActorSetContainer* GetMainWorldContainer() const = 0;
	virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const = 0;
};
#endif