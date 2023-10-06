// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPhysicalMaterial;

/** Set of delegates to allowing hooking different parts of the physics engine */
class FPhysicsDelegatesCore
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdatePhysXMaterial, UPhysicalMaterial*);
	static PHYSICSCORE_API FOnUpdatePhysXMaterial OnUpdatePhysXMaterial;
};
