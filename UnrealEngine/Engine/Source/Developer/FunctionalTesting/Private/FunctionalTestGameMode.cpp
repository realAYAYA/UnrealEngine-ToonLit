// Copyright Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestGameMode.h"
#include "GameFramework/SpectatorPawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FunctionalTestGameMode)

AFunctionalTestGameMode::AFunctionalTestGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

