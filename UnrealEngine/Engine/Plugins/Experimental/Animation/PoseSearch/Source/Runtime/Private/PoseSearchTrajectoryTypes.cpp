// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchTrajectoryTypes.h"

#include "Animation/AnimTypes.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchDefines.h"

FPoseSearchQueryTrajectorySample FPoseSearchQueryTrajectorySample::Lerp(const FPoseSearchQueryTrajectorySample& Other, float Alpha) const
{
	check(Facing.IsNormalized());
	check(Other.Facing.IsNormalized());

	FPoseSearchQueryTrajectorySample Result;
	
	Result.Facing = FQuat::FastLerp(Facing, Other.Facing, Alpha).GetNormalized();
	Result.Position = FMath::Lerp(Position, Other.Position, Alpha);
	Result.AccumulatedSeconds = FMath::Lerp(AccumulatedSeconds, Other.AccumulatedSeconds, Alpha);

	return Result;
}

void FPoseSearchQueryTrajectorySample::SetTransform(const FTransform& Transform)
{
	Position = Transform.GetTranslation();
	Facing = Transform.GetRotation();
}

FPoseSearchQueryTrajectorySample FPoseSearchQueryTrajectory::GetSampleAtTime(float Time, bool bExtrapolate) const
{
	const int32 Num = Samples.Num();
	if (Num > 1)
	{
		const int32 LowerBoundIdx = Algo::LowerBound(Samples, Time, [](const FPoseSearchQueryTrajectorySample& TrajectorySample, float Value)
			{
				return Value > TrajectorySample.AccumulatedSeconds;
			});

		const int32 NextIdx = FMath::Clamp(LowerBoundIdx, 1, Samples.Num() - 1);
		const int32 PrevIdx = NextIdx - 1;

		const float Denominator = Samples[NextIdx].AccumulatedSeconds - Samples[PrevIdx].AccumulatedSeconds;
		if (!FMath::IsNearlyZero(Denominator))
		{
			const float Numerator = Time - Samples[PrevIdx].AccumulatedSeconds;
			const float LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
			return Samples[PrevIdx].Lerp(Samples[NextIdx], LerpValue);
		}

		return Samples[PrevIdx];
	}

	if (Num > 0)
	{
		return Samples[0];
	}

	return FPoseSearchQueryTrajectorySample();
}

#if ENABLE_ANIM_DEBUG
void FPoseSearchQueryTrajectory::DebugDrawTrajectory(const UWorld* World) const
{
	const int32 LastIndex = Samples.Num() - 1;
	if (LastIndex >= 0)
	{
		for (int32 Index = 0; ; ++Index)
		{
			DrawDebugSphere(World, Samples[Index].Position, 2.f /*Radius*/, 4 /*Segments*/, FColor::Black);
			DrawDebugCoordinateSystem(World, Samples[Index].Position, FRotator(Samples[Index].Facing), 12.f /*Scale*/);

			if (Index == LastIndex)
			{
				break;
			}
			
			DrawDebugLine(World, Samples[Index].Position, Samples[Index + 1].Position, FColor::Black);
		}
	}
}
#endif // ENABLE_ANIM_DEBUG

FArchive& operator<<(FArchive& Ar, FPoseSearchQueryTrajectorySample& TrajectorySample)
{
	Ar << TrajectorySample.Facing;
	Ar << TrajectorySample.Position;
	Ar << TrajectorySample.AccumulatedSeconds;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FPoseSearchQueryTrajectory& Trajectory)
{
	Ar << Trajectory.Samples;
	return Ar;
}