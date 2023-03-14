// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassNavigationFragments.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

void FMassMoveTargetFragment::CreateNewAction(const EMassMovementAction InAction, const UWorld& InWorld)
{
	ensureMsgf(InWorld.GetNetMode() != NM_Client, TEXT("This version of SetDesiredAction should only be called on the authority."));
	CurrentAction = InAction;
	CurrentActionID++;
	CurrentActionWorldStartTime = InWorld.TimeSeconds;

	const AGameStateBase* GameState = InWorld.GetGameState();
	CurrentActionServerStartTime = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : CurrentActionWorldStartTime;

	MarkNetDirty();
}

void FMassMoveTargetFragment::CreateReplicatedAction(const EMassMovementAction InAction, const uint16 InActionID, const float InWorldStartTime, const float InServerStartTime)
{
	CurrentAction = InAction;
	CurrentActionID = InActionID;
	CurrentActionWorldStartTime = InWorldStartTime;
	CurrentActionServerStartTime = InServerStartTime;
}

FString FMassMoveTargetFragment::ToString() const
{
	return FString::Printf(TEXT("ActionID:%d Action:%s StartTime: World:%.1f Server:%.1f DesiredSpeed:%.1f "),
		CurrentActionID,
		*UEnum::GetValueAsString(CurrentAction),
		CurrentActionWorldStartTime,
		CurrentActionServerStartTime,
		DesiredSpeed.Get());
}