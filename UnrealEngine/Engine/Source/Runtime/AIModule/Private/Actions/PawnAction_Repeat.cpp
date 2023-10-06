// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction_Repeat.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnAction_Repeat)

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_PawnAction_Repeat::UDEPRECATED_PawnAction_Repeat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SubActionTriggeringPolicy(EPawnSubActionTriggeringPolicy::CopyBeforeTriggering)
{
	ChildFailureHandlingMode = EPawnActionFailHandling::IgnoreFailure;
}

UDEPRECATED_PawnAction_Repeat* UDEPRECATED_PawnAction_Repeat::CreateAction(UWorld& World, UDEPRECATED_PawnAction* ActionToRepeat, int32 NumberOfRepeats, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy)
{
	if (ActionToRepeat == NULL || !(NumberOfRepeats > 0 || NumberOfRepeats == UDEPRECATED_PawnAction_Repeat::LoopForever))
	{
		return NULL;
	}

	UDEPRECATED_PawnAction_Repeat* Action = UDEPRECATED_PawnAction::CreateActionInstance<UDEPRECATED_PawnAction_Repeat>(World);
	if (Action)
	{
		Action->ActionToRepeat_DEPRECATED = ActionToRepeat;
		Action->RepeatsLeft = NumberOfRepeats;
		Action->SubActionTriggeringPolicy = InSubActionTriggeringPolicy;

		Action->bShouldPauseMovement = ActionToRepeat->ShouldPauseMovement();
	}

	return Action;
}

bool UDEPRECATED_PawnAction_Repeat::Start()
{
	bool bResult = Super::Start();
	
	if (bResult)
	{
		UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("Starting repeating action: %s. Requested repeats: %d")
			, *GetNameSafe(ActionToRepeat_DEPRECATED), RepeatsLeft);
		bResult = PushSubAction();
	}

	return bResult;
}

bool UDEPRECATED_PawnAction_Repeat::Resume()
{
	bool bResult = Super::Resume();

	if (bResult)
	{
		bResult = PushSubAction();
	}

	return bResult;
}

void UDEPRECATED_PawnAction_Repeat::OnChildFinished(UDEPRECATED_PawnAction& Action, EPawnActionResult::Type WithResult)
{
	if (RecentActionCopy_DEPRECATED == &Action)
	{
		if (WithResult == EPawnActionResult::Success || (WithResult == EPawnActionResult::Failed && ChildFailureHandlingMode == EPawnActionFailHandling::IgnoreFailure))
		{
			PushSubAction();
		}
		else
		{
			Finish(EPawnActionResult::Failed);
		}
	}

	Super::OnChildFinished(Action, WithResult);
}

bool UDEPRECATED_PawnAction_Repeat::PushSubAction()
{
	if (ActionToRepeat_DEPRECATED == NULL)
	{
		Finish(EPawnActionResult::Failed);
		return false;
	}
	else if (RepeatsLeft == 0)
	{
		Finish(EPawnActionResult::Success);
		return true;
	}

	if (RepeatsLeft > 0)
	{
		--RepeatsLeft;
	}

	UDEPRECATED_PawnAction* ActionCopy = SubActionTriggeringPolicy == EPawnSubActionTriggeringPolicy::CopyBeforeTriggering 
		? Cast<UDEPRECATED_PawnAction>(StaticDuplicateObject(ActionToRepeat_DEPRECATED, this))
		: ToRawPtr(ActionToRepeat_DEPRECATED);

	UE_VLOG(GetPawn(), LogPawnAction, Log, TEXT("%s> pushing repeted action %s %s, repeats left: %d")
		, *GetName(), SubActionTriggeringPolicy == EPawnSubActionTriggeringPolicy::CopyBeforeTriggering ? TEXT("copy") : TEXT("instance")
		, *GetNameSafe(ActionCopy), RepeatsLeft);
	check(ActionCopy);
	RecentActionCopy_DEPRECATED = ActionCopy;
	return PushChildAction(*ActionCopy); 
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
