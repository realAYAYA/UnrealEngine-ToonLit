// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/Subsystem.h"

#include "GameInstanceSubsystem.generated.h"

class UGameInstance;

/**
 * UGameInstanceSubsystem
 * Base class for auto instanced and initialized systems that share the lifetime of the game instance
 */
UCLASS(Abstract, Within = GameInstance)
class ENGINE_API UGameInstanceSubsystem : public USubsystem
{
	GENERATED_BODY()

public:
	UGameInstanceSubsystem();

	UGameInstance* GetGameInstance() const;

};
