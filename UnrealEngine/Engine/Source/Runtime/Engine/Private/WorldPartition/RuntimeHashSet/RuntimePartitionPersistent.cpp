// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#if WITH_EDITOR
bool URuntimePartitionPersistent::GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult)
{
	const FString CellName(TEXT("Persistent"));
	OutResult.RuntimeCellDescs.Emplace(CreateCellDesc(CellName, false, 0, *InParams.ActorSetInstances));
	return true;
}
#endif