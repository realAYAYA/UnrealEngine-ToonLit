// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/Subsystem.h"

#include "LocalPlayerSubsystem.generated.h"

class ULocalPlayer;

/**
 * ULocalPlayerSubsystem
 * Base class for auto instanced and initialized subsystem that share the lifetime of local players
 */
UCLASS(Abstract, Within = LocalPlayer)
class ENGINE_API ULocalPlayerSubsystem : public USubsystem
{
	GENERATED_BODY()

public:
	ULocalPlayerSubsystem();

	template<typename LocalPlayerType = ULocalPlayer>
	LocalPlayerType* GetLocalPlayer() const
	{
		return Cast<LocalPlayerType>(GetOuter());
	}

	template<typename LocalPlayerType = ULocalPlayer>
	LocalPlayerType* GetLocalPlayerChecked() const
	{
		return CastChecked<LocalPlayerType>(GetOuter());
	}
};
