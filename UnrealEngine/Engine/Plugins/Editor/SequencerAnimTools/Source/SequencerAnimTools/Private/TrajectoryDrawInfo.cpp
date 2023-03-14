// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryDrawInfo.h"
#include "TrailHierarchy.h"
#include "TrajectoryCache.h"

namespace UE
{
namespace SequencerAnimTools
{

TOptional<FVector2D> FTrailScreenSpaceTransform::ProjectPoint(const FVector& Point) const
{

	FVector2D PixelLocation;
	if (View->WorldToPixel(Point, PixelLocation))
	{
		PixelLocation /= DPIScale;
		return PixelLocation;
	}

	return TOptional<FVector2D>();
}

FVector FTrajectoryDrawInfo::GetPoint(double InTime)
{
	FTransform Transform = TrajectoryCache->GetInterp(InTime);
	return Transform.GetLocation();
}

void FTrajectoryDrawInfo::GetTrajectoryPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector>& TrajectoryPoints, TArray<double>& TrajectoryTimes)
{	
	CachedViewRange = InDisplayContext.TimeRange;

	TrajectoryTimes = TrajectoryCache->GetAllTimesInRange(InDisplayContext.TimeRange);
	TrajectoryPoints.Reserve(TrajectoryTimes.Num());
	
	for (const double Time : TrajectoryTimes)
	{
		TrajectoryPoints.Add(TrajectoryCache->Get(Time).GetTranslation());
	}

}

void FTrajectoryDrawInfo::GetTickPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector2D>& Ticks, TArray<FVector2D>& TickNormals)
{
	const double FirstTick = FMath::FloorToDouble(InDisplayContext.TimeRange.GetLowerBoundValue() / InDisplayContext.SecondsPerTick) * InDisplayContext.SecondsPerTick;
	const double Spacing = InDisplayContext.SecondsPerSegment;

	for (double TickItr = FirstTick + InDisplayContext.SecondsPerTick; TickItr < InDisplayContext.TimeRange.GetUpperBoundValue(); TickItr += InDisplayContext.SecondsPerTick)
	{
		FVector Interpolated = TrajectoryCache->GetInterp(TickItr).GetTranslation();
		TOptional<FVector2D> Projected = InDisplayContext.ScreenSpaceTransform.ProjectPoint(Interpolated);
		const double PrevSampleTime = TickItr + Spacing < InDisplayContext.TimeRange.GetUpperBoundValue() ? TickItr + Spacing : TickItr - Spacing;
		TOptional<FVector2D> PrevProjected = InDisplayContext.ScreenSpaceTransform.ProjectPoint(TrajectoryCache->GetInterp(PrevSampleTime).GetTranslation());

		if (Projected && PrevProjected)
		{
			Ticks.Add(Projected.GetValue());

			FVector2D Diff = Projected.GetValue() - PrevProjected.GetValue();
			Diff.Normalize();

			TickNormals.Add(FVector2D(-Diff.Y, Diff.X));
		}
	}
}

} // namespace SequencerAnimTools
} // namespace UE
