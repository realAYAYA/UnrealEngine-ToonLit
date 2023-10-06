// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterMovementTrajectoryLibrary.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "MotionTrajectory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterMovementTrajectoryLibrary)

void UCharacterMovementTrajectoryLibrary::StepCharacterMovementGroundPrediction(
	float InDeltaTime, const FVector& InVelocity, const FVector& InAcceleration, const UCharacterMovementComponent* InCharacterMovementComponent,
	FVector& OutVelocity)
{
	OutVelocity = InVelocity;

	if (InCharacterMovementComponent == nullptr)
	{
		UE_LOG(LogMotionTrajectory, Warning, TEXT("UCharacterMovementTrajectoryLibrary::StepCharacterMovementGroundPrediction requires a valid CharacterMovementComponent"));
		return;
	}

	// Braking logic is copied from UCharacterMovementComponent::ApplyVelocityBraking()
	if (InAcceleration.IsZero())
	{
		if (InVelocity.IsZero())
		{
			return;
		}

		float ActualBrakingFriction = InCharacterMovementComponent->bUseSeparateBrakingFriction ? InCharacterMovementComponent->BrakingFriction : InCharacterMovementComponent->GroundFriction;
		const float FrictionFactor = FMath::Max(0.f, InCharacterMovementComponent->BrakingFrictionFactor);
		ActualBrakingFriction = FMath::Max(0.f, ActualBrakingFriction * FrictionFactor);

		const float BrakingDeceleration = FMath::Max(0.f, InCharacterMovementComponent->GetMaxBrakingDeceleration());
		const bool bZeroFriction = (ActualBrakingFriction == 0.f);
		const bool bZeroBraking = (BrakingDeceleration == 0.f);

		if (bZeroFriction && bZeroBraking)
		{
			return;
		}

		float RemainingTime = InDeltaTime;
		const float MaxTimeStep = FMath::Clamp(InCharacterMovementComponent->BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

		const FVector PrevLinearVelocity = OutVelocity;
		const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * OutVelocity.GetSafeNormal()));

		// Decelerate to brake to a stop
		while (RemainingTime >= UCharacterMovementComponent::MIN_TICK_TIME)
		{
			// Zero friction uses constant deceleration, so no need for iteration.
			const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
			RemainingTime -= dt;

			// apply friction and braking
			OutVelocity = OutVelocity + ((-ActualBrakingFriction) * OutVelocity + RevAccel) * dt;

			// Don't reverse direction
			if ((OutVelocity | PrevLinearVelocity) <= 0.f)
			{
				OutVelocity = FVector::ZeroVector;
				return;
			}
		}

		// Clamp to zero if nearly zero, or if below min threshold and braking
		const float VSizeSq = OutVelocity.SizeSquared();
		if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(UCharacterMovementComponent::BRAKE_TO_STOP_VELOCITY)))
		{
			OutVelocity = FVector::ZeroVector;
		}
	}
	// Acceleration logic is copied from  UCharacterMovementComponent::CalcVelocity
	else
	{
		const FVector AccelDir = InAcceleration.GetSafeNormal();
		const float VelSize = OutVelocity.Size();

		OutVelocity = OutVelocity - (OutVelocity - AccelDir * VelSize) * FMath::Min(InDeltaTime * InCharacterMovementComponent->GroundFriction, 1.f);

		const float MaxInputSpeed = FMath::Max(InCharacterMovementComponent->GetMaxSpeed() * InCharacterMovementComponent->GetAnalogInputModifier(), InCharacterMovementComponent->GetMinAnalogSpeed());
		OutVelocity += InAcceleration * InDeltaTime;
		OutVelocity = OutVelocity.GetClampedToMaxSize(MaxInputSpeed);
	}
}