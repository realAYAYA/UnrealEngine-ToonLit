// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/GenSources/PCGGenSourcePlayer.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGGenSourcePlayer)

TOptional<FVector> UPCGGenSourcePlayer::GetPosition() const
{
	if (PlayerController.IsValid())
	{
		if (APawn* PlayerPawn = PlayerController->GetPawn())
		{
			return PlayerPawn->GetActorLocation();
		}
	}

	return TOptional<FVector>();
}

TOptional<FVector> UPCGGenSourcePlayer::GetDirection() const
{
	if (PlayerController.IsValid())
	{
		if (APawn* PlayerPawn = PlayerController->GetPawn())
		{
			return PlayerPawn->GetActorForwardVector();
		}
	}

	return TOptional<FVector>();
}

void UPCGGenSourcePlayer::SetPlayerController(const APlayerController* InPlayerController)
{
	PlayerController = InPlayerController;
}
