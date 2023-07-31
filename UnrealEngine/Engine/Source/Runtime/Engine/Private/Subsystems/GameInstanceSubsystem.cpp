// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameInstanceSubsystem)

UGameInstanceSubsystem::UGameInstanceSubsystem()
	: USubsystem()
{

}

UGameInstance* UGameInstanceSubsystem::GetGameInstance() const
{
	return Cast<UGameInstance>(GetOuter());
}
