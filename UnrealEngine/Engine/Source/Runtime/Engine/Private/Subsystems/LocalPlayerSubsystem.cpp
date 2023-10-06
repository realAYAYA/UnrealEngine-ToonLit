// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalPlayerSubsystem)

ULocalPlayerSubsystem::ULocalPlayerSubsystem()
	: USubsystem()
{
}

void ULocalPlayerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	
}

