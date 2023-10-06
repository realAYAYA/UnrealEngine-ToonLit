// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODSourceActorsFromCell.h"

#if WITH_EDITOR
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#endif


#if WITH_EDITOR

static uint32 GetHLODHash(const TArray<FHLODSubActor>& InSubActors)
{
	uint32 HLODHash = 0;

	for (const FHLODSubActor& HLODSubActor : InSubActors)
	{
		HLODHash = HashCombine(GetTypeHash(HLODSubActor), HLODHash);
	}

	return HLODHash;
}

#endif // #if WITH_EDITOR

UWorldPartitionHLODSourceActorsFromCell::UWorldPartitionHLODSourceActorsFromCell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

ULevelStreaming* UWorldPartitionHLODSourceActorsFromCell::LoadSourceActors(bool& bOutDirty) const
{
	UPackage::WaitForAsyncFileWrites();

	bOutDirty = false;
	AWorldPartitionHLOD* HLODActor = CastChecked<AWorldPartitionHLOD>(GetOuter());
	UWorld* World = HLODActor->GetWorld();
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	check(WorldPartition);

	const FName LevelStreamingName = FName(*FString::Printf(TEXT("HLODLevelStreaming_%s"), *HLODActor->GetName()));
	TArray<FWorldPartitionRuntimeCellObjectMapping> Mappings;
	Mappings.Reserve(Actors.Num());
	Algo::Transform(Actors, Mappings, [World, HLODActor](const FHLODSubActor& SubActor)
	{
		return FWorldPartitionRuntimeCellObjectMapping(
			SubActor.ActorPackage,
			SubActor.ActorPath,
			SubActor.ContainerID,
			SubActor.ContainerTransform,
			SubActor.ContainerPackage,
			World->GetPackage()->GetFName(),
			SubActor.ActorGuid,
			false
		);
	});

	UWorldPartitionLevelStreamingDynamic* LevelStreaming = UWorldPartitionLevelStreamingDynamic::LoadInEditor(World, LevelStreamingName, Mappings);
	check(LevelStreaming);

	if (!LevelStreaming->GetLoadSucceeded())
	{
		bOutDirty = true;
	}

	return LevelStreaming;
}

uint32 UWorldPartitionHLODSourceActorsFromCell::GetHLODHash() const
{
	uint32 HLODHash = Super::GetHLODHash();

	// Sub Actors
	uint32 SubActorsHash = ::GetHLODHash(Actors);
	UE_LOG(LogHLODHash, VeryVerbose, TEXT(" - Sub Actors (%d actors) = %x"), Actors.Num(), SubActorsHash);
	HLODHash = HashCombine(HLODHash, SubActorsHash);

	return HLODHash;
}

void UWorldPartitionHLODSourceActorsFromCell::SetActors(const TArray<FHLODSubActor>& InSourceActors)
{
	Actors = InSourceActors;
}

const TArray<FHLODSubActor>& UWorldPartitionHLODSourceActorsFromCell::GetActors() const
{
	return Actors;
}

#endif // #if WITH_EDITOR
