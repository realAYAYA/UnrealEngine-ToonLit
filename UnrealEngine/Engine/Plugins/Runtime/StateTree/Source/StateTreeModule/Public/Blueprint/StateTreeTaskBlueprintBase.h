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

	/**
	 * Called when a new state is entered and task is part of active states.
	 * Use FinishTask() to set the task execution completed. State completion is controlled by completed tasks.
	 *
	 * GameplayTasks and other latent actions should be generally triggered on EnterState. When using a GameplayTasks it's required
	 * to manually cancel active tasks on ExitState if the GameplayTask's lifetime is tied to the State Tree task.
	 *
	 * @param Transition Describes the states involved in the transition
	 */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "EnterState"))
	void ReceiveLatentEnterState(const FStateTreeTransitionResult& Transition);

	/**
	 * Called when a current state is exited and task is part of active states.
	 * @param Transition Describes the states involved in the transition
	 */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "ExitState"))
	void ReceiveExitState(const FStateTreeTransitionResult& Transition);

	/**
	 * Called right after a state has been completed, but before new state has been selected. StateCompleted is called in reverse order to allow to propagate state to other Tasks that
	 * are executed earlier in the tree. Note that StateCompleted is not called if conditional transition changes the state.
	 * @param CompletionStatus Describes the running status of the completed state (Succeeded/Failed).
	 * @param CompletedActiveStates Active states at the time of completion.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "StateCompleted"))
	void ReceiveStateCompleted(const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates CompletedActiveStates);

	/**
	 * Called during state tree tick when the task is on active state.
	 * Use FinishTask() to set the task execution completed. State completion is controlled by completed tasks.
	 *
	 * Triggering GameplayTasks and other latent action should generally be done on EnterState. Tick is called on each update (or event)
	 * and can cause huge amount of task added if the logic is not handled carefully.
	 * Tick should be generally be used for monitoring that require polling, or actions that require constant ticking.  
	 *
	 * Note: The method is called only if bShouldCallTick or bShouldCallTickOnlyOnEvents is set.
	 * @param DeltaTime Time since last StateTree tick.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Tick"))
	void ReceiveLatentTick(const float DeltaTime);

	UE_DEPRECATED(5.3, "Use the new EnterState event without without return value instead. Task status is now controlled via FinishTask node, instead of a return value. Default status is running.")
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "EnterState (Deprecated)", DeprecatedFunction, DeprecationMessage="Use the new EnterState event without without return value instead. Task status is now controlled via FinishTask node, instead of a return value. Default status is running."))
	EStateTreeRunStatus ReceiveEnterState(const FStateTreeTransitionResult& Transition);

	UE_DEPRECATED(5.3, "Use the new Tick event without without return value instead. Task status is now controlled via FinishTask node, instead of a return value. Default status is running.")
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Tick (Deprecated)", DeprecatedFunction, DeprecationMessage="Use the new Tick event without without return value instead. Task status is now controlled via FinishTask node, instead of a return value. Default status is running."))
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

	/** Finish the task and sets it's status. */
	UFUNCTION(BlueprintCallable, Category = "StateTree", meta = (HideSelfPin = "true", DisplayName = "Finish Task"))
	void FinishTask(const bool bSucceeded = true)
	{
		RunStatus = bSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
	}

	/** Run status when using latent EnterState and Tick */
	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Running;
	
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
	
	uint8 bHasExitState : 1;
	uint8 bHasStateCompleted : 1;
	uint8 bHasLatentEnterState : 1;
	uint8 bHasLatentTick : 1;
	UE_DEPRECATED(5.3, "Use bHasLatentEnterState instead.")
	uint8 bHasEnterState_DEPRECATED : 1;
	UE_DEPRECATED(5.3, "Use bHasLatentTick instead.")
	uint8 bHasTick_DEPRECATED : 1;

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
