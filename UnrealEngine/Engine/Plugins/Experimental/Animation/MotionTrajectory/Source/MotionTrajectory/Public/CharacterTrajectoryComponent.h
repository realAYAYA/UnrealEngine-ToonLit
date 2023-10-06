// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "CharacterTrajectoryComponent.generated.h"

class UCharacterMovementComponent;

// Component for generating trajectories usable by Motion Matching. This component generates trajectories from ACharacter.
// This is intended to provide an example and starting point for using Motion Matching with a common setup using the default UCharacterMovementComponent.
// It is expected work flow to extend or replace this component for projects that use a custom movement component or custom movement modes.
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent), Experimental)
class MOTIONTRAJECTORY_API UCharacterTrajectoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	UCharacterTrajectoryComponent(const FObjectInitializer& ObjectInitializer);

	// Begin UActorComponent Interface
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	// End UActorComponent Interface

	// This can be called when bAutoUpdateTrajectory is false to manually control when trajectory updates.
	UFUNCTION(BlueprintCallable, Category = "Trajectory")
	void UpdateTrajectory(float DeltaSeconds);

	// This can be used to override the facing used for trajectory calculation.
	UFUNCTION(BlueprintNativeEvent, Category = "Trajectory")
	FRotator GetFacingFromMeshComponent(const USkeletalMeshComponent* MeshComponent) const;
	virtual FRotator GetFacingFromMeshComponent_Implementation(const USkeletalMeshComponent* MeshComponent) const;

protected:
	UFUNCTION()
	void OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	void UpdateHistory(float DeltaSeconds);
	void UpdatePrediction(const FVector& PositionWS, const FQuat& FacingWS, const FVector& VelocityWS, const FVector& AccelerationWS, const FRotator& ControllerRotationRate);

	FRotator CalculateControllerRotationRate(float DeltaSeconds, bool bShouldRemainVertical);

	// Trajectory stored in world space so it can be directly passed to Motion Matching.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Trajectory")
	FPoseSearchQueryTrajectory Trajectory;

	// This should generally match the longest history required by a Motion Matching Database in the project.
	// Motion Matching will use extrapolation to generate samples if the history doesn't contain enough samples.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	float HistoryLengthSeconds = 1.5f;

	// Higher values will cost more storage and processing time, but give higher accuracy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings", meta = (ClampMin = "1", ClampMax = "120"))
	int32 HistorySamplesPerSecond = 5;

	// This should match the longest trajectory prediction required by a Motion Matching Database in the project.
	// Motion Matching will use extrapolation to generate samples if the prediction doesn't contain enough samples.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	float PredictionLengthSeconds = 1.5f;

	// Higher values will cost more storage and processing time, but give higher accuracy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings", meta = (ClampMin = "1", ClampMax = "120"))
	int32 PredictionSamplesPerSecond = 5;

	// If the character is forward facing (i.e. bOrientRotationToMovement is true), this controls how quickly the trajectory will rotate
	// to face acceleration. It's common for this to differ from the rotation rate of the character, because animations are often authored 
	// with different rotation speeds than the character. This is especially true in cases where the character rotation snaps to movement.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	float RotateTowardsMovementSpeed = 10.f;

	// By default the component will always update trajectory. If desired, this can be disabled and the game can choose when to update.
	// For example, a game might want to only update trajectory for characters that are within view or very close to the local player.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	bool bAutoUpdateTrajectory = true;

	// Maximum controller rotation rate in degrees per second used to clamp the character owner controller desired rotation to generate the prediction trajectory.
	// Negative values disable the clamping behavior
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trajectory Settings")
	float MaxControllerRotationRate = -1.f;	

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkelMeshComponent;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> CharacterMovementComponent;

	int32 NumHistorySamples = -1;
	float SecondsPerHistorySample = 0.f;
	float SecondsPerPredictionSample = 0.f;

	// Forward axis for the SkeletalMeshComponent. It's common for skeletal mesh and animation data to not be X forward.
	FQuat ForwardFacingCS = FQuat::Identity;

	FRotator DesiredControllerRotationLastUpdate = FRotator::ZeroRotator;
};