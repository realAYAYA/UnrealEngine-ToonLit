// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP_FirstPersonGameMode.h"
#include "TP_FirstPersonCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATP_FirstPersonGameMode::ATP_FirstPersonGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
