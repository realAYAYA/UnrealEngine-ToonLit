// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"

/**
* FWorldPartitionHLODUtilities implementation
*/
class FWorldPartitionHLODUtilities : public IWorldPartitionHLODUtilities
{
public:
	virtual TArray<AWorldPartitionHLOD*> CreateHLODActors(FHLODCreationContext& InCreationContext, const FHLODCreationParams& InCreationParams, const TArray<IStreamingGenerationContext::FActorInstance>& InActors, const TArray<const UDataLayerInstance*>& InDataLayerInstances) override;
	virtual uint32 BuildHLOD(AWorldPartitionHLOD* InHLODActor) override;
	virtual TSubclassOf<UHLODBuilder> GetHLODBuilderClass(const UHLODLayer* InHLODLayer) override;
	virtual UHLODBuilderSettings* CreateHLODBuilderSettings(UHLODLayer* InHLODLayer) override;
};
