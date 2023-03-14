// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Misc/Optional.h"
#include "Math/Range.h"
#include "Math/Transform.h"

namespace UE
{
namespace SequencerAnimTools
{

bool RangesContain(const TSet<TRange<double>>& Ranges, const double Time);


// Intermediate range reprsentation structure
class FTrailEvaluateTimes
{
public:
	FTrailEvaluateTimes()
		: EvalTimes()
		, Spacing(0.1)
		, Range(TRange<double>::Empty())
	{}

	FTrailEvaluateTimes(TArrayView<double> InEvalTimes, double InSpacing) 
		: EvalTimes(InEvalTimes)
		, Spacing(InSpacing)
		, Range(TRange<double>(InEvalTimes[0], InEvalTimes.Last()))
	{}

	TArrayView<double> EvalTimes;
	double Spacing;
	TRange<double> Range;
};

class FTrajectoryCache
{
public:
	FTrajectoryCache()
		: Default(FTransform::Identity)
		, TrackRange()
		, Spacing()
	{}

	FTrajectoryCache(const double InSpacing, const TRange<double>& InTrackRange, const FTransform& InDefault)
		: Default(InDefault)
		, TrackRange(TRange<double>(FMath::FloorToDouble(InTrackRange.GetLowerBoundValue() / InSpacing)* InSpacing, FMath::FloorToDouble(InTrackRange.GetUpperBoundValue() / InSpacing)* InSpacing)) // snap to spacing
		, Spacing(InSpacing)

	{}

	virtual ~FTrajectoryCache() {}

	// alternatively, GetEvaluateTimes(const FTrailEvaluateTimes&) and GetInterpEvaluateTimes(const FTrailEvaluateTimes&)
	virtual const FTransform& Get(const double InTime) const = 0;
	virtual FTransform GetInterp(const double InTime) const = 0;
	virtual void GetInterp(const double InTime, FTransform& OutTransform, FTransform& OutParentTransform) const{};
	virtual const FTransform& GetParent(const double InTime) const { return FTransform::Identity; }

	virtual TArray<double> GetAllTimesInRange(const TRange<double>& InRange) const = 0;
	
	virtual void Set(const double InTime, const FTransform& InValue) = 0;
	virtual void SetTransforms(TArray<FTransform>& InTrajectoryCache, TArray<FTransform>& InParentCache) = 0;
	// Optionally implemented
	virtual const FTransform& GetDefault() const { return Default; };
	virtual void UpdateCacheTimes(const FTrailEvaluateTimes& InEvalTimes);
	
	const TRange<double>& GetTrackRange() const { return TrackRange; }

protected:
	FTransform Default;
	TRange<double> TrackRange;
	double Spacing;
};

class FArrayTrajectoryCache : public FTrajectoryCache
{
public:

	// for FRootTrail
	FArrayTrajectoryCache()
		: FTrajectoryCache()
		, TrajectoryCache()

	{}

	FArrayTrajectoryCache(const double InSpacing, const TRange<double>& InTrackRange, FTransform InDefault = FTransform::Identity)
		: FTrajectoryCache(InSpacing,InTrackRange,InDefault)
		, TrajectoryCache()

	{
		TrajectoryCache.SetNumUninitialized(int32(InTrackRange.Size<double>() / InSpacing) + 1);

		for (FTransform& Transform : TrajectoryCache)
		{
			Transform = InDefault;
		}
	}

	virtual const FTransform& Get(const double InTime) const override 
	{ 
		return TrajectoryCache.Num() > 0 ? TrajectoryCache[FMath::Clamp(int32((InTime - TrackRange.GetLowerBoundValue()) / Spacing), 0, TrajectoryCache.Num() - 1)] : Default; 
	}

	virtual FTransform GetInterp(const double InTime) const override;
	virtual void GetInterp(const double InTime, FTransform& OutTransform, FTransform& OutParentTransform) const override;

	virtual const FTransform& GetParent(const double InTime) const override
	{
		return ParentTrajectoryCache.Num() > 0 ? ParentTrajectoryCache[FMath::Clamp(int32((InTime - TrackRange.GetLowerBoundValue()) / Spacing), 0, ParentTrajectoryCache.Num() - 1)] : FTransform::Identity;
	}


	virtual TArray<double> GetAllTimesInRange(const TRange<double>& InRange) const override;

	virtual void Set(const double InTime, const FTransform& InValue) override 
	{ 
		if (TrajectoryCache.Num() > 0)
		{
			TrajectoryCache[FMath::Clamp(int32((InTime - TrackRange.GetLowerBoundValue()) / Spacing), 0, TrajectoryCache.Num() - 1)] = InValue;
		}
	}

	virtual void SetTransforms(TArray<FTransform>& InTrajectoryCache, TArray<FTransform>& InParentCache)
	{
		TrajectoryCache = InTrajectoryCache;
		ParentTrajectoryCache = InParentCache;
	}

	
private:
	TArray<FTransform> TrajectoryCache;
	TArray<FTransform> ParentTrajectoryCache;

};

} // namespace MovieScene
} // namespace UE
