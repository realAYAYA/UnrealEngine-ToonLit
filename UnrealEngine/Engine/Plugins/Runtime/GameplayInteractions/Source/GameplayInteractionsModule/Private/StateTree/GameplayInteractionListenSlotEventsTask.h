// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionListenSlotEventsTask.generated.h"

class USmartObjectSubsystem;

USTRUCT()
struct FGameplayInteractionListenSlotEventsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle TargetSlot;

	FDelegateHandle OnEventHandle;
};

/**
 * Task to listen Smart Object slot events on a specified slot.
 * Any event sent to the specified Smart Object slot will be translated to a State Tree event.
 */
USTRUCT(meta = (DisplayName = "Listen Slot Events", Category="Gameplay Interactions|Smart Object"))
struct FGameplayInteractionListenSlotEventsTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	FGameplayInteractionListenSlotEventsTask();
	
	using FInstanceDataType = FGameplayInteractionListenSlotEventsTaskInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
