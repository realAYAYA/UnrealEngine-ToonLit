// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction_Sequence.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnAction_Sequence)

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_PawnAction_Sequence::UDEPRECATED_PawnAction_Sequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SubActionTriggeringPolicy(EPawnSubActionTriggeringPolicy::CopyBeforeTriggering)
{
}

UDEPRECATED_PawnAction_Sequence* UDEPRECATED_PawnAction_Sequence::CreateAction(UWorld& World, TArray<UDEPRECATED_PawnAction*>& ActionSequence, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy)
{
	ActionSequence.Remove(NULL);
	if (ActionSequence.Num() <= 0)
	{
		return NULL;
	}

	UDEPRECATED_PawnAction_Sequence* Action = UDEPRECATED_PawnAction::CreateActionInstance<UDEPRECATED_PawnAction_Sequence>(World);
	if (Action)
	{
		Action->ActionSequence_DEPRECATED = ActionSequence;

		for (const UDEPRECATED_PawnAction* SubAction : ActionSequence)
		{
			if (SubAction && SubAction->ShouldPauseMovement())
			{
				Action->bShouldPauseMovement = true;
				break;
			}
		}

		Action->SubActionTriggeringPolicy = InSubActionTriggeringPolicy;
	}

	return Action;
}

bool UDEPRECATED_PawnAction_Sequence::Start()
{
	bool bResult = Super::Start();

	if (bResult)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("Starting sequence. Items:"), *GetName());
		for (auto Action : ActionSequence_DEPRECATED)
		{
			UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("    %s"), *GetNameSafe(Action));
		}

		bResult = PushNextActionCopy();
	}

	return bResult;
}

bool UDEPRECATED_PawnAction_Sequence::Resume()
{
	bool bResult = Super::Resume();

	if (bResult)
	{
		bResult = PushNextActionCopy();
	}

	return bResult;
}

void UDEPRECATED_PawnAction_Sequence::OnChildFinished(UDEPRECATED_PawnAction& Action, EPawnActionResult::Type WithResult)
{
	if (RecentActionCopy_DEPRECATED == &Action)
	{
		if (WithResult == EPawnActionResult::Success || (WithResult == EPawnActionResult::Failed && ChildFailureHandlingMode == EPawnActionFailHandling::IgnoreFailure))
		{
			if (GetAbortState() == EPawnActionAbortState::NotBeingAborted)
			{
				PushNextActionCopy();
			}
		}
		else
		{
			Finish(EPawnActionResult::Failed);
		}
	}

	Super::OnChildFinished(Action, WithResult);
}

bool UDEPRECATED_PawnAction_Sequence::PushNextActionCopy()
{
	if (CurrentActionIndex >= uint32(ActionSequence_DEPRECATED.Num()))
	{
		Finish(EPawnActionResult::Success);
		return true;
	}

	UDEPRECATED_PawnAction* ActionCopy = SubActionTriggeringPolicy == EPawnSubActionTriggeringPolicy::CopyBeforeTriggering
		? Cast<UDEPRECATED_PawnAction>(StaticDuplicateObject(ActionSequence_DEPRECATED[CurrentActionIndex], this))
		: ToRawPtr(ActionSequence_DEPRECATED[CurrentActionIndex]);

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> pushing action %s")
		, *GetName(), *GetNameSafe(ActionCopy));
	++CurrentActionIndex;	
	check(ActionCopy);
	RecentActionCopy_DEPRECATED = ActionCopy;
	return PushChildAction(*ActionCopy);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
