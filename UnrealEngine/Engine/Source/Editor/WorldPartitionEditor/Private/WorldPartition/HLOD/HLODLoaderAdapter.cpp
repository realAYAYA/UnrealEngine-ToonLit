// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODLoaderAdapter.h"

#include "Engine/World.h"

#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLayer.h"

#include "WorldPartition/ActorDescContainerInstanceCollection.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"


FLoaderAdapterHLOD::FLoaderAdapterHLOD(UWorld* InWorld)
	: ILoaderAdapter(InWorld)
{
	Load();
}

void FLoaderAdapterHLOD::ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const
{
	UWorldPartition* WorldPartition = GetWorld()->GetWorldPartition();

	for (FActorDescContainerInstanceCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition); HLODIterator; ++HLODIterator)
	{
		const FHLODActorDesc& HLODActorDesc = *(FHLODActorDesc*)HLODIterator->GetActorDesc();
		if (ShouldLoadHLOD(HLODActorDesc))
		{
			InOperation(FWorldPartitionHandle(WorldPartition, HLODActorDesc.GetGuid()));
		}
	}
}

// TODO - How to choose which HLODs are loaded ?
// * Always loaded should be loaded...
// * Should be a user setting/bookmark setting maybe?
// * World settings? 
// * HLOD Layer setting?
//
// Current solution is to load always loaded HLOD + any HLOD not built from instancing
bool FLoaderAdapterHLOD::ShouldLoadHLOD(const FHLODActorDesc& HLODActorDesc) const
{
	FSoftObjectPath HLODLayerPath(HLODActorDesc.GetSourceHLODLayer());
	if (UHLODLayer* HLODLayer = Cast<UHLODLayer>(HLODLayerPath.TryLoad()))
	{
		if (!HLODActorDesc.GetIsSpatiallyLoaded() || HLODLayer->GetLayerType() != EHLODLayerType::Instancing)
		{
			return true;
		}
	}

	return false;
}

bool FLoaderAdapterHLOD::PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const
{
	// Avoid the base class implementation which will skip actors which are not editor relevant
	return true;
}
