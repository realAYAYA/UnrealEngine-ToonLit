// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassMovementTypes.h"
#include "HAL/IConsoleManager.h"

namespace UE::MassMovement
{
	int32 bFreezeMovement = 0;
	FAutoConsoleVariableRef CVarFreezeMovement(TEXT("ai.debug.mass.FreezeMovement"), bFreezeMovement, TEXT("Freeze any movement by common movement processors."));

} // UE::MassMovement
