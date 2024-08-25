// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveFloat.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PoseSearchTrajectoryLibrary.generated.h"

class UAnimInstance;

USTRUCT(BlueprintType)
struct POSESEARCH_API FPoseSearchTrajectoryData
{
public:
	GENERATED_BODY()

	struct FState
	{
		float DesiredControllerYawLastUpdate = 0.f;
	};

	struct FDerived
	{
		float ControllerYawRate = 0.f;

		float MaxSpeed = 0.f;
		float BrakingDeceleration = 0.f;
		float Friction = 0.f;

		FVector Velocity = FVector::ZeroVector;
		FVector Acceleration = FVector::ZeroVector;
		
		FVector Position = FVector::ZeroVector;
		FQuat Facing = FQuat::Identity;
		FQuat MeshCompRelativeRotation = FQuat::Identity;
		bool bOrientRotationToMovement = false;
		bool bStepGroundPrediction = true;
	};

	struct FSampling
	{
		int32 NumHistorySamples = 0;
		// if SecondsPerHistorySample <= 0, then we collect every update
		float SecondsPerHistorySample = 0.f;

		int32 NumPredictionSamples = 0;
		float SecondsPerPredictionSample = 0.f;
	};

	void UpdateData(float DeltaTime, const FAnimInstanceProxy& AnimInstanceProxy, FDerived& TrajectoryDataDerived, FState& TrajectoryDataState) const;
	void UpdateData(float DeltaTime, const UAnimInstance* AnimInstance, FDerived& TrajectoryDataDerived, FState& TrajectoryDataState) const;
	FVector StepCharacterMovementGroundPrediction(float DeltaTime, const FVector& InVelocity, const FVector& InAcceleration, const FDerived& TrajectoryDataDerived) const;
	
	// If the character is forward facing (i.e. bOrientRotationToMovement is true), this controls how quickly the trajectory will rotate
	// to face acceleration. It's common for this to differ from the rotation rate of the character, because animations are often authored 
	// with different rotation speeds than the character. This is especially true in cases where the character rotation snaps to movement.
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings")
	float RotateTowardsMovementSpeed = 10.f;

	// Maximum controller yaw  rate in degrees per second used to clamp the character owner controller desired yaw to generate the prediction trajectory.
	// Negative values disable the clamping behavior
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings")
	float MaxControllerYawRate = 70.f;

	// artificially bend character velocity towards acceleration direction to compute trajectory prediction, to get sharper turns
	// 0: character velocity is used with no alteration, 1: the acceleration direction is used as velocity direction
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float BendVelocityTowardsAcceleration = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (InlineEditConditionToggle))
	bool bUseSpeedRemappingCurve = false;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (EditCondition = "bUseSpeedRemappingCurve"))
	FRuntimeFloatCurve SpeedRemappingCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (InlineEditConditionToggle))
	bool bUseAccelerationRemappingCurve = false;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory Settings", meta = (EditCondition = "bUseAccelerationRemappingCurve"))
	FRuntimeFloatCurve AccelerationRemappingCurve;
};

USTRUCT(BlueprintType)
struct POSESEARCH_API FPoseSearchTrajectory_WorldCollisionResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Trajectory Settings")
	float TimeToLand  = 0.0f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Trajectory Settings")
	float LandSpeed  = 0.0f;
};

/**
 * Set of functions to help populate a FPoseSearchQueryTrajectory for motion matching.
 */
UCLASS()
class POSESEARCH_API UPoseSearchTrajectoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void InitTrajectorySamples(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Update history by tracking offsets that result from character intent (e.g. movement component velocity) and applying
	// that to the current world transform. This works well on moving platforms as it only stores a history of movement
	// that results from character intent, not movement from platforms.
	static void UpdateHistory_TransformHistory(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Update prediction by simulating the movement math for ground locomotion from UCharacterMovementComponent.
	static void UpdatePrediction_SimulateCharacterMovement(FPoseSearchQueryTrajectory& Trajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FDerived& TrajectoryDataDerived, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, float DeltaTime);

	// Generates a prediction trajectory based of the current character intent.
	UFUNCTION(BlueprintCallable, Category = "Animation|PoseSearch", meta = (BlueprintThreadSafe, DisplayName = "Pose Search Generate Trajectory"))
	static void PoseSearchGenerateTrajectory(const UAnimInstance* InAnimInstance, UPARAM(ref) const FPoseSearchTrajectoryData& InTrajectoryData, float InDeltaTime,
		UPARAM(ref) FPoseSearchQueryTrajectory& InOutTrajectory, UPARAM(ref) float& InOutDesiredControllerYawLastUpdate, FPoseSearchQueryTrajectory& OutTrajectory,
		float InHistorySamplingInterval = 0.04f, int32 InTrajectoryHistoryCount = 10, float InPredictionSamplingInterval = 0.2f, int32 InTrajectoryPredictionCount = 8);

	// Experimental: Process InTrajectory to apply gravity and handle collisions. Eventually returns the modified OutTrajectory.
	// If bApplyGravity is true, gravity from the UCharacterMovementComponent will be applied.
	// If FloorCollisionsOffset > 0, vertical collision will be performed to every sample of the trajectory to have the samples float over the geometry (by FloorCollisionsOffset).
	UFUNCTION(BlueprintCallable, Category="Animation|PoseSearch|Experimental", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceChannel,bTraceComplex,ActorsToIgnore,DrawDebugType,bIgnoreSelf,MaxObstacleHeight,TraceColor,TraceHitColor,DrawTime"))
	static void HandleTrajectoryWorldCollisions(const UObject* WorldContextObject, const UAnimInstance* AnimInstance, UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, bool bApplyGravity, float FloorCollisionsOffset, FPoseSearchQueryTrajectory& OutTrajectory, FPoseSearchTrajectory_WorldCollisionResults& CollisionResult,
		ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, bool bIgnoreSelf = true, float MaxObstacleHeight = 10000.f, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe), Category="Animation|PoseSearch")
	static void GetTrajectorySampleAtTime(UPARAM(ref) const FPoseSearchQueryTrajectory& InTrajectory, float Time, FPoseSearchQueryTrajectorySample& OutTrajectorySample, bool bExtrapolate = false);

private:
	static FVector RemapVectorMagnitudeWithCurve(const FVector& Vector, bool bUseCurve, const FRuntimeFloatCurve& Curve);
};