// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "Actions/PawnAction.h"
#include "GameFramework/Pawn.h"
#include "PawnActionsComponent.generated.h"

class AController;

USTRUCT()
struct AIMODULE_API FPawnActionEvent
{
	GENERATED_USTRUCT_BODY()

	// used for marking FPawnActionEvent instances created solely for comparisons uses
	static const int32 FakeActionIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UDEPRECATED_PawnAction> Action_DEPRECATED;

	EPawnActionEventType::Type EventType;

	EAIRequestPriority::Type Priority;

	// used to maintain order of equally-important messages
	uint32 Index;

	FPawnActionEvent() : Action_DEPRECATED(NULL), EventType(EPawnActionEventType::Invalid), Priority(EAIRequestPriority::MAX), Index(uint32(-1))
	{}

	FPawnActionEvent(UDEPRECATED_PawnAction& Action, EPawnActionEventType::Type EventType, uint32 Index);

	bool operator==(const FPawnActionEvent& Other) const { return (Action_DEPRECATED == Other.Action_DEPRECATED) && (EventType == Other.EventType) && (Priority == Other.Priority); }
};

USTRUCT()
struct AIMODULE_API FPawnActionStack
{
	GENERATED_USTRUCT_BODY()

	FPawnActionStack()
		: TopAction_DEPRECATED(nullptr)
	{}

private:
	UPROPERTY()
	TObjectPtr<UDEPRECATED_PawnAction> TopAction_DEPRECATED;

public:
	void Pause();
	void Resume();

	/** All it does is tie actions into a double-linked list making NewTopAction
	 *	new stack's top */
	void PushAction(UDEPRECATED_PawnAction& NewTopAction);

	/** Looks through the double-linked action list looking for specified action
	 *	and if found action will be popped along with all it's siblings */
	void PopAction(UDEPRECATED_PawnAction& ActionToPop);
	
	FORCEINLINE UDEPRECATED_PawnAction* GetTop() const { return TopAction_DEPRECATED; }

	FORCEINLINE bool IsEmpty() const { return TopAction_DEPRECATED == NULL; }

	//----------------------------------------------------------------------//
	// Debugging-testing purposes 
	//----------------------------------------------------------------------//
	int32 GetStackSize() const;
};

UCLASS(deprecated, meta = (DeprecationMessage = "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead."))
class AIMODULE_API UDEPRECATED_PawnActionsComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, Category="PawnActions")
	TObjectPtr<APawn> ControlledPawn;

	UPROPERTY()
	TArray<FPawnActionStack> ActionStacks;

	UPROPERTY()
	TArray<FPawnActionEvent> ActionEvents;

	UPROPERTY(Transient)
	TObjectPtr<UDEPRECATED_PawnAction> CurrentAction_DEPRECATED;

	/** set when logic was locked by hi priority stack */
	uint32 bLockedAILogic : 1;

private:
	uint32 ActionEventIndex;

public:
	//----------------------------------------------------------------------//
	// UActorComponent
	//----------------------------------------------------------------------//
	virtual void OnUnregister() override;

	//----------------------------------------------------------------------//
	// blueprint interface
	//----------------------------------------------------------------------//

	UFUNCTION(BlueprintCallable, Category = "AI|PawnActions", meta = (DisplayName = "PerformAction_DEPRECATED", ScriptName = "PerformAction", DeprecatedFunction, DeprecationMessage = "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead."))
	static bool K2_PerformAction(APawn* Pawn, UDEPRECATED_PawnAction* Action, TEnumAsByte<EAIRequestPriority::Type> Priority = EAIRequestPriority::HardScript);

	UE_DEPRECATED(5.2, "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead.")
	static bool PerformAction(APawn& Pawn, UDEPRECATED_PawnAction& Action, TEnumAsByte<EAIRequestPriority::Type> Priority = EAIRequestPriority::HardScript);

	//----------------------------------------------------------------------//
	// 
	//----------------------------------------------------------------------//
	/** Use it to save component work to figure out what it's controlling
	 *	or if component can't/won't be able to figure it out properly
	 *	@NOTE will throw a log warning if trying to set ControlledPawn if it's already set */
	void SetControlledPawn(APawn* NewPawn);
	FORCEINLINE APawn* GetControlledPawn() { return ControlledPawn; }
	FORCEINLINE const APawn* GetControlledPawn() const { return ControlledPawn; }
	FORCEINLINE AController* GetController() { return ControlledPawn ? ControlledPawn->GetController() : NULL; }
	FORCEINLINE UDEPRECATED_PawnAction* GetCurrentAction() { return CurrentAction_DEPRECATED; }

	bool OnEvent(UDEPRECATED_PawnAction& Action, EPawnActionEventType::Type Event);

	UFUNCTION(BlueprintCallable, Category = PawnAction, meta = (DisplayName = "PushAction_DEPRECATED", ScriptName = "PushAction", DeprecatedFunction, DeprecationMessage = "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead."))
	bool K2_PushAction(UDEPRECATED_PawnAction* NewAction, EAIRequestPriority::Type Priority, UObject* Instigator = NULL);

	UE_DEPRECATED(5.2, "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead.")
	bool PushAction(UDEPRECATED_PawnAction& NewAction, EAIRequestPriority::Type Priority, UObject* Instigator = NULL);	

	/** Aborts given action instance */
	UFUNCTION(BlueprintCallable, Category = PawnAction, meta = (DisplayName = "AbortAction_DEPRECATED", ScriptName = "AbortAction", DeprecatedFunction, DeprecationMessage = "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead."))
	EPawnActionAbortState::Type K2_AbortAction(UDEPRECATED_PawnAction* ActionToAbort);

	UE_DEPRECATED(5.2, "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead.")
	EPawnActionAbortState::Type AbortAction(UDEPRECATED_PawnAction& ActionToAbort);

	/** Aborts given action instance */
	UFUNCTION(BlueprintCallable, Category = PawnAction, meta = (DisplayName = "ForceAbortAction_DEPRECATED", ScriptName = "ForceAbortAction", DeprecatedFunction, DeprecationMessage = "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead."))
	EPawnActionAbortState::Type K2_ForceAbortAction(UDEPRECATED_PawnAction* ActionToAbort);

	UE_DEPRECATED(5.2, "PawnActions have been deprecated and are no longer being supported. It will get removed in following UE5 releases. Use GameplayTasks or AITasks instead.")
	EPawnActionAbortState::Type ForceAbortAction(UDEPRECATED_PawnAction& ActionToAbort);

	/** removes all actions instigated with Priority by Instigator
	 *	@param Priority if equal to EAIRequestPriority::MAX then all priority queues will be searched. 
	 *		This is less efficient so use with caution 
	 *	@return number of action abortions requested (performed asyncronously) */
	uint32 AbortActionsInstigatedBy(UObject* const Instigator, EAIRequestPriority::Type Priority);
	
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	FORCEINLINE UDEPRECATED_PawnAction* GetActiveAction(EAIRequestPriority::Type Priority) const { return ActionStacks[Priority].GetTop(); }
	bool HasActiveActionOfType(EAIRequestPriority::Type Priority, TSubclassOf<UDEPRECATED_PawnAction> PawnActionClass) const;

#if ENABLE_VISUAL_LOG
	void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	static FString DescribeEventType(EPawnActionEventType::Type EventType);

	//----------------------------------------------------------------------//
	// Debugging-testing purposes 
	//----------------------------------------------------------------------//
	int32 GetActionStackSize(EAIRequestPriority::Type Priority) const { return ActionStacks[Priority].GetStackSize(); }
	int32 GetActionEventsQueueSize() const { return ActionEvents.Num(); }

protected:
	/** Finds the action that should be running. If it's different from CurrentAction
	 *	then CurrentAction gets paused and newly selected action gets started up */
	void UpdateCurrentAction();

	APawn* CacheControlledPawn();

	void UpdateAILogicLock();

private:
	/** Removed all pending action events associated with PawnAction. Private to make sure it's called only in special cases */
	void RemoveEventsForAction(UDEPRECATED_PawnAction& PawnAction);
};
