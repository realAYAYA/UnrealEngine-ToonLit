// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "CharacterMovementTrajectoryLibrary.generated.h"

class UCharacterMovementComponent;

/**
 * Library of functions useful for generating trajectories based on the behavior of the UCharacterMovementComponent.
 */
UCLASS()
class MOTIONTRAJECTORY_API UCharacterMovementTrajectoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Approximate the behavior of the UCharacterMovementComponent during ground locomotion.
	 * @param InDeltaTime - Length of time to predict forward. Running multiple shorter iterations will generate more accurate results than one large time step.
	 * @param InVelocity - Current velocity.
	 * @param InAcceleration - Current acceleration.
	 * @param InCharacterMovementComponent - Tuning values, such as GroundFriction, are read from the movement component.
	 * @param OutVelocity - The resulting predicted velocity.
	 */
	UFUNCTION(BlueprintPure, Category = "Motion Trajectory")
	static void StepCharacterMovementGroundPrediction(
		float InDeltaTime, const FVector& InVelocity, const FVector& InAcceleration, const UCharacterMovementComponent* InCharacterMovementComponent,
		FVector& OutVelocity);
};