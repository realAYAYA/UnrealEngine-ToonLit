// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPhysicalMaterial;

/** Set of delegates to allowing hooking different parts of the physics engine */
class PHYSICSCORE_API FPhysicsDelegatesCore
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdatePhysXMaterial, UPhysicalMaterial*);
	static FOnUpdatePhysXMaterial OnUpdatePhysXMaterial;
};
