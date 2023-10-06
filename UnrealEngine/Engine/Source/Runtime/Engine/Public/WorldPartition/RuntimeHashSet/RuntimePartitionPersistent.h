// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "RuntimePartitionPersistent.generated.h"

UCLASS(HideDropdown)
class URuntimePartitionPersistent : public URuntimePartition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual bool SupportsHLODs() const override { return false; }
	virtual bool IsValidGrid(FName GridName) const override { return true; }
	virtual bool GenerateStreaming(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FCellDesc>& OutRuntimeCellDescs) override;
#endif
};
