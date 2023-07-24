// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction.h"
#include "UObject/Package.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Actions/PawnActionsComponent.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnAction)

PRAGMA_DISABLE_DEPRECATION_WARNINGS

DEFINE_LOG_CATEGORY(LogPawnAction);

namespace
{
	FString GetActionResultName(int64 Value)
	{
		static const UEnum* Enum = StaticEnum<EPawnActionResult::Type>();
		check(Enum);
		return Enum->GetNameStringByValue(Value);
	}
}

UDEPRECATED_PawnAction::UDEPRECATED_PawnAction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RequiredResources(FAIResourcesSet::AllResources)
	, AbortState(EPawnActionAbortState::NeverStarted)
	, FinishResult(EPawnActionResult::NotStarted)
{
	// actions start their lives paused
	bPaused = true;
	bFailedToStart = false;
	IndexOnStack = INDEX_NONE;
}

UWorld* UDEPRECATED_PawnAction::GetWorld() const
{
	return OwnerComponent_DEPRECATED ? OwnerComponent_DEPRECATED->GetWorld() : Cast<UWorld>(GetOuter());
}

void UDEPRECATED_PawnAction::Tick(float DeltaTime)
{
}

EPawnActionAbortState::Type UDEPRECATED_PawnAction::Abort(EAIForceParam::Type ShouldForce)
{
	// if already aborting, and this request is not Forced, just skip it
	if (AbortState != EPawnActionAbortState::NotBeingAborted && ShouldForce != EAIForceParam::Force)
	{
		if (AbortState == EPawnActionAbortState::NeverStarted)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Discarding Abort request since the action has never been started yet"), *GetName());
		}
		else
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Discarding Abort request due to action being already in abort state"), *GetName());
		}
		
		return AbortState;
	}

	const bool bForce = ShouldForce == EAIForceParam::Force;
	EPawnActionAbortState::Type Result = EPawnActionAbortState::NotBeingAborted;
	EPawnActionAbortState::Type ChildResult = EPawnActionAbortState::AbortDone;

	SetAbortState(EPawnActionAbortState::MarkPendingAbort);

	if (ChildAction_DEPRECATED != NULL)
	{
		ChildResult = ChildAction_DEPRECATED->Abort(ShouldForce);

		if (ChildResult == EPawnActionAbortState::NotBeingAborted)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Error, TEXT("%s> ChildAction_DEPRECATED %s failed to carry out proper abortion! Things might get ugly..")
				, *GetName(), *ChildAction_DEPRECATED->GetName());

			// fake proper result and hope for the best!
			ChildResult = EPawnActionAbortState::AbortDone;
		}
	}

	if (bForce)
	{
		Result = PerformAbort(ShouldForce);
		if (Result != EPawnActionAbortState::AbortDone)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Error, TEXT("%s> failed to force-abort! Things might get ugly..")
				, *GetName());

			// fake proper result and hope for the best!
			Result = EPawnActionAbortState::AbortDone;
		}
	}
	else
	{
		switch (ChildResult)
		{
		case EPawnActionAbortState::MarkPendingAbort:
			// this means child is awaiting its abort, so should parent
		case EPawnActionAbortState::LatentAbortInProgress:
			// this means child is performing time-consuming abort. Parent should wait
			Result = EPawnActionAbortState::MarkPendingAbort;
			break;

		case EPawnActionAbortState::AbortDone:
			Result = IsPaused() ? EPawnActionAbortState::MarkPendingAbort : PerformAbort(ShouldForce);
			break;

		default:
			UE_VLOG(GetPawn(), LogPawnAction, Error, TEXT("%s> Unhandled Abort State!")
				, *GetName());
			Result = EPawnActionAbortState::AbortDone;
			break;
		}
	}

	SetAbortState(Result);

	return Result;
}

APawn* UDEPRECATED_PawnAction::GetPawn() const
{
	return OwnerComponent_DEPRECATED ? OwnerComponent_DEPRECATED->GetControlledPawn() : NULL;
}

AController* UDEPRECATED_PawnAction::GetController() const
{
	return OwnerComponent_DEPRECATED ? OwnerComponent_DEPRECATED->GetController() : NULL;
}

void UDEPRECATED_PawnAction::SetAbortState(EPawnActionAbortState::Type NewAbortState)
{
	// allowing only progression
	if (NewAbortState <= AbortState)
	{
		return;
	}

	AbortState = NewAbortState;
	if (AbortState == EPawnActionAbortState::AbortDone)
	{
		SendEvent(EPawnActionEventType::FinishedAborting);
	}
}

void UDEPRECATED_PawnAction::SendEvent(EPawnActionEventType::Type Event)
{
	if (IsValid(OwnerComponent_DEPRECATED))
	{
		// this will get communicated to parent action if needed, latently 
		OwnerComponent_DEPRECATED->OnEvent(*this, Event);
	}

	ActionObserver.ExecuteIfBound(*this, Event);
}

void UDEPRECATED_PawnAction::StopWaitingForMessages()
{
	MessageHandlers.Reset();
}

void UDEPRECATED_PawnAction::SetFinishResult(EPawnActionResult::Type Result)
{
	// once return value had been set it's no longer possible to back to InProgress
	if (Result <= EPawnActionResult::InProgress)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> UDEPRECATED_PawnAction::SetFinishResult setting FinishResult as EPawnActionResult::InProgress or EPawnActionResult::NotStarted - should not be happening"), *GetName());
		return;
	}

	if (FinishResult != Result)
	{
		FinishResult = Result;
	}
}

void UDEPRECATED_PawnAction::SetOwnerComponent(UDEPRECATED_PawnActionsComponent* Component)
{
	if (OwnerComponent_DEPRECATED != NULL && OwnerComponent_DEPRECATED != Component)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> UDEPRECATED_PawnAction::SetOwnerComponent called to change already set valid owner component"), *GetName());
	}

	OwnerComponent_DEPRECATED = Component;
	if (Component != NULL)
	{
		AAIController* AIController = Cast<AAIController>(Component->GetController());
		if (AIController != NULL)
		{
			BrainComp = AIController->FindComponentByClass<UBrainComponent>();
		}
	}
}

void UDEPRECATED_PawnAction::SetInstigator(UObject* const InInstigator)
{ 
	if (Instigator && Instigator != InInstigator)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> setting Instigator to %s when already has instigator set to %s")
			, *GetName(), *Instigator->GetName(), InInstigator ? *InInstigator->GetName() : TEXT("<null>"));
	}
	Instigator = InInstigator; 
}

void UDEPRECATED_PawnAction::Finish(TEnumAsByte<EPawnActionResult::Type> WithResult)
{
	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> finishing with result %s")
		, *GetName(), *GetActionResultName(WithResult));

	SetFinishResult(WithResult);

	StopWaitingForMessages();

	SendEvent(EPawnActionEventType::FinishedExecution);
}

bool UDEPRECATED_PawnAction::Activate()
{
	bool bResult = false; 

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Activating at priority %s! First start? %s Paused? %s")
		, *GetName()
		, *GetPriorityName()
		, HasBeenStarted() ? TEXT("NO") : TEXT("YES")
		, IsPaused() ? TEXT("YES") : TEXT("NO"));

	if (HasBeenStarted() && IsPaused())
	{
		bResult = Resume();
		if (bResult == false)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Failed to RESUME.")
				, *GetName());
			bFailedToStart = true;
			SetFinishResult(EPawnActionResult::Failed);
			SendEvent(EPawnActionEventType::FailedToStart);
		}
	}
	else 
	{
		bResult = Start();
		if (bResult == false)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Failed to START.")
				, *GetName());
			bFailedToStart = true;
			SetFinishResult(EPawnActionResult::Failed);
			SendEvent(EPawnActionEventType::FailedToStart);
		}
	}

	return bResult;
}

void UDEPRECATED_PawnAction::OnPopped()
{
	// not calling OnFinish if action haven't actually started
	if (!bFailedToStart || bAlwaysNotifyOnFinished)
	{
		OnFinished(FinishResult);
	}
}

bool UDEPRECATED_PawnAction::Start()
{
	AbortState = EPawnActionAbortState::NotBeingAborted;
	FinishResult = EPawnActionResult::InProgress;
	bPaused = false;
	return true;
}


bool UDEPRECATED_PawnAction::Pause(const UDEPRECATED_PawnAction* PausedBy)
{
	// parent should be paused anyway
	ensure(ParentAction_DEPRECATED == NULL || ParentAction_DEPRECATED->IsPaused() == true);

	// don't pause twice, this should be guaranteed by the PawnActionsComponent
	ensure(bPaused == false);
	
	if (AbortState == EPawnActionAbortState::LatentAbortInProgress || AbortState == EPawnActionAbortState::AbortDone)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Warning, TEXT("%s> Not pausing due to being in unpausable aborting state"), *GetName());
		return false;
	}

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Pausing..."), *GetName());
	
	bPaused = true;

	if (ChildAction_DEPRECATED)
	{
		ChildAction_DEPRECATED->Pause(PausedBy);
	}

	return bPaused;
}

bool UDEPRECATED_PawnAction::Resume()
{
	// parent should be paused anyway
	ensure(ParentAction_DEPRECATED == NULL || ParentAction_DEPRECATED->IsPaused() == true);

	// do not unpause twice
	if (bPaused == false)
	{
		return false;
	}
	
	ensure(ChildAction_DEPRECATED == NULL);

	if (ChildAction_DEPRECATED)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Resuming child, %s"), *GetName(), *ChildAction_DEPRECATED->GetName());
		ChildAction_DEPRECATED->Resume();
	}
	else
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Resuming."), *GetName());
		bPaused = false;
	}

	return !bPaused;
}

void UDEPRECATED_PawnAction::OnFinished(EPawnActionResult::Type WithResult)
{
}

void UDEPRECATED_PawnAction::OnChildFinished(UDEPRECATED_PawnAction& Action, EPawnActionResult::Type WithResult)
{
	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Child \'%s\' finished with result %s")
		, *GetName(), *Action.GetName(), *GetActionResultName(WithResult));

	ensure(Action.ParentAction_DEPRECATED == this);
	ensure(ChildAction_DEPRECATED == &Action);
	Action.ParentAction_DEPRECATED = NULL;
	ChildAction_DEPRECATED = NULL;
}

void UDEPRECATED_PawnAction::OnPushed()
{
	IndexOnStack = 0;
	UDEPRECATED_PawnAction* PrevAction = ParentAction_DEPRECATED;
	while (PrevAction)
	{
		++IndexOnStack;
		PrevAction = PrevAction->ParentAction_DEPRECATED;
	}

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> Pushed with priority %s, IndexOnStack: %d, instigator %s")
		, *GetName(), *GetPriorityName(), IndexOnStack, *GetNameSafe(Instigator));
}

bool UDEPRECATED_PawnAction::PushChildAction(UDEPRECATED_PawnAction& Action)
{
	bool bResult = false;
	
	if (OwnerComponent_DEPRECATED != NULL)
	{
		UE_CVLOG( ChildAction_DEPRECATED != NULL
			, GetPawn(), LogPawnAction, Log, TEXT("%s> Pushing child action %s while already having ChildAction_DEPRECATED set to %s")
			, *GetName(), *Action.GetName(), *ChildAction_DEPRECATED->GetName());
		
		// copy runtime data
		// note that priority and instigator will get assigned as part of PushAction.

		bResult = OwnerComponent_DEPRECATED->PushAction(Action, GetPriority(), Instigator);

		// adding a check to make sure important data has been set 
		ensure(Action.GetPriority() == GetPriority() && Action.GetInstigator() == GetInstigator());
	}

	return bResult;
}

//----------------------------------------------------------------------//
// messaging
//----------------------------------------------------------------------//

void UDEPRECATED_PawnAction::WaitForMessage(FName MessageType, FAIRequestID InRequestID)
{
	MessageHandlers.Add(FAIMessageObserver::Create(BrainComp, MessageType, InRequestID.GetID(), FOnAIMessage::CreateUObject(this, &UDEPRECATED_PawnAction::HandleAIMessage)));
}

//----------------------------------------------------------------------//
// blueprint interface
//----------------------------------------------------------------------//

UDEPRECATED_PawnAction* UDEPRECATED_PawnAction::CreateActionInstance(UObject* WorldContextObject, TSubclassOf<UDEPRECATED_PawnAction> ActionClass)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World && ActionClass)
	{
		return NewObject<UDEPRECATED_PawnAction>(World, ActionClass);
	}
	return NULL;
}

//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//

FString UDEPRECATED_PawnAction::GetStateDescription() const
{
	static const UEnum* AbortStateEnum = StaticEnum<EPawnActionAbortState::Type>(); 
		
	if (AbortState != EPawnActionAbortState::NotBeingAborted)
	{
		return *AbortStateEnum->GetDisplayNameTextByValue(AbortState).ToString();
	}
	return IsPaused() ? TEXT("Paused") : TEXT("Active");
}

FString UDEPRECATED_PawnAction::GetPriorityName() const
{
	static const UEnum* Enum = StaticEnum<EAIRequestPriority::Type>();
	check(Enum);
	return Enum->GetNameStringByValue(GetPriority());
}

FString UDEPRECATED_PawnAction::GetDisplayName() const
{
	return GetClass()->GetName();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
