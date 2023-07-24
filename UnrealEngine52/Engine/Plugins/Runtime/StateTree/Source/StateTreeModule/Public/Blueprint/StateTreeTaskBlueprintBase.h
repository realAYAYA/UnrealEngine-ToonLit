// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "StateTreeTaskBase.h"
#include "StateTreeNodeBlueprintBase.h"
#include "StateTreeTaskBlueprintBase.generated.h"

struct FStateTreeExecutionContext;

/*
 * Base class for Blueprint based Tasks. 
 */
UCLASS(Abstract, Blueprintable)
class STATETREEMODULE_API UStateTreeTaskBlueprintBase : public UStateTreeNodeBlueprintBase
{
	GENERATED_BODY()
public:
	UStateTreeTaskBlueprintBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "EnterState"))
	EStateTreeRunStatus ReceiveEnterState(const FStateTreeTransitionResult& Transition);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "ExitState"))
	void ReceiveExitState(const FStateTreeTransitionResult& Transition);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "StateCompleted"))
	void ReceiveStateCompleted(const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates CompletedActiveStates);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Tick"))
	EStateTreeRunStatus ReceiveTick(const float DeltaTime);

protected:
	/**
	 * Note: The API has been deprecated. ChangeType is moved into FStateTreeTransitionResult.
	 * You can configure the task to be only called on state changes (that is, never call sustained changes) by setting bShouldStateChangeOnReselect to true.
	 */
	UE_DEPRECATED(5.2, "Use EnterState without ChangeType instead.")
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) final { return EStateTreeRunStatus::Running; }
	UE_DEPRECATED(5.2, "Use ExitState without ChangeType instead.")
	virtual void ExitState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) final {};

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition);
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition);

	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates);
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime);

	/**
	 * If set to true, the task will receive EnterState/ExitState even if the state was previously active.
	 * Generally this should be true for action type tasks, like playing animation,
	 * and false on state like tasks like claiming a resource that is expected to be acquired on child states. */
	UPROPERTY(EditDefaultsOnly, Category="Default")
	uint8 bShouldStateChangeOnReselect : 1;

	/**
	 * If set to true, Tick() is called. Not ticking implies no property copy. Default true.
	 * Note: this is intentionally not a property, should be only set by C++ derived classes when the tick should not be called.
	 */
	uint8 bShouldCallTick : 1;

	/** If set to true, Tick() is called. Default false. */
	UPROPERTY(EditDefaultsOnly, Category="Default")
	uint8 bShouldCallTickOnlyOnEvents : 1;

	/** If set to true, copy the values of bound properties before calling Tick(). Default true. */
	UPROPERTY(EditDefaultsOnly, Category="Default")
	uint8 bShouldCopyBoundPropertiesOnTick : 1;
	
	/** If set to true, copy the values of bound properties before calling ExitState(). Default true. */
	UPROPERTY(EditDefaultsOnly, Category="Default")
	uint8 bShouldCopyBoundPropertiesOnExitState : 1;
	
	uint8 bHasEnterState : 1;
	uint8 bHasExitState : 1;
	uint8 bHasStateCompleted : 1;
	uint8 bHasTick : 1;

	friend struct FStateTreeBlueprintTaskWrapper;
};

/**
 * Wrapper for Blueprint based Tasks.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeBlueprintTaskWrapper : public FStateTreeTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return TaskClass; };
	virtual EDataValidationResult Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	UPROPERTY()
	TSubclassOf<UStateTreeTaskBlueprintBase> TaskClass = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "StateTreeEvents.h"
#endif
