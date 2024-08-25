// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartitionLevelStreaming.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#if WITH_EDITOR
bool URuntimePartitionLevelStreaming::IsValidPartitionTokens(const TArray<FName>& InPartitionTokens) const
{
	return InPartitionTokens.Num() && (InPartitionTokens.Num() <= 2);
}

bool URuntimePartitionLevelStreaming::GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult)
{
	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	TMap<FName, TArray<const IStreamingGenerationContext::FActorSetInstance*>> CellsActorSetInstances;
	for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : *InParams.ActorSetInstances)
	{
		TArray<FName> ActorSetGridNameList;

		FName LevelName = NAME_Default;
		if (!ActorSetInstance->RuntimeGrid.IsNone())
		{
			TArray<FName> MainPartitionTokens;
			TArray<FName> HLODPartitionTokens;
			if (UWorldPartitionRuntimeHashSet::ParseGridName(ActorSetInstance->RuntimeGrid, MainPartitionTokens, HLODPartitionTokens))
			{
				if (MainPartitionTokens.Num() == 2)
				{
					LevelName = MainPartitionTokens[1];
				}
			}
		}
		ActorSetGridNameList.Add(LevelName);

		TStringBuilder<512> StringBuilder;
		StringBuilder += Name.ToString();
		StringBuilder += TEXT("_");
		StringBuilder += LevelName.ToString();
		FName CellName = *StringBuilder;

		CellsActorSetInstances.FindOrAdd(CellName).Add(ActorSetInstance);
	}

	for (auto& [CellName, CellActorSetInstances] : CellsActorSetInstances)
	{
		OutResult.RuntimeCellDescs.Emplace(CreateCellDesc(CellName.ToString(), true, 0, CellActorSetInstances));
	}

	return true;
}
#endif