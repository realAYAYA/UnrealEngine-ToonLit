// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionSyncSlotTagTransition.generated.h"

enum class EDataValidationResult : uint8;

class USmartObjectSubsystem;

UENUM()
enum class EGameplayInteractionSyncSlotTransitionState : uint8
{
	WaitingForFromTag,
	WaitingForToTag,
	Completed,
};

USTRUCT()
struct FGameplayInteractionSyncSlotTagTransitionInstanceData
{
	GENERATED_BODY()

	/** The target slot to monitor */
	UPROPERTY(EditAnywhere, Category="Input")
	FSmartObjectSlotHandle TargetSlot;

	/** Smart Object Slot event handle */
	FDelegateHandle OnEventHandle;

	/** Transition monitoring state */
	EGameplayInteractionSyncSlotTransitionState State = EGameplayInteractionSyncSlotTransitionState::WaitingForFromTag;
};

/**
 * Task to monitor transition of a Gameplay Tag on the specified Smart Object slot.
 *
 * First the task will wait until it sees TransitionFromTag tag on the target Smart Object slot.
 * - TransitionEventTag event is sent to the target slot, which is assumed to trigger a transition to a state that sets TransitionToTag on the Smart Object slot.
 *
 * Then the task will wait until it sees TransitionToTag tag on the target Smart Object slot.
 * - TransitionEventTag event is sent to running State Tree, which allows the running State Tree to transition a new state that is now executed in sync with the other tree.
 */
USTRUCT(meta = (DisplayName = "Sync Slot Tag Transition", Category="Gameplay Interactions|Smart Object"))
struct FGameplayInteractionSyncSlotTagTransitionTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	FGameplayInteractionSyncSlotTagTransitionTask();
	
	using FInstanceDataType = FGameplayInteractionSyncSlotTagTransitionInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EDataValidationResult Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Tag to monitor to see if the slot is ready to transition. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag TransitionFromTag;

	/** Tag to monitor to see if the slot has transitioned. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag TransitionToTag;

	/** Event that is sent to target slot when TransitionFromTag is seen, and event that is send to running State Tree when TransitionToTag is seen. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag TransitionEventTag;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
