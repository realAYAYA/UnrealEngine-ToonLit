// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODSourceActorsFromCell.h"

#if WITH_EDITOR
#include "WorldPartition/HLOD/HLODActor.h"
#include "Serialization/ArchiveCrc32.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#endif

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

	const FName LevelStreamingName = FName(*FString::Printf(TEXT("HLODLevelStreaming_%s"), *HLODActor->GetName()));

	UWorldPartitionLevelStreamingDynamic* LevelStreaming = UWorldPartitionLevelStreamingDynamic::LoadInEditor(World, LevelStreamingName, Actors);
	check(LevelStreaming);

	if (!LevelStreaming->GetLoadSucceeded())
	{
		bOutDirty = true;
	}

	return LevelStreaming;
}

uint32 UWorldPartitionHLODSourceActorsFromCell::GetHLODHash(const TArray<FWorldPartitionRuntimeCellObjectMapping>& InSourceActors)
{
	TArray<FWorldPartitionRuntimeCellObjectMapping>& MutableSourceActors(const_cast<TArray<FWorldPartitionRuntimeCellObjectMapping>&>(InSourceActors));

	FArchiveCrc32 Ar;
	for (FWorldPartitionRuntimeCellObjectMapping& Mapping : MutableSourceActors)
	{
		Ar << Mapping;
	}
	
	return Ar.GetCrc();
}

uint32 UWorldPartitionHLODSourceActorsFromCell::GetHLODHash() const
{
	uint32 HLODHash = Super::GetHLODHash();

	// Source Actors
	uint32 SourceActorsHash = GetHLODHash(Actors);
	UE_LOG(LogHLODHash, VeryVerbose, TEXT(" - Source Actors (%d actors) = %x"), Actors.Num(), SourceActorsHash);
	HLODHash = HashCombine(HLODHash, SourceActorsHash);

	return HLODHash;
}

void UWorldPartitionHLODSourceActorsFromCell::SetActors(const TArray<FWorldPartitionRuntimeCellObjectMapping>&& InSourceActors)
{
	Actors = InSourceActors;
}

const TArray<FWorldPartitionRuntimeCellObjectMapping>& UWorldPartitionHLODSourceActorsFromCell::GetActors() const
{
	return Actors;
}

#endif // #if WITH_EDITOR
