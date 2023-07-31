// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSplineData.h"

#include "PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSurfaceData.h"
#include "Elements/PCGSplineSampler.h"

#include "Components/SplineComponent.h"

void UPCGSplineData::Initialize(USplineComponent* InSpline)
{
	check(InSpline);
	Spline = InSpline;
	TargetActor = InSpline->GetOwner();

	CachedBounds = PCGHelpers::GetActorBounds(Spline->GetOwner());

	// Expand bounds by the radius of points, otherwise sections of the curve that are close
	// to the bounds will report an invalid density.
	FVector SplinePointsRadius = FVector::ZeroVector;
	const FInterpCurveVector& SplineScales = Spline->GetSplinePointsScale();
	for (const FInterpCurvePoint<FVector>& SplineScale : SplineScales.Points)
	{
		SplinePointsRadius = FVector::Max(SplinePointsRadius, SplineScale.OutVal.GetAbs());
	}

	CachedBounds = CachedBounds.ExpandBy(SplinePointsRadius, SplinePointsRadius);
}

int UPCGSplineData::GetNumSegments() const
{
	return Spline ? Spline->GetNumberOfSplineSegments() : 0;
}

FVector::FReal UPCGSplineData::GetSegmentLength(int SegmentIndex) const
{
	return Spline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex + 1) - Spline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex);
}

FVector UPCGSplineData::GetLocationAtDistance(int SegmentIndex, FVector::FReal Distance) const
{
	return Spline->GetLocationAtDistanceAlongSpline(Spline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex) + Distance, ESplineCoordinateSpace::World);
}

FTransform UPCGSplineData::GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, FBox* OutBounds) const
{
	if (OutBounds)
	{
		*OutBounds = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector);
	}

	return Spline->GetTransformAtDistanceAlongSpline(Spline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex) + Distance, ESplineCoordinateSpace::World, /*bUseScale=*/true);
}

const UPCGPointData* UPCGSplineData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSplineData::CreatePointData);
	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);

	FPCGSplineSamplerParams SamplerParams;
	SamplerParams.Mode = EPCGSplineSamplingMode::Distance;

	PCGSplineSampler::SampleLineData(this, this, SamplerParams, Data);

	UE_LOG(LogPCG, Verbose, TEXT("Spline %s generated %d points"), *Spline->GetFName().ToString(), Data->GetPoints().Num());

	return Data;
}

FBox UPCGSplineData::GetBounds() const
{
	return CachedBounds;
}

bool UPCGSplineData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// TODO: support metadata
	// TODO: support proper bounds

	// Find nearest point on spline
	const FVector InPosition = InTransform.GetLocation();
	float NearestPointKey = Spline->FindInputKeyClosestToWorldLocation(InPosition);
	FTransform NearestTransform = Spline->GetTransformAtSplineInputKey(NearestPointKey, ESplineCoordinateSpace::World, true);
	FVector LocalPoint = NearestTransform.InverseTransformPosition(InPosition);
	
	// Linear fall off based on the distance to the nearest point
	// TODO: should be based on explicit settings
	float Distance = LocalPoint.Length();
	if (Distance > 1.0f)
	{
		return false;
	}
	else
	{
		OutPoint.Transform = NearestTransform;
		OutPoint.Transform.SetLocation(InPosition);
		OutPoint.SetLocalBounds(InBounds);
		OutPoint.Density = 1.0f - Distance;

		return true;
	}
}

UPCGProjectionData* UPCGSplineData::ProjectOn(const UPCGSpatialData* InOther) const
{
	if(InOther->GetDimension() == 2)
	{
		UPCGSplineProjectionData* SplineProjectionData = NewObject<UPCGSplineProjectionData>(const_cast<UPCGSplineData*>(this));
		SplineProjectionData->Initialize(this, InOther);
		return SplineProjectionData;
	}
	else
	{
		return Super::ProjectOn(InOther);
	}
}

FVector2D UPCGSplineProjectionData::Project(const FVector& InVector) const
{
	const FVector& SurfaceNormal = GetSurface()->GetNormal();
	FVector Projection = InVector - InVector.ProjectOnToNormal(SurfaceNormal);

	// Ignore smallest absolute coordinate value.
	// Normally, one should be zero, but because of numerical precision,
	// We'll make sure we're doing it right
	FVector::FReal SmallestCoordinate = TNumericLimits<FVector::FReal>::Max();
	int SmallestCoordinateAxis = -1;

	for (int Axis = 0; Axis < 3; ++Axis)
	{
		FVector::FReal AbsoluteCoordinateValue = FMath::Abs(Projection[Axis]);
		if (AbsoluteCoordinateValue < SmallestCoordinate)
		{
			SmallestCoordinate = AbsoluteCoordinateValue;
			SmallestCoordinateAxis = Axis;
		}
	}

	FVector2D Projection2D;
	int AxisIndex = 0;
	for (int Axis = 0; Axis < 3; ++Axis)
	{
		if (Axis != SmallestCoordinateAxis)
		{
			Projection2D[AxisIndex++] = Projection[Axis];
		}
	}

	return Projection2D;
}

void UPCGSplineProjectionData::Initialize(const UPCGSplineData* InSourceSpline, const UPCGSpatialData* InTargetSurface)
{
	Super::Initialize(InSourceSpline, InTargetSurface);

	const USplineComponent* Spline = GetSpline()->Spline.Get();
	const FVector& SurfaceNormal = GetSurface()->GetNormal();

	if (Spline)
	{
		const FInterpCurveVector& SplinePosition = Spline->GetSplinePointsPosition();

		// Build projected spline data
		ProjectedPosition.bIsLooped = SplinePosition.bIsLooped;
		ProjectedPosition.LoopKeyOffset = SplinePosition.LoopKeyOffset;

		ProjectedPosition.Points.Reserve(SplinePosition.Points.Num());

		for (const FInterpCurvePoint<FVector>& SplinePoint : SplinePosition.Points)
		{
			FInterpCurvePoint<FVector2D>& ProjectedPoint = ProjectedPosition.Points.Emplace_GetRef();

			ProjectedPoint.InVal = SplinePoint.InVal;
			ProjectedPoint.OutVal = Project(SplinePoint.OutVal);
			// TODO: correct tangent if it becomes null
			ProjectedPoint.ArriveTangent = Project(SplinePoint.ArriveTangent).GetSafeNormal();
			ProjectedPoint.LeaveTangent = Project(SplinePoint.LeaveTangent).GetSafeNormal();
			ProjectedPoint.InterpMode = SplinePoint.InterpMode;
		}
	}
}

bool UPCGSplineProjectionData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// TODO: support metadata
	// TODO: support bounds

	// Find nearest point on projected spline
	const FVector InPosition = InTransform.GetLocation();
	const USplineComponent* Spline = GetSpline()->Spline.Get();
	const FVector& SurfaceNormal = GetSurface()->GetNormal();

	// Project to 2D space
	const FTransform LocalTransform = InTransform * Spline->GetComponentTransform().Inverse();
	FVector2D LocalPosition2D = Project(LocalTransform.GetLocation());
	float Dummy;
	// Find nearest key on 2D spline
	float NearestInputKey = ProjectedPosition.InaccurateFindNearest(LocalPosition2D, Dummy);
	// TODO: if we didn't want to hand off density computation to the spline and do it here instead, we could do it in 2D space.
	// Find point on original spline using the previously found key
	// Note: this is an approximation that might not hold true since we are changing the curve length
	const FVector NearestPointOnSpline = Spline->GetLocationAtSplineInputKey(NearestInputKey, ESplineCoordinateSpace::World);
	const FVector PointOnLine = FMath::ClosestPointOnInfiniteLine(InPosition, InPosition + SurfaceNormal, NearestPointOnSpline);

	// TODO: this is super inefficient
	FPCGPoint SplinePoint;
	if (GetSpline()->SamplePoint(FTransform(PointOnLine), InBounds, SplinePoint, OutMetadata))
	{
		FPCGPoint SurfacePoint;
		if (GetSurface()->SamplePoint(SplinePoint.Transform, InBounds, SurfacePoint, OutMetadata))
		{
			OutPoint = SplinePoint;
			OutPoint.Transform = SurfacePoint.Transform;
			OutPoint.Density *= SurfacePoint.Density;
			OutPoint.Color *= SurfacePoint.Color;

			if (OutMetadata)
			{
				if (SplinePoint.MetadataEntry != PCGInvalidEntryKey && SurfacePoint.MetadataEntry)
				{
					//TODO review op
					OutMetadata->MergePointAttributesSubset(SplinePoint, OutMetadata, GetSpline()->Metadata, SurfacePoint, OutMetadata, GetSurface()->Metadata, OutPoint, EPCGMetadataOp::Max);
				}
				else if (SurfacePoint.MetadataEntry != PCGInvalidEntryKey)
				{
					OutPoint.MetadataEntry = SurfacePoint.MetadataEntry;
				}
			}

			return true;
		}
	}

	return false;
}

const UPCGSplineData* UPCGSplineProjectionData::GetSpline() const
{
	return Cast<const UPCGSplineData>(Source);
}

const UPCGSpatialData* UPCGSplineProjectionData::GetSurface() const
{
	return Target;
}