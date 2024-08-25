// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/MovementMixer.h"
#include "LayeredMove.h"
#include "MoverLog.h"

UMovementMixer::UMovementMixer()
	: CurrentHighestPriority(0)
	, CurrentLayeredMoveStartTime(TNumericLimits<float>::Max())
{
}

void UMovementMixer::MixLayeredMove(const FLayeredMoveBase& ActiveMove, const FProposedMove& MoveStep, FProposedMove& OutCumulativeMove)
{
	if (OutCumulativeMove.PreferredMode != MoveStep.PreferredMode && !OutCumulativeMove.PreferredMode.IsNone() && !MoveStep.PreferredMode.IsNone())
	{
		UE_LOG(LogMover, Log, TEXT("Multiple LayeredMoves are conflicting with preferred moves. %s will override %s"),
			*MoveStep.PreferredMode.ToString(), *OutCumulativeMove.PreferredMode.ToString());
	}

	if (MoveStep.bHasDirIntent && OutCumulativeMove.MixMode != EMoveMixMode::OverrideAll && ActiveMove.Priority >= CurrentHighestPriority)
	{
		if (OutCumulativeMove.bHasDirIntent)
		{
			UE_LOG(LogMover, Log, TEXT("Multiple LayeredMoves are setting direction intent and the layered move with highest priority will be used."));
		}
				
		OutCumulativeMove.bHasDirIntent = MoveStep.bHasDirIntent;
		OutCumulativeMove.DirectionIntent = MoveStep.DirectionIntent;
	}

	if (ActiveMove.MixMode == EMoveMixMode::OverrideVelocity)
	{
		if (CheckPriority(&ActiveMove, CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EMoveMixMode::OverrideAll)
			{
				UE_LOG(LogMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}

			if (!MoveStep.PreferredMode.IsNone())
			{
				OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			}
				
			OutCumulativeMove.MixMode = EMoveMixMode::OverrideVelocity;
			OutCumulativeMove.LinearVelocity  = MoveStep.LinearVelocity;
			OutCumulativeMove.AngularVelocity = MoveStep.AngularVelocity;
		}
	}
	else if (ActiveMove.MixMode == EMoveMixMode::AdditiveVelocity)
	{
		if (OutCumulativeMove.MixMode != EMoveMixMode::OverrideVelocity && OutCumulativeMove.MixMode != EMoveMixMode::OverrideAll)
		{
			if (!MoveStep.PreferredMode.IsNone())
			{
				OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			}

			OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			OutCumulativeMove.LinearVelocity += MoveStep.LinearVelocity;
			OutCumulativeMove.AngularVelocity += MoveStep.AngularVelocity;
		}
	}
	else if (ActiveMove.MixMode == EMoveMixMode::OverrideAll)
	{
		if (CheckPriority(&ActiveMove, CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EMoveMixMode::OverrideAll)
			{
				UE_LOG(LogMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}
				
			OutCumulativeMove = MoveStep;
		}
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled move mix mode was found."));
	}
}

void UMovementMixer::MixProposedMoves(const FProposedMove& MoveToMix, FProposedMove& OutCumulativeMove)
{
	if (MoveToMix.bHasDirIntent && OutCumulativeMove.MixMode != EMoveMixMode::OverrideAll)
	{
		OutCumulativeMove.bHasDirIntent = MoveToMix.bHasDirIntent;
		OutCumulativeMove.DirectionIntent = MoveToMix.DirectionIntent;
	}

	// Combine movement parameters from layered moves into what the mode wants to do
	if (MoveToMix.MixMode == EMoveMixMode::OverrideAll)
	{
		OutCumulativeMove = MoveToMix;
	}
	else if (MoveToMix.MixMode == EMoveMixMode::AdditiveVelocity)
	{
		OutCumulativeMove.LinearVelocity += MoveToMix.LinearVelocity;
		OutCumulativeMove.AngularVelocity += MoveToMix.AngularVelocity;
	}
	else if (MoveToMix.MixMode == EMoveMixMode::OverrideVelocity)
	{
		OutCumulativeMove.LinearVelocity = MoveToMix.LinearVelocity;
		OutCumulativeMove.AngularVelocity = MoveToMix.AngularVelocity;
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled move mix mode was found."));
	}
}

void UMovementMixer::ResetMixerState()
{
	CurrentHighestPriority = 0;
	CurrentLayeredMoveStartTime = TNumericLimits<float>::Max();
}

bool UMovementMixer::CheckPriority(const FLayeredMoveBase* LayeredMove, uint8& InOutHighestPriority, float& InOutCurrentLayeredMoveStartTimeMs)
{
	if (LayeredMove->Priority > InOutHighestPriority)
	{
		InOutHighestPriority = LayeredMove->Priority;
		InOutCurrentLayeredMoveStartTimeMs = LayeredMove->StartSimTimeMs;
		return true;
	}
	if (LayeredMove->Priority == InOutHighestPriority && LayeredMove->StartSimTimeMs < InOutCurrentLayeredMoveStartTimeMs)
	{
		InOutCurrentLayeredMoveStartTimeMs = LayeredMove->StartSimTimeMs;
		return true;
	}

	return false;
}
