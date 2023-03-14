// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MotionTrajectory.h"

#include "MotionTrajectoryCharacterMovement.generated.h"

class UCharacterMovementComponent;

// Example motion trajectory component implementation for encapsulating: Character Movement ground locomotion
UCLASS(meta=(BlueprintSpawnableComponent), Category="Motion Trajectory")
class MOTIONTRAJECTORY_API UCharacterMovementTrajectoryComponent : public UMotionTrajectoryComponent
{
	GENERATED_BODY()

protected:

	// Begin UMotionTrajectoryComponent Interface
	virtual FTrajectorySample CalcWorldSpacePresentTrajectorySample(float DeltaTime) const override;
	virtual void TickTrajectory(float DeltaTime) override;
	// End UMotionTrajectoryComponent Interface

	UFUNCTION()
	void OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UPROPERTY(Transient)
	FRotator LastDesiredControlRotation = FRotator::ZeroRotator;
	
	UPROPERTY(Transient)
	FRotator DesiredControlRotationVelocity = FRotator::ZeroRotator;

public:

	UCharacterMovementTrajectoryComponent(const FObjectInitializer& ObjectInitializer);

	// Begin UActorComponent Interface
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	// End UActorComponent Interface

	// Begin UMotionTrajectoryComponent Interface
	virtual FTrajectorySampleRange GetTrajectory() const override;
	virtual FTrajectorySampleRange GetTrajectoryWithSettings(const FMotionTrajectorySettings& Settings, bool bIncludeHistory) const override;
	// End UMotionTrajectoryComponent Interface


protected:
	// Predicts future trajectory and writes prediction to ReturnValue
	virtual void PredictTrajectory(
		int32 SampleRate,
		int32 MaxSamples,
		const FMotionTrajectorySettings& Settings,
		const FTrajectorySample& PresentTrajectory,
		const FRotator& DesiredControlRotationVelocity,
		FTrajectorySampleRange& OutTrajectoryRange) const;

	// Updates InOutSample predicting IntegrationDelta forward
	virtual void StepPrediction(
		float IntegrationDelta,
		const FRotator& ControlRotationVelocity,
		FRotator& InOutControlRotationTotalDelta,
		FTrajectorySample& InOutSample) const;

	// The methods below allow tweaking movement model values per prediction sample

	// Returns the friction to be used when updating Sample
	virtual float GetFriction(
		const UCharacterMovementComponent* MoveComponent, 
		const FTrajectorySample& Sample, 
		float DeltaSeconds) const;

	// Returns maximum deceleration of Sample when the character is braking
	virtual float GetMaxBrakingDeceleration(
		const UCharacterMovementComponent* MoveComponent, 
		const FTrajectorySample& Sample, 
		float DeltaSeconds) const;

	// Returns input acceleration in world space when predicting how Sample will change DeltaSeconds in the future
	virtual FVector GetAccelerationWS(
		const UCharacterMovementComponent* MoveComponent, 
		const FTrajectorySample& Sample, 
		float DeltaSeconds) const;

	// This function outputs a new rotator obtained by integrating Sample forward by IntegrationDelta based on the
	// dynamics specified in MovementComponent
	virtual FRotator StepRotationWS(
		const UCharacterMovementComponent* MovementComponent,
		const FTrajectorySample& Sample,
		const FRotator& PrevRotator,
		const FRotator& ControlRotationTotalDelta,
		float IntegrationDelta) const;
};