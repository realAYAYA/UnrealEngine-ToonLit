// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"

#include "CameraModeTransition.generated.h"

class UBlendCameraNode;
class UCameraAsset;
class UCameraMode;

/**
 * Parameter structure for camera transitions.
 */
struct FCameraModeTransitionConditionMatchParams
{
	/** The previous camera mode. */
	const UCameraMode* FromCameraMode = nullptr;
	/** The previous camera asset. */
	const UCameraAsset* FromCameraAsset = nullptr;

	/** The next camera mode. */
	const UCameraMode* ToCameraMode = nullptr;
	/** The next camera asset. */
	const UCameraAsset* ToCameraAsset = nullptr;
};

/**
 * Base class for a camera transition condition.
 */
UCLASS(Abstract, DefaultToInstanced, MinimalAPI)
class UCameraModeTransitionCondition : public UObject
{
	GENERATED_BODY()

public:

	/** Evaluates whether this transition should be used. */
	bool TransitionMatches(const FCameraModeTransitionConditionMatchParams& Params) const;

protected:

	/** Evaluates whether this transition should be used. */
	virtual bool OnTransitionMatches(const FCameraModeTransitionConditionMatchParams& Params) const { return false; }
};

/**
 * A camera transition.
 */
USTRUCT()
struct FCameraModeTransition
{
	GENERATED_BODY()

	/** The list of conditions that must pass for this transition to be used. */
	UPROPERTY(EditAnywhere, Instanced, Category=Common)
	TArray<TObjectPtr<UCameraModeTransitionCondition>> Conditions;

	/** The blend to use to blend a given camera mode in or out. */
	UPROPERTY(EditAnywhere, Instanced, Category=Common)
	TObjectPtr<UBlendCameraNode> Blend;
};

