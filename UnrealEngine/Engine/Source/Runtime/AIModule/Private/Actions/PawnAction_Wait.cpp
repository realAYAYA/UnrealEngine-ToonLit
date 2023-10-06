// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/PawnAction_Wait.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnAction_Wait)

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_PawnAction_Wait::UDEPRECATED_PawnAction_Wait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimeToWait(0.f)
	, FinishTimeStamp(0.f)
{

}

UDEPRECATED_PawnAction_Wait* UDEPRECATED_PawnAction_Wait::CreateAction(UWorld& World, float InTimeToWait)
{
	UDEPRECATED_PawnAction_Wait* Action = UDEPRECATED_PawnAction::CreateActionInstance<UDEPRECATED_PawnAction_Wait>(World);
	
	if (Action != NULL)
	{
		Action->TimeToWait = InTimeToWait;
	}

	return Action;
}

bool UDEPRECATED_PawnAction_Wait::Start()
{
	if (Super::Start())
	{
		if (TimeToWait >= 0)
		{
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UDEPRECATED_PawnAction_Wait::TimerDone, TimeToWait);
		}
		// else hang in there for ever!

		return true;
	}

	return false;
}

bool UDEPRECATED_PawnAction_Wait::Pause(const UDEPRECATED_PawnAction* PausedBy)
{
	GetWorld()->GetTimerManager().PauseTimer(TimerHandle);
	return true;
}

bool UDEPRECATED_PawnAction_Wait::Resume()
{
	GetWorld()->GetTimerManager().UnPauseTimer(TimerHandle);
	return true;
}

EPawnActionAbortState::Type UDEPRECATED_PawnAction_Wait::PerformAbort(EAIForceParam::Type ShouldForce)
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	return EPawnActionAbortState::AbortDone;
}

void UDEPRECATED_PawnAction_Wait::TimerDone()
{
	Finish(EPawnActionResult::Success);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
