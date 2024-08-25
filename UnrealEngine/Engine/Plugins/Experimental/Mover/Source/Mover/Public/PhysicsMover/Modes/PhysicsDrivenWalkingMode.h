// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsDrivenWalkingMode.generated.h"


namespace Chaos { class FCharacterGroundConstraint; }
struct FWaterCheckResult;

/**
 * PhysicsDrivenWalkingMode: Override base kinematic walking mode for physics based motion.
 */
UCLASS(Blueprintable, BlueprintType)
class MOVER_API UPhysicsDrivenWalkingMode : public UWalkingMode, public IPhysicsCharacterMovementModeInterface
{
	GENERATED_UCLASS_BODY()

public:

	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	virtual bool AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output);

	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const override;
	virtual void OnContactModification_Internal(const FPhysicsMoverSimulationContactModifierParams& Params, Chaos::FCollisionContactModifier& Modifier) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	// Maximum force the character can apply to reach the motion target
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Newtons"))
	float RadialForceLimit = 1500.0f;

	// Maximum force the character can apply to hold in place while standing on an unwalkable incline
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Newtons"))
	float FrictionForceLimit = 100.0f;

	// Maximum torque the character can apply to rotate in air about the vertical axis
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float TwistTorqueLimit = 1000.0f;

	// Maximum torque the character can apply to remain upright
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "NewtonMeters"))
	float SwingTorqueLimit = 3000.0f;

	// Target height for the character. This is the desired distance from the center of the capsule to the floor
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float TargetHeight = 95.0f;

	// Damping factor to control the softness of the interaction between the character and the ground
	// Set to 0 for no damping and 1 for maximum damping
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float GroundDamping = 0.0f;

	// With this enabled, in addition to the constraint radial force, the mover applies an initial velocity to reach the target
	// movement. This allows for a separation between pushing force and force to reach the target movement.
	// A value of 1 means the character will always be able to reach the target motion (if there are no obstacles) regardless of
	// the radial force limit.
	// A value of 0 means the only thing moving the character is the constraint
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalVelocityToTarget = 1.0f;

	// Controls how much downward velocity is applied to keep the character rooted to the ground when the character
	// is within MaxStepHeight of the ground surface.
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float FractionalDownwardVelocityToTarget = 1.0f;

	// Time limit for being unsupported before moving from a walking to a falling state.
	// This provides some grace period when walking off of an edge during which locomotion
	// and jumping are still possible even though the character has started falling under gravity
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float MaxUnsupportedTimeBeforeFalling = 0.06f;

protected:
	void SwitchToState(const FName& StateName, const FSimulationTickParams& Params, FMoverTickEndData& OutputState);

	bool CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const;

	void FloorCheck(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, UPrimitiveComponent* UpdatedPrimitive, float DeltaSeconds,
		FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const;
};
