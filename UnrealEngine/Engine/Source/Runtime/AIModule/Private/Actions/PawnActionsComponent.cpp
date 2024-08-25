// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnActionsComponent.h"
#include "UObject/Package.h"
#include "GameFramework/Controller.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTNode.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "Actions/PawnAction_Sequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnActionsComponent)

PRAGMA_DISABLE_DEPRECATION_WARNINGS

//----------------------------------------------------------------------//
// helpers
//----------------------------------------------------------------------//

namespace
{
	FString GetEventName(int64 Value)
	{
		static const UEnum* Enum = StaticEnum<EPawnActionEventType::Type>();
		check(Enum);
		return Enum->GetNameStringByValue(Value);
	}

	FString GetPriorityName(int64 Value)
	{
		static const UEnum* Enum = StaticEnum<EAIRequestPriority::Type>();
		check(Enum);
		return Enum->GetNameStringByValue(Value);
	}

	FString GetActionSignature(UDEPRECATED_PawnAction* Action)
	{
		if (Action == NULL)
		{
			return TEXT("NULL");
		}
		
		return FString::Printf(TEXT("[%s, %s]"), *Action->GetName(), *GetPriorityName(Action->GetPriority()));
	}
}

//----------------------------------------------------------------------//
// FPawnActionEvent
//----------------------------------------------------------------------//

namespace
{
	struct FPawnActionEvenSort
	{
		FORCEINLINE bool operator()(const FPawnActionEvent& A, const FPawnActionEvent& B) const
		{
			return A.Priority < B.Priority
				|| (A.Priority == B.Priority
					&& (A.EventType < B.EventType
						|| (A.EventType == B.EventType && A.Index < B.Index)));
		}
	};
}

FPawnActionEvent::FPawnActionEvent(UDEPRECATED_PawnAction& InAction, EPawnActionEventType::Type InEventType, uint32 InIndex)
	: Action_DEPRECATED(&InAction), EventType(InEventType), Index(InIndex)
{
	Priority = InAction.GetPriority();
}

//----------------------------------------------------------------------//
// FPawnActionStack
//----------------------------------------------------------------------//

void FPawnActionStack::Pause()
{
	if (TopAction_DEPRECATED != NULL)
	{
		TopAction_DEPRECATED->Pause(NULL);
	}
}

void FPawnActionStack::Resume()
{
	if (TopAction_DEPRECATED != NULL)
	{
		TopAction_DEPRECATED->Resume();
	}
}

void FPawnActionStack::PushAction(UDEPRECATED_PawnAction& NewTopAction)
{
	if (TopAction_DEPRECATED != NULL)
	{
		if (TopAction_DEPRECATED->IsPaused() == false && TopAction_DEPRECATED->HasBeenStarted() == true)
		{
			TopAction_DEPRECATED->Pause(&NewTopAction);
		}
		ensure(TopAction_DEPRECATED->ChildAction_DEPRECATED == NULL);
		TopAction_DEPRECATED->ChildAction_DEPRECATED = &NewTopAction;
		NewTopAction.ParentAction_DEPRECATED = TopAction_DEPRECATED;
	}

	TopAction_DEPRECATED = &NewTopAction;
	NewTopAction.OnPushed();
}

void FPawnActionStack::PopAction(UDEPRECATED_PawnAction& ActionToPop)
{
	// first check if it's there
	UDEPRECATED_PawnAction* CutPoint = TopAction_DEPRECATED;
	while (CutPoint != NULL && CutPoint != &ActionToPop)
	{
		CutPoint = CutPoint->ParentAction_DEPRECATED;
	}

	if (CutPoint == &ActionToPop)
	{
		UDEPRECATED_PawnAction* ActionBeingRemoved = TopAction_DEPRECATED;
		// note StopAction can be null
		UDEPRECATED_PawnAction* StopAction = ActionToPop.ParentAction_DEPRECATED;

		while (ActionBeingRemoved != StopAction && ActionBeingRemoved != nullptr)
		{
			checkSlow(ActionBeingRemoved);
			UDEPRECATED_PawnAction* NextAction = ActionBeingRemoved->ParentAction_DEPRECATED;

			if (ActionBeingRemoved->IsBeingAborted() == false && ActionBeingRemoved->IsFinished() == false)
			{
				// forcing abort to make sure it happens instantly. We don't have time for delayed finish here.
				ActionBeingRemoved->Abort(EAIForceParam::Force);
			}
			ActionBeingRemoved->OnPopped();
			if (ActionBeingRemoved->ParentAction_DEPRECATED)
			{
				ActionBeingRemoved->ParentAction_DEPRECATED->OnChildFinished(*ActionBeingRemoved, ActionBeingRemoved->FinishResult);
			}
			ActionBeingRemoved = NextAction;
		}

		TopAction_DEPRECATED = StopAction;
	}
}

int32 FPawnActionStack::GetStackSize() const
{
	int32 Size = 0;
	const UDEPRECATED_PawnAction* TempAction = TopAction_DEPRECATED;
	while (TempAction != nullptr)
	{
		TempAction = TempAction->GetParentAction();
		++Size;
	}
	return Size;
}

//----------------------------------------------------------------------//
// UDEPRECATED_PawnActionsComponent
//----------------------------------------------------------------------//

UDEPRECATED_PawnActionsComponent::UDEPRECATED_PawnActionsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	bAutoActivate = true;
	bLockedAILogic = false;

	ActionEventIndex = 0;

	ActionStacks.AddZeroed(EAIRequestPriority::MAX);
}

void UDEPRECATED_PawnActionsComponent::OnUnregister()
{
	if ((ControlledPawn != nullptr) && !ControlledPawn->IsPendingKillPending())
	{
		// call for every regular priority 
		for (int32 PriorityIndex = 0; PriorityIndex < EAIRequestPriority::MAX; ++PriorityIndex)
		{
			UDEPRECATED_PawnAction* Action = ActionStacks[PriorityIndex].GetTop();
			while (Action)
			{
				Action->Abort(EAIForceParam::Force);
				Action = Action->ParentAction_DEPRECATED;
			}
		}
	}

	Super::OnUnregister();
}

void UDEPRECATED_PawnActionsComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ControlledPawn == NULL)
	{
		CacheControlledPawn();
	}

	if (ActionEvents.Num() > 1)
	{
		ActionEvents.Sort(FPawnActionEvenSort());
	}

	if (ActionEvents.Num() > 0)
	{
		for (int32 EventIndex = 0; EventIndex < ActionEvents.Num(); ++EventIndex)
		{
			FPawnActionEvent& Event = ActionEvents[EventIndex];

			if (Event.Action_DEPRECATED == nullptr)
			{
				UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("NULL action encountered during ActionEvents processing. May result in some notifies not being sent out."));
				continue;
			}

			switch (Event.EventType)
			{
			case EPawnActionEventType::InstantAbort:
				// can result in adding new ActionEvents (from child actions) and reallocating data in ActionEvents array
				// because of it, we need to operate on copy instead of reference to memory address
				{
					FPawnActionEvent EventCopy(Event);
					EventCopy.Action_DEPRECATED->Abort(EAIForceParam::Force);
					ActionStacks[EventCopy.Priority].PopAction(*EventCopy.Action_DEPRECATED);
				}
				break;
			case EPawnActionEventType::FinishedAborting:
			case EPawnActionEventType::FinishedExecution:
			case EPawnActionEventType::FailedToStart:
				ActionStacks[Event.Priority].PopAction(*Event.Action_DEPRECATED);
				break;
			case EPawnActionEventType::Push:
				ActionStacks[Event.Priority].PushAction(*Event.Action_DEPRECATED);
				break;
			default:
				break;
			}
		}

		ActionEvents.Reset();

		UpdateCurrentAction();
	}

	if (CurrentAction_DEPRECATED)
	{
		CurrentAction_DEPRECATED->TickAction(DeltaTime);
	}

	// it's possible we got new events with CurrentAction_DEPRECATED's tick
	if (ActionEvents.Num() == 0 && (CurrentAction_DEPRECATED == NULL || CurrentAction_DEPRECATED->WantsTick() == false))
	{
		SetComponentTickEnabled(false);
	}
}

bool UDEPRECATED_PawnActionsComponent::HasActiveActionOfType(EAIRequestPriority::Type Priority, TSubclassOf<UDEPRECATED_PawnAction> PawnActionClass) const
{
	TArray<UDEPRECATED_PawnAction*> ActionsToTest;
	ActionsToTest.Add(GetActiveAction(Priority));

	while (ActionsToTest.Num() > 0)
	{
		UDEPRECATED_PawnAction* ActiveActionIter = ActionsToTest[0];

		if (ActiveActionIter)
		{
			if (ActiveActionIter->GetClass()->IsChildOf(*PawnActionClass))
			{
				return true;
			}	
			else
			{
				UDEPRECATED_PawnAction_Sequence* PawnActionSequence = Cast<UDEPRECATED_PawnAction_Sequence>(ActiveActionIter);

				if (PawnActionSequence)
				{
					for (int32 PawnActionSequenceCount = 0; PawnActionSequenceCount < PawnActionSequence->ActionSequence_DEPRECATED.Num(); ++PawnActionSequenceCount)
					{
						ActionsToTest.Add(PawnActionSequence->ActionSequence_DEPRECATED[PawnActionSequenceCount]);
					}
				}
			}
		}

		ActionsToTest.RemoveAt(0);
	}

	// Didn't find one.
	return false;
}

void UDEPRECATED_PawnActionsComponent::UpdateCurrentAction()
{
	UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Picking new current actions. Old CurrentAction_DEPRECATED %s")
		, *GetActionSignature(CurrentAction_DEPRECATED));

	// find the highest priority action available
	UDEPRECATED_PawnAction* NewCurrentAction = NULL;
	int32 Priority = EAIRequestPriority::MAX - 1;
	do 
	{
		NewCurrentAction = ActionStacks[Priority].GetTop();

	} while (NewCurrentAction == NULL && --Priority >= 0);

	// if it's a new Action then enable it
	if (CurrentAction_DEPRECATED != NewCurrentAction)
	{
		UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("New action: %s")
			, *GetActionSignature(NewCurrentAction));

		if (CurrentAction_DEPRECATED != NULL && CurrentAction_DEPRECATED->IsActive())
		{
			CurrentAction_DEPRECATED->Pause(NewCurrentAction);
		}
		CurrentAction_DEPRECATED = NewCurrentAction;
		bool bNewActionStartedSuccessfully = true;
		if (CurrentAction_DEPRECATED != NULL)
		{
			bNewActionStartedSuccessfully = CurrentAction_DEPRECATED->Activate();
		}

		if (bNewActionStartedSuccessfully == false)
		{
			UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("CurrentAction_DEPRECATED %s failed to activate. Removing and re-running action selection")
				, *GetActionSignature(NewCurrentAction));

			CurrentAction_DEPRECATED = NULL;			
		}
		// @HACK temporary solution to have actions and old BT tasks work together
		else if (CurrentAction_DEPRECATED == NULL || CurrentAction_DEPRECATED->GetPriority() != EAIRequestPriority::Logic)
		{
			UpdateAILogicLock();
		}
	}
	else
	{
		if (CurrentAction_DEPRECATED == NULL)
		{
			UpdateAILogicLock();
		}
		else if (CurrentAction_DEPRECATED->IsFinished())
		{
			UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Re-running same action"));
			CurrentAction_DEPRECATED->Activate();
		}
		else
		{ 
			UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Still doing the same action"));
		}
	}
}

void UDEPRECATED_PawnActionsComponent::UpdateAILogicLock()
{
	if (ControlledPawn && ControlledPawn->GetController())
	{
		UBrainComponent* BrainComp = ControlledPawn->GetController()->FindComponentByClass<UBrainComponent>();
		if (BrainComp)
		{
			if (CurrentAction_DEPRECATED != NULL && CurrentAction_DEPRECATED->GetPriority() > EAIRequestPriority::Logic)
			{
				UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Locking AI logic"));
				BrainComp->LockResource(EAIRequestPriority::HardScript);
				bLockedAILogic = true;
			}
			else if (bLockedAILogic)
			{
				UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Clearing AI logic lock"));
				bLockedAILogic = false;
				BrainComp->ClearResourceLock(EAIRequestPriority::HardScript);
				if (BrainComp->IsResourceLocked() == false)
				{
					UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("Reseting AI logic"));
					BrainComp->RestartLogic();
				}
				// @todo consider checking if lock priority is < Logic
				else
				{
					UE_VLOG(ControlledPawn, LogPawnAction, Log, TEXT("AI logic still locked with other priority"));
					BrainComp->RequestLogicRestartOnUnlock();
				}
			}
		}
	}
}

EPawnActionAbortState::Type UDEPRECATED_PawnActionsComponent::K2_AbortAction(UDEPRECATED_PawnAction* ActionToAbort)
{
	if (ActionToAbort != NULL)
	{
		return AbortAction(*ActionToAbort);
	}
	return EPawnActionAbortState::NeverStarted;
}

EPawnActionAbortState::Type UDEPRECATED_PawnActionsComponent::AbortAction(UDEPRECATED_PawnAction& ActionToAbort)
{
	const EPawnActionAbortState::Type AbortState = ActionToAbort.Abort(EAIForceParam::DoNotForce);
	if (AbortState == EPawnActionAbortState::NeverStarted)
	{
		// this is a special case. It's possible someone tried to abort an action that 
		// has just requested to be pushes and the push event has not been processed yet.
		// in such a case we'll look through the awaiting action events and remove a push event 
		// for given ActionToAbort
		RemoveEventsForAction(ActionToAbort);
	}
	return AbortState;
}

void UDEPRECATED_PawnActionsComponent::RemoveEventsForAction(UDEPRECATED_PawnAction& PawnAction)
{
	for (int32 ActionIndex = ActionEvents.Num() - 1; ActionIndex >= 0; --ActionIndex)
	{
		if (ActionEvents[ActionIndex].Action_DEPRECATED == &PawnAction)
		{
			ActionEvents.RemoveAtSwap(ActionIndex, /*Count=*/1, EAllowShrinking::No);
		}
	}
}

EPawnActionAbortState::Type UDEPRECATED_PawnActionsComponent::K2_ForceAbortAction(UDEPRECATED_PawnAction* ActionToAbort)
{
	if (ActionToAbort)
	{
		return ForceAbortAction(*ActionToAbort);
	}
	return EPawnActionAbortState::NeverStarted;
}

EPawnActionAbortState::Type UDEPRECATED_PawnActionsComponent::ForceAbortAction(UDEPRECATED_PawnAction& ActionToAbort)
{
	return ActionToAbort.Abort(EAIForceParam::Force);
}

uint32 UDEPRECATED_PawnActionsComponent::AbortActionsInstigatedBy(UObject* const Instigator, EAIRequestPriority::Type Priority)
{
	uint32 AbortedActionsCount = 0;

	if (Priority == EAIRequestPriority::MAX)
	{
		// call for every regular priority 
		for (int32 PriorityIndex = 0; PriorityIndex < EAIRequestPriority::MAX; ++PriorityIndex)
		{
			AbortedActionsCount += AbortActionsInstigatedBy(Instigator, EAIRequestPriority::Type(PriorityIndex));
		}
	}
	else
	{
		UDEPRECATED_PawnAction* Action = ActionStacks[Priority].GetTop();
		while (Action)
		{
			if (Action->GetInstigator() == Instigator)
			{
				OnEvent(*Action, EPawnActionEventType::InstantAbort);
				++AbortedActionsCount;
			}
			Action = Action->ParentAction_DEPRECATED;
		}

		for (int32 ActionIndex = ActionEvents.Num() - 1; ActionIndex >= 0; --ActionIndex)
		{
			const FPawnActionEvent& Event = ActionEvents[ActionIndex];
			if (Event.Priority == Priority &&
				Event.EventType == EPawnActionEventType::Push &&
				Event.Action_DEPRECATED && Event.Action_DEPRECATED->GetInstigator() == Instigator)
			{
				ActionEvents.RemoveAtSwap(ActionIndex, /*Count=*/1, EAllowShrinking::No);
				AbortedActionsCount++;
			}
		}
	}

	return AbortedActionsCount;
}

bool UDEPRECATED_PawnActionsComponent::K2_PushAction(UDEPRECATED_PawnAction* NewAction, EAIRequestPriority::Type Priority, UObject* Instigator)
{
	if (NewAction)
	{
		return PushAction(*NewAction, Priority, Instigator);
	}
	return false;
}

bool UDEPRECATED_PawnActionsComponent::PushAction(UDEPRECATED_PawnAction& NewAction, EAIRequestPriority::Type Priority, UObject* Instigator)
{
	if (NewAction.HasBeenStarted() == false || NewAction.IsFinished() == true)
	{
		NewAction.ExecutionPriority = Priority;
		NewAction.SetOwnerComponent(this);
		NewAction.SetInstigator(Instigator);
		return OnEvent(NewAction, EPawnActionEventType::Push);
	}

	return false;
}

bool UDEPRECATED_PawnActionsComponent::OnEvent(UDEPRECATED_PawnAction& Action, EPawnActionEventType::Type Event)
{
	bool bResult = false;
	const FPawnActionEvent ActionEvent(Action, Event, ActionEventIndex++);

	if (Event != EPawnActionEventType::Invalid && ActionEvents.Find(ActionEvent) == INDEX_NONE)
	{
		ActionEvents.Add(ActionEvent);

		// if it's a first even enable tick
		if (ActionEvents.Num() == 1)
		{
			SetComponentTickEnabled(true);
		}

		bResult = true;
	}
	else if (Event == EPawnActionEventType::Invalid)
	{
		// ignore
		UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Ignoring Action Event: Action %s Event %s")
			, *Action.GetName(), *GetEventName(Event));
	}
	else
	{
		UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Ignoring duplicate Action Event: Action %s Event %s")
			, *Action.GetName(), *GetEventName(Event));
	}

	return bResult;
}

void UDEPRECATED_PawnActionsComponent::SetControlledPawn(APawn* NewPawn)
{
	if (ControlledPawn != NULL && ControlledPawn != NewPawn)
	{
		UE_VLOG(ControlledPawn, LogPawnAction, Warning, TEXT("Trying to set ControlledPawn to new value while ControlledPawn is already set!"));
	}
	else
	{
		ControlledPawn = NewPawn;
	}
}

APawn* UDEPRECATED_PawnActionsComponent::CacheControlledPawn()
{
	if (ControlledPawn == NULL)
	{
		AActor* ActorOwner = GetOwner();
		if (ActorOwner)
		{
			ControlledPawn = Cast<APawn>(ActorOwner);
			if (ControlledPawn == NULL)
			{
				AController* Controller = Cast<AController>(ActorOwner);
				if (Controller != NULL)
				{
					ControlledPawn = Controller->GetPawn();
				}
			}
		}
	}

	return ControlledPawn;
}


//----------------------------------------------------------------------//
// blueprint interface
//----------------------------------------------------------------------//
bool UDEPRECATED_PawnActionsComponent::K2_PerformAction(APawn* Pawn, UDEPRECATED_PawnAction* Action, TEnumAsByte<EAIRequestPriority::Type> Priority)
{
	if (Pawn && Action)
	{
		return PerformAction(*Pawn, *Action, Priority);
	}
	return false;
}

bool UDEPRECATED_PawnActionsComponent::PerformAction(APawn& Pawn, UDEPRECATED_PawnAction& Action, TEnumAsByte<EAIRequestPriority::Type> Priority)
{
	bool bSuccess = false;

	ensure(Priority < EAIRequestPriority::MAX);

	if (Pawn.GetController())
	{
		UDEPRECATED_PawnActionsComponent* ActionComp = Pawn.GetController()->FindComponentByClass<UDEPRECATED_PawnActionsComponent>();
		if (ActionComp)
		{
			ActionComp->PushAction(Action, Priority);
			bSuccess = true;
		}
	}

	return bSuccess;
}

//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//
#if ENABLE_VISUAL_LOG
void UDEPRECATED_PawnActionsComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	static const FString Category = TEXT("PawnActions");

	if (!IsValid(this))
	{
		return;
	}

	for (int32 PriorityIndex = 0; PriorityIndex < ActionStacks.Num(); ++PriorityIndex)
	{
		const UDEPRECATED_PawnAction* Action = ActionStacks[PriorityIndex].GetTop();
		if (Action == NULL)
		{
			continue;
		}

		FVisualLogStatusCategory StatusCategory;
		StatusCategory.Category = Category + TEXT(": ") + GetPriorityName(PriorityIndex);

		while (Action)
		{
			FString InstigatorDesc;
			const UObject* InstigatorOb = Action->GetInstigator();
			const UBTNode* InstigatorBT = Cast<const UBTNode>(InstigatorOb);
			InstigatorDesc = InstigatorBT ?
				FString::Printf(TEXT("%s = %s"), *UBehaviorTreeTypes::DescribeNodeHelper(InstigatorBT), *InstigatorBT->GetName()) :
				GetNameSafe(InstigatorOb);

			StatusCategory.Add(Action->GetName(), FString::Printf(TEXT("%s, Instigator:%s"), *Action->GetStateDescription(), *InstigatorDesc));
			Action = Action->GetParentAction();
		}

		Snapshot->Status.Add(StatusCategory);
	}
}
#endif // ENABLE_VISUAL_LOG

FString UDEPRECATED_PawnActionsComponent::DescribeEventType(EPawnActionEventType::Type EventType)
{
	return GetEventName(EventType);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
