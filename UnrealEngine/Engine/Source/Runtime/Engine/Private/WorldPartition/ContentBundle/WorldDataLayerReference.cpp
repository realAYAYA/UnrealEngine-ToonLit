// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/WorldDataLayerReference.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartition.h"

FWorldDataLayersReference::FWorldDataLayersReference()
{
	WorldDataLayersVariant.Set<AWorldDataLayers*>(nullptr);
}

FWorldDataLayersReference::~FWorldDataLayersReference()
{
	Reset();
}

FWorldDataLayersReference::FWorldDataLayersReference(UActorDescContainer* Container, FName WorldDataLayerName)
{
	TrySetReference(Container, WorldDataLayerName);
}

FWorldDataLayersReference::FWorldDataLayersReference(const FActorSpawnParameters& SpawnParameters)
{
	AWorldDataLayers* WorldDataLayers = AWorldDataLayers::Create(SpawnParameters);
	WorldDataLayers->SetActorLabel(SpawnParameters.Name.ToString());
	WorldDataLayersVariant.Set<AWorldDataLayers*>(WorldDataLayers);
}

FWorldDataLayersReference::FWorldDataLayersReference(FWorldDataLayersReference&& Other)
	:WorldDataLayersVariant(MoveTemp(Other.WorldDataLayersVariant))
{
	if (Other.WorldDataLayersVariant.IsType<AWorldDataLayers*>())
	{
		Other.WorldDataLayersVariant.Set<AWorldDataLayers*>(nullptr);
	}
}

FWorldDataLayersReference& FWorldDataLayersReference::operator=(FWorldDataLayersReference&& Other)
{
	if (this != &Other)
	{
		WorldDataLayersVariant = MoveTemp(Other.WorldDataLayersVariant);

		if (Other.WorldDataLayersVariant.IsType<AWorldDataLayers*>())
		{
			Other.WorldDataLayersVariant.Set<AWorldDataLayers*>(nullptr);
		}
	}
	
	return *this;
}

const AWorldDataLayers* FWorldDataLayersReference::Get() const
{
	if (WorldDataLayersVariant.IsType<FWorldPartitionReference>())
	{
		const FWorldPartitionReference& WorldDataLayerReference = WorldDataLayersVariant.Get<FWorldPartitionReference>();
		if (WorldDataLayerReference.IsValid())
		{
			return Cast<AWorldDataLayers>(WorldDataLayerReference->GetActor());
		}
		return nullptr;
	}

	return WorldDataLayersVariant.Get<AWorldDataLayers*>();
}

bool FWorldDataLayersReference::TrySetReference(UActorDescContainer* Container, FName WorldDataLayerName)
{
	for (UActorDescContainer::TIterator<> ActorDescIterator(Container); ActorDescIterator; ++ActorDescIterator)
	{
		if (ActorDescIterator->GetActorNativeClass()->IsChildOf<AWorldDataLayers>())
		{
			if (ActorDescIterator->GetActorName() == WorldDataLayerName)
			{
				WorldDataLayersVariant.Emplace<FWorldPartitionReference>(Container, ActorDescIterator->GetGuid());
				return true;
			}
		}
	}

	return false;
}

void FWorldDataLayersReference::Reset()
{
	// An Actor Descriptor might have been created since we created the reference.
	// Try resolving the reference. If it still does note resolve, delete the actor.
	if (WorldDataLayersVariant.IsType<AWorldDataLayers*>())
	{
		AWorldDataLayers* WorldDataLayers = WorldDataLayersVariant.Get<AWorldDataLayers*>();
		if (WorldDataLayers != nullptr)
		{
			if (UWorld* World = WorldDataLayers->GetWorld()) // May be null during Garbage collection
			{
				UWorldPartition* WorldPartition = World->GetWorldPartition();
				FWorldPartitionReference ResolvedReference(WorldPartition, WorldDataLayers->GetActorGuid());
				if (ResolvedReference.IsValid())
				{
					check(WorldDataLayers == ResolvedReference->GetActor());
					WorldDataLayersVariant.Emplace<FWorldPartitionReference>(ResolvedReference);
				}
				else
				{
					World->DestroyActor(WorldDataLayers);
					WorldDataLayersVariant.Set<AWorldDataLayers*>(nullptr);
				}
			}
			
		}
	}

	if (WorldDataLayersVariant.IsType<FWorldPartitionReference>())
	{
		WorldDataLayersVariant.Get<FWorldPartitionReference>() = nullptr;
	}
}

#endif // WITH_EDITOR