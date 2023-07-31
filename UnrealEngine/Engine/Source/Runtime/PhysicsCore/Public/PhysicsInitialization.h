// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "PhysicsCore.h"

namespace PhysDLLHelper
{
	/**
	 *	Load the required modules for PhysX
	 */
	PHYSICSCORE_API bool LoadPhysXModules(bool bLoadCooking);

	/**
	 *	Unload the required modules for PhysX
	 */
	PHYSICSCORE_API void UnloadPhysXModules();
}

bool PHYSICSCORE_API InitGamePhysCore();
void PHYSICSCORE_API TermGamePhysCore();