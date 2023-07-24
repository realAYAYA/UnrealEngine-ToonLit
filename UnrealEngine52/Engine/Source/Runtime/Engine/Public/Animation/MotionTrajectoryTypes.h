// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/BinarySearch.h"
#include "UObject/ObjectMacros.h"
#include "MotionTrajectoryTypes.generated.h"

USTRUCT(BlueprintType, Category="Motion Trajectory")
struct ENGINE_API FTrajectorySample
{
	GENERATED_BODY()

	// The relative accumulated time that this sample is associated with
	// Zero value for instantaneous, negative values for the past, and positive values for the future
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	float AccumulatedSeconds = 0.f;

	// Position relative to the sampled in-motion object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	FTransform Transform = FTransform::Identity;

	// Linear velocity relative to the sampled in-motion object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	FVector LinearVelocity = FVector::ZeroVector;

	// Linear interpolation of all parameters of two trajectory samples
	FTrajectorySample Lerp(const FTrajectorySample& Sample, float Alpha) const;

	// Centripetal Catmull–Rom spline interpolation of all parameters of two trajectory samples
	FTrajectorySample SmoothInterp(const FTrajectorySample& PrevSample
		, const FTrajectorySample& Sample
		, const FTrajectorySample& NextSample
		, float Alpha) const;

	// Concatenates DeltaTransform before the current transform is applied and shifts the accumulated time by 
	// DeltaSeconds
	void PrependOffset(const FTransform DeltaTransform, float DeltaSeconds);

	void TransformReferenceFrame(const FTransform DeltaTransform);

	// Determines if all sample properties are zeroed
	bool IsZeroSample() const;
};

// A container of ordered trajectory samples and associated sampling rate
USTRUCT(BlueprintType, Category="Motion Trajectory")
struct ENGINE_API FTrajectorySampleRange
{
	GENERATED_BODY()

	// Linearly ordered container for past, present, and future trajectory samples
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion Trajectory")
	TArray<FTrajectorySample> Samples;

	// Removes history samples from trajectory (retains present and future)
	void RemoveHistory();

	// Rotates all samples in the trajectory
	void Rotate(const FQuat& Rotation);

	// Interpolates transform over time
	void TransformOverTime(const FTransform& Transform, float StartTime, float DeltaTime);

	// Rotates all samples in the trajectory
	void TransformReferenceFrame(const FTransform& Transform);

	// Determine if any trajectory samples are present
	bool HasSamples() const;

	// Determine if all trajectory samples are default values
	bool HasOnlyZeroSamples() const;

	FTrajectorySample GetSampleAtTime(float Time, bool bExtrapolate = false) const;

	// Debug draw in-world trajectory samples and optional corresponding information
	void DebugDrawTrajectory(bool bEnable
		, const UWorld* World
		, const FTransform& WorldTransform
		, const FLinearColor PredictionColor = { 0.f, 1.f, 0.f }
		, const FLinearColor HistoryColor = { 0.f, 0.f, 1.f }
		, float TransformScale = 10.f
		, float TransformThickness = 2.f
		, float VelArrowScale = 0.025f
		, float VelArrowSize = 40.f
		, float VelArrowThickness = 2.f) const;
};