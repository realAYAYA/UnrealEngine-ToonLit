// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayInteractionsTypes.h"
#include "GameplayInteractionModifySlotTagTask.generated.h"

enum class EDataValidationResult : uint8;

class USmartObjectSubsystem;

USTRUCT()
struct FGameplayInteractionModifySlotTagTaskInstanceData
{
	GENERATED_BODY()

	/** Target slot whose tags are modified. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle TargetSlot;

	/** When using OnEnterStateUndoOnExitState, indicates if a tag was removed, and whether is should be restored on ExitState(). */
	UPROPERTY()
	bool bTagRemoved = false;
};

/**
 * Task to modify Smart Object Slot tags.
 * The tags can be added or removed, and they can be modified for the duration of the task (OnEnterStateUndoOnExitState),
 * or permanently modified at the beginning or end of the state.
 */
USTRUCT(meta = (DisplayName = "Modify Slot Tag", Category="Gameplay Interactions|Smart Object"))
struct FGameplayInteractionModifySlotTagTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	FGameplayInteractionModifySlotTagTask();
	
	using FInstanceDataType = FGameplayInteractionModifySlotTagTaskInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EDataValidationResult Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** When to modify the tags. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EGameplayInteractionTaskModify Modify = EGameplayInteractionTaskModify::OnEnterStateUndoOnExitState;

	/** If true, handle external State Tree stop as a failure. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bHandleExternalStopAsFailure = true;

	/** How to modify the tags. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EGameplayInteractionModifyGameplayTagOperation Operation = EGameplayInteractionModifyGameplayTagOperation::Add;
	
	/** Tag to add or remove. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;

	/** Handle to retrieve USmartObjectSubsystem. */
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
};
