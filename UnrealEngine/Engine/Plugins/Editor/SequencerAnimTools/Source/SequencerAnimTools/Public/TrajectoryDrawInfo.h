// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "Math/Color.h"
#include "Math/Range.h"
#include "Math/Vector.h"
#include "Misc/Optional.h"

#include "SceneView.h"
#include "UnrealClient.h"

namespace UE
{
namespace SequencerAnimTools
{

class FTrail;  
class FMapTrajectoryCache;
class FTrajectoryCache;

class FTrailScreenSpaceTransform
{
public:
	FTrailScreenSpaceTransform(const FSceneView* InView,  const float InDPIScale = 1.0f)
		: View(InView)
		, DPIScale(InDPIScale)
	{}

	TOptional<FVector2D> ProjectPoint(const FVector& Point) const;

private:
	const FSceneView* View;
	const float DPIScale;
};

struct FDisplayContext
{
	FGuid YourNode;
	const FTrailScreenSpaceTransform& ScreenSpaceTransform;
	double SecondsPerTick;
	const TRange<double>& TimeRange;
	double SecondsPerSegment;
};


class FTrajectoryDrawInfo
{
public:

	FTrajectoryDrawInfo(const FLinearColor& InColor, FTrajectoryCache* InTrajectoryCache)
		: Color(InColor)
		, CachedViewRange(TRange<double>::Empty())
		, TrajectoryCache(InTrajectoryCache)
	{}

	virtual ~FTrajectoryDrawInfo() {}

	void GetTrajectoryPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector>& OutPoints, TArray<double>& OutSeconds);
	void GetTickPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector2D>& OutTicks, TArray<FVector2D>& OutTickTangents);
	FVector GetPoint(double InTime);

	// Optionally implemented methods
	void SetColor(const FLinearColor& InColor) { Color = InColor; }
	FLinearColor GetColor() const { return Color; }

	virtual const TRange<double>& GetCachedViewRange() const { return CachedViewRange; }
protected:
	FLinearColor Color;
	TRange<double> CachedViewRange;
	FTrajectoryCache* TrajectoryCache;
};


} // namespace MovieScene
} // namespace UE
