// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#if WITH_EDITOR
bool URuntimePartitionPersistent::GenerateStreaming(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FCellDesc>& OutRuntimeCellDescs)
{
	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	TArray<IStreamingGenerationContext::FActorInstance> CellActorInstances;
	if (PopulateCellActorInstances(ActorSetInstances, bIsMainWorldPartition, true, CellActorInstances))
	{
		FCellDesc& CellDesc = OutRuntimeCellDescs.Emplace_GetRef();

		CellDesc.Name = NAME_PersistentLevel;
		CellDesc.bIsSpatiallyLoaded = false;
		CellDesc.ContentBundleID = CellActorInstances[0].ActorSetInstance->ContentBundleID;
		CellDesc.bBlockOnSlowStreaming = bBlockOnSlowStreaming;
		CellDesc.bClientOnlyVisible = bClientOnlyVisible;
		CellDesc.Priority = Priority;
		CellDesc.ActorInstances = CellActorInstances;
	}

	return true;
}
#endif