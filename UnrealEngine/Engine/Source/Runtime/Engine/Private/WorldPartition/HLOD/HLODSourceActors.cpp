// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODSourceActors.h"

#if WITH_EDITOR
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#endif


#if WITH_EDITOR

static uint32 GetHLODHash(const UHLODLayer* InHLODLayer)
{
	UHLODLayer& HLODLayer = *const_cast<UHLODLayer*>(InHLODLayer);

	uint32 HLODHash;

	HLODHash = GetTypeHash(HLODLayer.GetLayerType());
	UE_LOG(LogHLODHash, VeryVerbose, TEXT(" - LayerType = %d"), HLODHash);

	HLODHash = HashCombine(HLODLayer.GetHLODBuilderSettings()->GetCRC(), HLODHash);
	UE_LOG(LogHLODHash, VeryVerbose, TEXT(" - HLODBuilderSettings = %d"), HLODHash);

	HLODHash = HashCombine(HLODLayer.GetCellSize(), HLODHash);
	UE_LOG(LogHLODHash, VeryVerbose, TEXT(" - CellSize = %d"), HLODHash);

	return HLODHash;
}

#endif // #if WITH_EDITOR

UWorldPartitionHLODSourceActors::UWorldPartitionHLODSourceActors(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

uint32 UWorldPartitionHLODSourceActors::GetHLODHash() const
{
	uint32 HLODHash = 0;
	if (HLODLayer)
	{
		// HLOD Layer
		HLODHash = ::GetHLODHash(HLODLayer);
		UE_LOG(LogHLODHash, VeryVerbose, TEXT(" - HLOD Layer (%s) = %x"), *HLODLayer->GetName(), HLODHash);
	}
	return HLODHash;
}

void UWorldPartitionHLODSourceActors::SetHLODLayer(const UHLODLayer* InHLODLayer)
{
	HLODLayer = InHLODLayer;
}

const UHLODLayer* UWorldPartitionHLODSourceActors::GetHLODLayer() const
{
	return HLODLayer;
}

#endif // #if WITH_EDITOR