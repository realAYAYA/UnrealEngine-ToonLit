// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovementModeTransition.h"
#include "PhysicsJumpCheck.generated.h"

/**
 * Transition that handles jumping based on input for a physics-based character
 */
UCLASS(Blueprintable, BlueprintType)
class MOVER_API UPhysicsJumpCheck : public UBaseMovementModeTransition
{
	GENERATED_UCLASS_BODY()


public:
	virtual FTransitionEvalResult OnEvaluate(const FSimulationTickParams& Params) const override;
	virtual void OnTrigger(const FSimulationTickParams& Params) override;

	/** Instantaneous speed induced in an actor upon jumping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeed = 500.0f;

	/** Name of movement mode to transition to when jumping is activated */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Parameters)
	FName TransitionToMode;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR


};