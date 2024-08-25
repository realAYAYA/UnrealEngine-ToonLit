// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/SwimmingMode.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "PhysicsDrivenSwimmingMode.generated.h"


namespace Chaos { class FCharacterGroundConstraint; }

/**
 * PhysicsDrivenSwimmingMode: Override base kinematic Swimming mode for physics based motion.
 */
UCLASS(Blueprintable, BlueprintType)
class MOVER_API UPhysicsDrivenSwimmingMode : public USwimmingMode, public IPhysicsCharacterMovementModeInterface
{
	GENERATED_UCLASS_BODY()

public:

	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	virtual bool AttemptJump(float UpwardsSpeed, FMoverTickEndData& Output);
	virtual bool AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output);

	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	// Target height for the character. This is the desired distance from the center of the capsule to the floor
	UPROPERTY(EditAnywhere, Category = "Physics Mover", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float TargetHeight = 54.0f;
};
