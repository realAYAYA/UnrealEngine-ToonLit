// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "Engine/DataAsset.h"
#include "Types/TargetingSystemTypes.h"

#include "TargetingPreset.generated.h"

class UTargetingTask;
struct FTargetingRequestData;
struct FTargetingDefaultResultsSet;


/**
*	@class UTargetingPreset
*
*	This object is used when a data asset is preferred for task setup.
*
*	Tasks will be processed in the order they are setup in the task set. It
*	is recommended that selection tasks happen first before any filtering
*	and sorting.
*/
UCLASS(BlueprintType)
class TARGETINGSYSTEM_API UTargetingPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UTargetingPreset(const FObjectInitializer& ObjectInitializer);

	/** Get the set of tasks defined for this targeting preset */
	FORCEINLINE const FTargetingTaskSet* GetTargetingTaskSet() const { return &TargetingTaskSet; }
	FORCEINLINE FTargetingTaskSet& GetMutableTargetingTaskSet() { return TargetingTaskSet; }

	/** The tasks that make up this targeting preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ShowOnlyInnerProperties))
	FTargetingTaskSet TargetingTaskSet;
};