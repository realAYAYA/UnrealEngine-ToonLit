// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimTypes.h"
#include "Animation/MotionTrajectoryTypes.h"

#include "PoseSearchTrajectoryTypes.generated.h"

struct FAnimInstanceProxy;

USTRUCT(BlueprintType, Category="Pose Search Trajectory")
struct POSESEARCH_API FPoseSearchQueryTrajectorySample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose Search Query Trajectory")
	FQuat Facing = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose Search Query Trajectory")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose Search Query Trajectory")
	float AccumulatedSeconds = 0.f;

	FPoseSearchQueryTrajectorySample Lerp(const FPoseSearchQueryTrajectorySample& Other, float Alpha) const;
	void SetTransform(const FTransform& Transform);
	FTransform GetTransform() const { return FTransform(Facing, Position); }
};
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FPoseSearchQueryTrajectorySample& TrajectorySample);

USTRUCT(BlueprintType, Category = "Motion Trajectory")
struct POSESEARCH_API FPoseSearchQueryTrajectory
{
	GENERATED_BODY()

	// This contains zero or more history samples, a current sample, and zero or more future predicted samples.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose Search Query Trajectory")
	TArray<FPoseSearchQueryTrajectorySample> Samples;

	FPoseSearchQueryTrajectorySample GetSampleAtTime(float Time, bool bExtrapolate = false) const;
	
#if ENABLE_ANIM_DEBUG
	void DebugDrawTrajectory(const UWorld* World, const float DebugThickness = 0.f, float HeightOffset = 0.f) const;
	void DebugDrawTrajectory(FAnimInstanceProxy& AnimInstanceProxy, const float DebugThickness = 0.f, float HeightOffset = 0.f, int MaxHistorySamples = -1, int MaxPredictionSamples = -1) const;
#endif // ENABLE_ANIM_DEBUG
};
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FPoseSearchQueryTrajectory& Trajectory);
