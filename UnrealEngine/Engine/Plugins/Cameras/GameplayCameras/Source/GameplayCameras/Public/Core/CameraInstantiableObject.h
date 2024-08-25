// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/Object.h"

#include "CameraInstantiableObject.generated.h"

/**
 *
 */
UENUM()
enum class ECameraNodeInstantiationState
{
	None,
	HasInstantiations,
	IsInstantiated
};

/**
 *
 */
UCLASS(MinimalAPI)
class UCameraInstantiableObject : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR

public:

	/** Gets the instantiation state of this object. */
	ECameraNodeInstantiationState GetInstantiationState() const { return InstantiationState; }
	/** Sets the instantiation state of this object. */
	void SetInstantiationState(ECameraNodeInstantiationState bInValue) { InstantiationState = bInValue; }

	/** Returns whether this object is an instantiated object. */
	bool IsInstantiated() const { return EnumHasAnyFlags(InstantiationState, ECameraNodeInstantiationState::IsInstantiated); }
	/** Returns whether this object has any known instantiations. */
	bool HasInstantiations() const { return EnumHasAnyFlags(InstantiationState, ECameraNodeInstantiationState::HasInstantiations); }

private:

	ECameraNodeInstantiationState InstantiationState = ECameraNodeInstantiationState::None;

#endif  // WITH_EDITOR
};

