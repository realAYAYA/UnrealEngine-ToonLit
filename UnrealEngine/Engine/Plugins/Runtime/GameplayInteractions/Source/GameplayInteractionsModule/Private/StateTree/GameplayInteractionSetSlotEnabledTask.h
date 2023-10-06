// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionSetSlotEnabledTask.generated.h"

class USmartObjectSubsystem;

USTRUCT()
struct FGameplayInteractionSetSlotEnabledInstanceData
{
	GENERATED_BODY()

	/** Target slot whose tags are modified. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle TargetSlot;

	/** When using OnEnterStateUndoOnExitState, indicates initial enabled state to be restored on ExitState(). */
	UPROPERTY()
	bool bInitialState = false;
};

/**
 * Task to set a Smart Object Slot enabled to disabled.
 * The slot can be enabled or disable for the duration of the task (OnEnterStateUndoOnExitState),
 * or permanently at the beginning or end of the state.
 */
USTRUCT(meta = (DisplayName = "Set Slot Enabled", Category="Gameplay Interactions|Smart Object"))
struct FGameplayInteractionSetSlotEnabledTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	FGameplayInteractionSetSlotEnabledTask();
	
	using FInstanceDataType = FGameplayInteractionSetSlotEnabledInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** When to modify the tags. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EGameplayInteractionTaskModify Modify = EGameplayInteractionTaskModify::OnEnterStateUndoOnExitState;

	/** If true, handle external State Tree stop as a failure. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bHandleExternalStopAsFailure = true;

	/** Whether to enable or disable the slot. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bEnableSlot = true;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
