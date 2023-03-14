// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputSubsystems.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputModule.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputSubsystems)


// **************************************************************************************************
// *
// * UEnhancedInputLocalPlayerSubsystem
// *
// **************************************************************************************************

UEnhancedPlayerInput* UEnhancedInputLocalPlayerSubsystem::GetPlayerInput() const
{	
	if (APlayerController* PlayerController = GetLocalPlayer()->GetPlayerController(GetWorld()))
	{
		return Cast<UEnhancedPlayerInput>(PlayerController->PlayerInput);
	}
	return nullptr;
}

void UEnhancedInputLocalPlayerSubsystem::ControlMappingsRebuiltThisFrame()
{
	ControlMappingsRebuiltDelegate.Broadcast();
}
