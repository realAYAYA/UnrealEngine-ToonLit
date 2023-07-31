// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryCache.h"

namespace UE
{
namespace SequencerAnimTools
{

bool RangesContain(const TSet<TRange<double>>& Ranges, const double Time)
{
	for (const TRange<double>& Range : Ranges)
	{
		if (Range.Contains(Time))
		{
			return true;
		}
	}
	return false;
}

void FTrajectoryCache::UpdateCacheTimes(const FTrailEvaluateTimes& EvalTimes)
{
	TrackRange = EvalTimes.Range;
	Spacing = EvalTimes.Spacing;
}

FTransform FArrayTrajectoryCache::GetInterp(const double InTime) const
{
	if (TrajectoryCache.Num() == 0)
	{
		return Default;
	}

	const double T = (InTime/Spacing) - FMath::FloorToDouble(InTime / Spacing);
	const int32 LowIdx = FMath::Clamp(int32((InTime - TrackRange.GetLowerBoundValue()) / Spacing), 0, TrajectoryCache.Num() - 1);
	const int32 HighIdx = FMath::Clamp(LowIdx + 1, 0, TrajectoryCache.Num() - 1);

	if (LowIdx == HighIdx)
	{
		return TrajectoryCache[LowIdx];
	}

	FTransform KeyAtom1 = TrajectoryCache[LowIdx];
	FTransform KeyAtom2 = TrajectoryCache[HighIdx];
	
	KeyAtom1.NormalizeRotation();
	KeyAtom2.NormalizeRotation();

	FTransform TempBlended; 
	TempBlended.Blend(KeyAtom1, KeyAtom2, T);
	return TempBlended;
}

void FArrayTrajectoryCache::GetInterp(const double InTime,FTransform& Transform, FTransform& ParentTransform) const
{
	if (TrajectoryCache.Num() == 0)
	{
		Transform =ParentTransform = Default;
	}

	const double T = (InTime / Spacing) - FMath::FloorToDouble(InTime / Spacing);
	const int32 LowIdx = FMath::Clamp(int32((InTime - TrackRange.GetLowerBoundValue()) / Spacing), 0, TrajectoryCache.Num() - 1);
	const int32 HighIdx = FMath::Clamp(LowIdx + 1, 0, TrajectoryCache.Num() - 1);

	if (LowIdx == HighIdx)
	{
		Transform = TrajectoryCache[LowIdx];
		if (ParentTrajectoryCache.Num() > 0)
		{
			ParentTransform = ParentTrajectoryCache[LowIdx];
		}
		else
		{
			ParentTransform = FTransform::Identity;
		}
	}

	FTransform KeyAtom1 = TrajectoryCache[LowIdx];
	FTransform KeyAtom2 = TrajectoryCache[HighIdx];
	
	KeyAtom1.NormalizeRotation();
	KeyAtom2.NormalizeRotation();

	Transform.Blend(KeyAtom1, KeyAtom2, T);
	if (ParentTrajectoryCache.Num() > 0)
	{
		KeyAtom1 = ParentTrajectoryCache[LowIdx];
		KeyAtom2 = ParentTrajectoryCache[HighIdx];

		KeyAtom1.NormalizeRotation();
		KeyAtom2.NormalizeRotation();
		
		ParentTransform.Blend(KeyAtom1, KeyAtom2, T);
	}
	else
	{
		ParentTransform = FTransform::Identity;
	}

}

TArray<double> FArrayTrajectoryCache::GetAllTimesInRange(const TRange<double>& InRange) const
{
	TRange<double> GenRange = TRange<double>::Intersection({ TrackRange, InRange });

	TArray<double> AllTimesInRange;
	AllTimesInRange.Reserve(int(GenRange.Size<double>() / Spacing) + 1);
	const double FirstTick = FMath::FloorToDouble((GenRange.GetLowerBoundValue() / Spacing) + KINDA_SMALL_NUMBER) * Spacing;
	for (double TickItr = FirstTick + KINDA_SMALL_NUMBER; TickItr < GenRange.GetUpperBoundValue(); TickItr += Spacing)
	{
		AllTimesInRange.Add(TickItr);
	}

	AllTimesInRange.Add(GenRange.GetUpperBoundValue());

	return AllTimesInRange;
}

} // namespace SequencerAnimTools
} // namespace UE
