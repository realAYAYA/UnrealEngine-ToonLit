// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::MassMovement::Delegates
{
#if WITH_EDITOR
	/** Called when movement names have changed (UI update). */
	DECLARE_MULTICAST_DELEGATE(FOnMassMovementNamesChanged);
	extern MASSMOVEMENT_API FOnMassMovementNamesChanged OnMassMovementNamesChanged;
#endif // WITH_EDITOR
} // UE::MassMovement::Delegates
