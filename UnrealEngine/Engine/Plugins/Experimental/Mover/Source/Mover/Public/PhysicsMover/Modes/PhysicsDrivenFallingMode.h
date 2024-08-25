// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsDrivenFallingMode.generated.h"


namespace Chaos { class FCharacterGroundConstraint; }

/**
 * PhysicsDrivenFallingMode: Override base kinematic falling mode for physics based motion.
 */
UCLASS(Blueprintable, BlueprintType)
class MOVER_API UPhysicsDrivenFallingMode : public UFallingMode, public IPhysicsCharacterMovementModeInterface
{
	GENERATED_UCLASS_BODY()

public:

	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	// Maximum torque the character can apply to rotate in air about the vertical axis
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float TwistTorqueLimit = 0.0f;

	// Maximum torque the character can apply to remain upright
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float SwingTorqueLimit = 3000.0f;

	// Target height for the character. This is the desired distance from the center of the capsule to the floor
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float TargetHeight = 95.0f;
};
