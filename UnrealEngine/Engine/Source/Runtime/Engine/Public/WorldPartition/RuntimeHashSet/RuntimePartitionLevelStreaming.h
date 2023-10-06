// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "RuntimePartitionLevelStreaming.generated.h"

UCLASS()
class URuntimePartitionLevelStreaming : public URuntimePartition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual bool SupportsHLODs() const override;
	virtual bool IsValidGrid(FName GridName) const override;
	virtual bool GenerateStreaming(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FCellDesc>& OutRuntimeCellDescs) override;
#endif

#if WITH_EDITORONLY_DATA
	/** Splits actors into different sublevels based on their actor containers (level instance) */
	UPROPERTY(EditAnywhere, Category = RuntimeSettings, Meta = (EditCondition = "!bIsHLODSetup", EditConditionHides, HideEditConditionToggle))
	bool bOneLevelPerActorContainer;
#endif
};