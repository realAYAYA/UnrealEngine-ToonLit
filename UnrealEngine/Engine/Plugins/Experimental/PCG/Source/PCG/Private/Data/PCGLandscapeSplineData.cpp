// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGLandscapeSplineData.h"

#include "PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGSplineSampler.h"

#include "Landscape.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplineControlPoint.h"

namespace PCGLandscapeDataHelpers
{
	// This function assumes that the A-B segment has a "1" density, while the C-D segment has a "0" density
	FVector::FReal GetDensityInQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& P)
	{
		// Since the landscape has a strict Z behavior and our points might not be directly on the plane, we should only consider
		// the 2D plane. When we support other axes, we could just remove the normal component off the position to correct.
		const FVector::FReal Tolerance = UE_SMALL_NUMBER;
		FVector BaryABC = FMath::GetBaryCentric2D(P, A, B, C);

		if (BaryABC.X >= -Tolerance && BaryABC.Y >= -Tolerance && BaryABC.Z >= -Tolerance)
		{
			return 1.0f - FMath::Max(BaryABC.Z, 0);
		}

		FVector BaryACD = FMath::GetBaryCentric2D(P, A, C, D);

		if (BaryACD.X >= -Tolerance && BaryACD.Y >= -Tolerance && BaryACD.Z >= -Tolerance)
		{
			return FMath::Max(BaryACD.X, 0);
		}

		return FVector::FReal(-1);
	}
}

void UPCGLandscapeSplineData::Initialize(ULandscapeSplinesComponent* InSplineComponent)
{
	check(InSplineComponent);
	Spline = InSplineComponent;
	TargetActor = InSplineComponent->GetOwner();
}

int UPCGLandscapeSplineData::GetNumSegments() const
{
	check(Spline.IsValid());
	return Spline->GetSegments().Num();
}

FVector::FReal UPCGLandscapeSplineData::GetSegmentLength(int SegmentIndex) const
{
	check(Spline.IsValid());
	check(SegmentIndex >= 0 && SegmentIndex < Spline->GetSegments().Num());
	
	const ULandscapeSplineSegment* Segment = Spline->GetSegments()[SegmentIndex];
	const TArray<FLandscapeSplineInterpPoint>& InterpPoints = Segment->GetPoints();
	FVector::FReal Length = 0;

	for (int PointIndex = 1; PointIndex < InterpPoints.Num(); ++PointIndex)
	{
		Length += (InterpPoints[PointIndex].Center - InterpPoints[PointIndex - 1].Center).Length();
	}

	return Length;
}

FTransform UPCGLandscapeSplineData::GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, FBox* OutBounds) const
{
	check(Spline.IsValid());
	check(SegmentIndex >= 0 && SegmentIndex < Spline->GetSegments().Num());

	const ULandscapeSplineSegment* Segment = Spline->GetSegments()[SegmentIndex];
	check(Segment);

	const FLandscapeSplineSegmentConnection& Start = Segment->Connections[0];
	const FLandscapeSplineSegmentConnection& End = Segment->Connections[1];

	const TArray<FLandscapeSplineInterpPoint>& InterpPoints = Segment->GetPoints();
	FVector::FReal Length = FMath::Max(0, Distance);

	for (int PointIndex = 1; PointIndex < InterpPoints.Num(); ++PointIndex)
	{
		const FLandscapeSplineInterpPoint& PreviousPoint = InterpPoints[PointIndex - 1];
		const FLandscapeSplineInterpPoint& CurrentPoint = InterpPoints[PointIndex];

		const FVector::FReal SegmentLength = (CurrentPoint.Center - PreviousPoint.Center).Length();
		if (SegmentLength > Length || PointIndex == InterpPoints.Num() - 1)
		{
			const FVector XAxis = CurrentPoint.Center - PreviousPoint.Center;
			const FVector PreviousYAxis = PreviousPoint.Right - PreviousPoint.Center;
			const FVector CurrentYAxis = CurrentPoint.Right - CurrentPoint.Center;
			const FVector PreviousZAxis = (XAxis ^ PreviousYAxis).GetSafeNormal(UE_SMALL_NUMBER, FVector::ZAxisVector);
			const FVector CurrentZAxis = (XAxis ^ CurrentYAxis).GetSafeNormal(UE_SMALL_NUMBER, FVector::ZAxisVector);

			FTransform PreviousTransform = FTransform(XAxis, PreviousYAxis, PreviousZAxis, PreviousPoint.Center);
			FTransform CurrentTransform = FTransform(XAxis, CurrentYAxis, CurrentZAxis, CurrentPoint.Center);

			const FVector::FReal BlendRatio = ((SegmentLength > Length) ? (Length / SegmentLength) : 1.0);
			PreviousTransform.BlendWith(CurrentTransform, BlendRatio);

			if (OutBounds)
			{
				// Important note: the box here is going to be useful to be able to specify the relative sizes of the falloffs
				*OutBounds = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector);
				OutBounds->Min.Y *= (CurrentPoint.FalloffLeft - CurrentPoint.Center).Length() / (CurrentPoint.Left - CurrentPoint.Center).Length();
				OutBounds->Max.Y *= (CurrentPoint.FalloffRight - CurrentPoint.Center).Length() / (CurrentPoint.Right - CurrentPoint.Center).Length();
			}

			return PreviousTransform * Spline->GetComponentTransform();
		}
		else
		{
			Length -= SegmentLength;
		}
	}
	
	check(0);
	return FTransform();
}

const UPCGPointData* UPCGLandscapeSplineData::CreatePointData(FPCGContext* Context) const
{
	check(Spline.IsValid());
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGLandscapeSplineData::CreatePointData);

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);
	TArray<FPCGPoint>& Points = Data->GetMutablePoints();

	FPCGSplineSamplerParams SamplerParams;
	SamplerParams.Mode = EPCGSplineSamplingMode::Distance;
	SamplerParams.Dimension = EPCGSplineSamplingDimension::OnHorizontal;

	PCGSplineSampler::SampleLineData(this, this, SamplerParams, Data);

	UE_LOG(LogPCG, Verbose, TEXT("Landscape spline %s generated %d points"), *Spline->GetFName().ToString(), Points.Num());

	return Data;
}

FBox UPCGLandscapeSplineData::GetBounds() const
{
	check(Spline.IsValid());

	FBox Bounds(EForceInit::ForceInit);
	for (const TObjectPtr<ULandscapeSplineSegment>& Segment : Spline->GetSegments())
	{
		Bounds += Segment->GetBounds();
	}

	if (Bounds.IsValid)
	{
		Bounds = Bounds.TransformBy(Spline->GetComponentToWorld());
	}

	return Bounds;
}

bool UPCGLandscapeSplineData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// TODO : add metadata support on poly lines
	// TODO : add support for bounds
	const ULandscapeSplinesComponent* SplinePtr = Spline.Get();
	check(SplinePtr);

	OutPoint.Transform = InTransform;
	OutPoint.SetLocalBounds(InBounds); // TODO: should maybe do Min.Z = Max.Z = 0 ?

	const FVector Position = SplinePtr->GetComponentTransform().InverseTransformPosition(OutPoint.Transform.GetLocation());

	float PointDensity = 0.0f;

	for(const TObjectPtr<ULandscapeSplineSegment>& Segment : SplinePtr->GetSegments())
	{
		// Considering the landscape spline always exists on the landscape,
		// we'll ignore the Z component of the input here for the bounds check.
		if(!PCGHelpers::IsInsideBoundsXY(Segment->GetBounds(), Position))
		{
			continue;
		}

		const TArray<FLandscapeSplineInterpPoint>& InterpPoints = Segment->GetPoints();

		float SegmentDensity = 0.0f;

		for (int PointIndex = 1; PointIndex < InterpPoints.Num(); ++PointIndex)
		{
			const FLandscapeSplineInterpPoint& Start = InterpPoints[PointIndex - 1];
			const FLandscapeSplineInterpPoint& End = InterpPoints[PointIndex];

			float Density = 0.0f;

			// Note: these checks have no prior information on the structure of the data, except that they form quads.
			// Considering that the points on a given control point are probably aligned, we could do an early check
			// in the original quad (start left falloff -> start right falloff -> end right falloff -> end left falloff)
			// TODO: this sequence here can be optimized knowing that some checks will be redundant.
			// Important note: the order and selection of points is important to the density computation. This assumes the first 2 points are the "1" density edge
			Density = FMath::Max(Density, PCGLandscapeDataHelpers::GetDensityInQuad(Start.Center, End.Center, End.Left, Start.Left, Position) >= 0 ? 1.0f : 0.0f);
			Density = FMath::Max(Density, PCGLandscapeDataHelpers::GetDensityInQuad(Start.Left, End.Left, End.FalloffLeft, Start.FalloffLeft, Position));
			Density = FMath::Max(Density, PCGLandscapeDataHelpers::GetDensityInQuad(End.Center, Start.Center, Start.Right, End.Right, Position) >= 0 ? 1.0f : 0.0f);
			Density = FMath::Max(Density, PCGLandscapeDataHelpers::GetDensityInQuad(End.Right, Start.Right, Start.FalloffRight, End.FalloffRight, Position));

			if (Density > SegmentDensity)
			{
				SegmentDensity = Density;
			}
		}

		if (SegmentDensity > PointDensity)
		{
			PointDensity = SegmentDensity;
		}
	}

	OutPoint.Density = PointDensity;
	return OutPoint.Density > 0;
}