// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSplineInteriorSurfaceData.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Elements/PCGSplineSampler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSplineInteriorSurfaceData)

void UPCGSplineInteriorSurfaceData::Initialize(const UPCGSplineData* InSplineData)
{
	check(InSplineData);

	InitializeFromData(InSplineData);
	SplineStruct = InSplineData->SplineStruct;

	CacheData();
}

void UPCGSplineInteriorSurfaceData::PostLoad()
{
	Super::PostLoad();
	CacheData();
}

void UPCGSplineInteriorSurfaceData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use a global unique data CRC.
	AddUIDToCrc(Ar);
}

bool UPCGSplineInteriorSurfaceData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	if (!PointInsidePolygon(InTransform, InBounds))
	{
		return false;
	}

	const FVector InLocation = InTransform.GetLocation();

	// Project the sampled point onto the approximate spline surface.
	const FVector::FReal ProjectionHeight = PCGSplineSamplerHelpers::ProjectOntoSplineInteriorSurface(CachedSplinePoints, InLocation);
	const FVector::FReal SampleMinHeight = InLocation.Z + InBounds.Min.Z;
	const FVector::FReal SampleMaxHeight = InLocation.Z + InBounds.Max.Z;

	// Discard if the sample point lies above/below the spline surface.
	if (SampleMinHeight > ProjectionHeight || SampleMaxHeight < ProjectionHeight)
	{
		return false;
	}

	new(&OutPoint) FPCGPoint(InTransform, /*Density=*/1.0f, /*Seed=*/0);
	OutPoint.SetLocalBounds(InBounds);

	return true;
}

bool UPCGSplineInteriorSurfaceData::ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	if (!PointInsidePolygon(InTransform, InBounds))
	{
		return false;
	}

	FTransform ProjectedTransform = InTransform;
	const FVector InLocation = InTransform.GetLocation();
	const FVector::FReal ProjectionHeight = (InParams.bProjectPositions || InParams.bProjectRotations) ? PCGSplineSamplerHelpers::ProjectOntoSplineInteriorSurface(CachedSplinePoints, InLocation) : 0.0f;

	if (InParams.bProjectPositions)
	{
		// Project the sampled point onto the approximate spline surface.
		ProjectedTransform.SetLocation(FVector(InLocation.X, InLocation.Y, ProjectionHeight));
	}

	if (InParams.bProjectRotations)
	{
		// Project rotation by sampling the rotation of the nearest point on the spline.
		float OutDistanceSquared, OutSegment;
		const float NearestSplineKey = SplineStruct.SplineCurves.Position.FindNearest(FVector(InLocation.X, InLocation.Y, ProjectionHeight), OutDistanceSquared, OutSegment);
		ProjectedTransform.SetRotation(SplineStruct.GetQuaternionAtSplineInputKey(NearestSplineKey, ESplineCoordinateSpace::Local));
	}

	new(&OutPoint) FPCGPoint(ProjectedTransform, /*Density=*/1.0f, /*Seed=*/0);
	OutPoint.SetLocalBounds(InBounds);

	return true;
}

const UPCGPointData* UPCGSplineInteriorSurfaceData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSplineData::CreatePointData);

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);

	FPCGSplineSamplerParams SamplerParams;
	SamplerParams.Dimension = EPCGSplineSamplingDimension::OnInterior;
	SamplerParams.bProjectOntoSurface = true;

	UPCGSplineData* SplineData = NewObject<UPCGSplineData>();
	SplineData->Initialize(SplineStruct);

	// TODO: Ideally SampleInteriorData could just consume a UPCGSplineInteriorSurfaceData or FPCGSplineStruct, to avoid the extra copy on the spline struct.
	PCGSplineSamplerHelpers::SampleInteriorData(Context, SplineData, /*InBoundingShape=*/nullptr, /*InProjectionTarget=*/nullptr, /*InProjectionParams=*/{}, SamplerParams, Data);
	UE_LOG(LogPCG, Verbose, TEXT("SplineInteriorSurface generated %d points."), Data->GetPoints().Num());

	SplineData->MarkAsGarbage();

	return Data;
}

UPCGSpatialData* UPCGSplineInteriorSurfaceData::CopyInternal() const
{
	UPCGSplineInteriorSurfaceData* NewData = NewObject<UPCGSplineInteriorSurfaceData>();

	CopyBaseSurfaceData(NewData);
	NewData->SplineStruct = SplineStruct;
	NewData->CachedBounds = CachedBounds;
	NewData->CachedSplinePoints = CachedSplinePoints;
	NewData->CachedSplinePoints2D = CachedSplinePoints2D;
#if WITH_EDITOR
	NewData->bNeedsToCache = false;
#endif

	return NewData;
}

void UPCGSplineInteriorSurfaceData::CacheData()
{
#if WITH_EDITOR
	bNeedsToCache = false;
#endif

	CachedBounds = SplineStruct.GetBounds();

	// Expand bounds by the radius of points, otherwise sections of the curve that are close
	// to the bounds will report an invalid density.
	FVector SplinePointsRadius = FVector::ZeroVector;
	const FInterpCurveVector& SplineScales = SplineStruct.GetSplinePointsScale();
	for (const FInterpCurvePoint<FVector>& SplineScale : SplineScales.Points)
	{
		SplinePointsRadius = FVector::Max(SplinePointsRadius, SplineScale.OutVal.GetAbs());
	}

	CachedBounds = CachedBounds.ExpandBy(SplinePointsRadius, SplinePointsRadius);
	CachedBounds = CachedBounds.TransformBy(SplineStruct.Transform);

	FPCGSplineSamplerParams SamplerParams;
	SamplerParams.Mode = EPCGSplineSamplingMode::Subdivision;
	SamplerParams.SubdivisionsPerSegment = 5;

	UPCGPointData* PointData = NewObject<UPCGPointData>();
	UPCGSplineData* SplineData = NewObject<UPCGSplineData>();
	SplineData->Initialize(SplineStruct);

	// TODO: Ideally SampleLineData could just consume a UPCGSplineInteriorSurfaceData or FPCGSplineStruct, to avoid the extra copy on the spline struct.
	// TODO: It might be preferable to directly sample the spline than use the spline sampler, since it does a lot of work we don't care about.
	PCGSplineSamplerHelpers::SampleLineData(SplineData, /*InBoundingShape=*/nullptr, /*InProjectionTarget=*/nullptr, /*InProjectionParams=*/{}, SamplerParams, PointData);

	// Cache the points which describe the polygon of our spline.
	for (const FPCGPoint& Point : PointData->GetPoints())
	{
		CachedSplinePoints.Add(Point.Transform.GetLocation());
		CachedSplinePoints2D.Add(FVector2D(Point.Transform.GetLocation()));
	}

	SplineData->MarkAsGarbage();
	PointData->MarkAsGarbage();
}

bool UPCGSplineInteriorSurfaceData::PointInsidePolygon(const FTransform& InTransform, const FBox& InBounds) const
{
#if WITH_EDITOR
	check(!bNeedsToCache);
#endif

	const FBox TransformedBounds = InBounds.TransformBy(InTransform);

	// Test point bounds against the spline bounds.
	if (CachedBounds.ComputeSquaredDistanceToBox(TransformedBounds) > 0.0)
	{
		return false;
	}

	const FVector PointLocation = InTransform.GetLocation();

	// Maximum distance a ray needs to travel to guarantee exiting the polygon at its widest point from the sample location.
	const FVector::FReal MaxDistance = CachedBounds.Max.X - PointLocation.X + UE_KINDA_SMALL_NUMBER;

	// Test sample location against spline interior.
	// TODO: This could also consume a transform + bounds to accept rotated bounds that overlap the polygon.
	return PCGSplineSamplerHelpers::PointInsidePolygon2D(CachedSplinePoints2D, FVector2D(PointLocation), MaxDistance);
}
