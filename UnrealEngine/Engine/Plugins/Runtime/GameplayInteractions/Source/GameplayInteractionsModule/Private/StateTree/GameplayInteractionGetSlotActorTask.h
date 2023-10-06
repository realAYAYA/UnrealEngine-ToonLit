// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionGetSlotActorTask.generated.h"

class USmartObjectSubsystem;

USTRUCT()
struct FGameplayInteractionGetSlotActorTaskInstanceData
{
	GENERATED_BODY()

	/** Target slot to get the Actor from. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle TargetSlot;

	/** Actor in the specified target slot, or empty if target slot is not valid or there is not Actor present. */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> ResultActor;
};

/**
 * Task to get an Actor on a specified Smart Object slot. 
 */
USTRUCT(meta = (DisplayName = "Get Slot Actor", Category="Gameplay Interactions|Smart Object"))
struct FGameplayInteractionGetSlotActorTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	FGameplayInteractionGetSlotActorTask();
	
	using FInstanceDataType = FGameplayInteractionGetSlotActorTaskInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** If true, and no valid actor is found, the task will fail. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bFailIfNotFound = true;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
