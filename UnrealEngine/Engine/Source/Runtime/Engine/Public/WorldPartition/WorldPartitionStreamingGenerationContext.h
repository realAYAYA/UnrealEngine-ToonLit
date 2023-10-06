// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OverrideVoidReturnInvoker.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"

class FActorDescViewMap;
class FStreamingGenerationActorDescCollection;
class FWorldPartitionActorDescView;
class UActorDescContainer;
class UDataLayerInstance;

#if WITH_EDITOR
class IStreamingGenerationContext
{
public:
	struct FActorSet
	{
		TArray<FGuid> Actors;
	};

	struct FActorSetContainer
	{
		FActorSetContainer()
			: ActorDescViewMap(nullptr)
			, ActorDescCollection(nullptr)
		{}

		// Non-copyable
		FActorSetContainer(const FActorSetContainer&) = delete;
		FActorSetContainer& operator=(const FActorSetContainer&) = delete;

		const FActorDescViewMap* ActorDescViewMap;
		const FStreamingGenerationActorDescCollection* ActorDescCollection;
		TArray<TUniquePtr<FActorSet>> ActorSets;
	};

	struct FActorSetInstance
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
		const TSet<FGuid>* FilteredActors;

		template <typename Func>
		void ForEachActor(Func InFunc) const
		{
			TOverrideVoidReturnInvoker Invoker(true, InFunc);

			for (const FGuid& ActorGuid : ActorSet->Actors)
			{
				if (!FilteredActors || !FilteredActors->Contains(ActorGuid))
				{
					if (!Invoker(ActorGuid))
					{
						break;
					}
				}
			}
		}
	};

	struct FActorInstance
	{
		FActorInstance(const FGuid& InActorGuid, const FActorSetInstance* InActorSetInstance)
			: ActorGuid(InActorGuid)
			, ActorSetInstance(InActorSetInstance)
		{}

		FGuid ActorGuid;
		const FActorSetInstance* ActorSetInstance;

		ENGINE_API const FWorldPartitionActorDescView& GetActorDescView() const;
		ENGINE_API const FActorContainerID& GetContainerID() const;
		ENGINE_API const FTransform& GetTransform() const;
		ENGINE_API const UActorDescContainer* GetActorDescContainer() const;
	};

	virtual FBox GetWorldBounds() const = 0;
	virtual const FActorSetContainer* GetMainWorldContainer() const = 0;
	virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const = 0;
	virtual void ForEachActorSetContainer(TFunctionRef<void(const FActorSetContainer&)> Func) const = 0;
};
#endif
