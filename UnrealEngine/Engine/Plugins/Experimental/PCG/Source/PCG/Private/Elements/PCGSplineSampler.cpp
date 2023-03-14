// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSplineSampler.h"

#include "PCGCommon.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGLandscapeSplineData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Components/SplineComponent.h"
#include "Voronoi/Voronoi.h"

namespace PCGSplineSamplerHelpers
{
	/**
	* Robust 2D line segment intersection. Returns true if the segments intersect and stores the intersection in OutIntersectionPoint.
	* If the segments are collinear, one of the contained endpoints will be chosen as the intersection.
	*/
	bool SegmentIntersection2D(const FVector2D& SegmentStartA, const FVector2D& SegmentEndA, const FVector2D& SegmentStartB, const FVector2D& SegmentEndB, FVector2D& OutIntersectionPoint)
	{
		const FVector2D VectorA = SegmentEndA - SegmentStartA;
		const FVector2D VectorB = SegmentEndB - SegmentStartB;

		const FVector::FReal Determinant = VectorA.X * VectorB.Y - VectorB.X * VectorA.Y;

		// Determinant is zero means the segments are parallel. Check if they are also collinear by projecting (0, 0) onto both lines.
		if (FMath::IsNearlyZero(Determinant))
		{
			const FVector::FReal SquareMagnitudeA = VectorA.SquaredLength();
			if (FMath::IsNearlyZero(SquareMagnitudeA))
			{
				const FVector2D& MinB = SegmentStartB.X < SegmentEndB.X ? SegmentStartB : SegmentEndB;
				const FVector2D& MaxB = SegmentStartB.X > SegmentEndB.X ? SegmentStartB : SegmentEndB;

				if (SegmentStartA.X > MinB.X && SegmentStartA.X < MaxB.X)
				{
					OutIntersectionPoint = SegmentStartA;
					return true;
				}

				return false;
			}

			const FVector::FReal SquareMagnitudeB = VectorB.SquaredLength();
			if (FMath::IsNearlyZero(SquareMagnitudeB))
			{
				const FVector2D& MinA = SegmentStartA.X < SegmentEndA.X ? SegmentStartA : SegmentEndA;
				const FVector2D& MaxA = SegmentStartA.X > SegmentEndA.X ? SegmentStartA : SegmentEndA;

				if (SegmentStartB.X > MinA.X && SegmentStartB.X < MaxA.X)
				{
					OutIntersectionPoint = SegmentStartB;
					return true;
				}

				return false;
			}

			// Taking the ray from SegmentStart to (0, 0) for both segments, and projecting the ray onto its
			// respective line segment should yield two coincident points if the segments are collinear
			const FVector2D AToZero = -SegmentStartA; // (0, 0) - SegmentStartA
			const FVector2D ProjectionOnA = VectorA * VectorA.Dot(AToZero) / SquareMagnitudeA;
			const FVector2D ProjectedPointOnA = SegmentStartA + ProjectionOnA;

			const FVector2D BToZero = -SegmentStartB; // (0, 0) - SegmentStartB
			const FVector2D ProjectionOnB = VectorB * VectorB.Dot(BToZero) / SquareMagnitudeB;
			const FVector2D ProjectedPointOnB = SegmentStartB + ProjectionOnB;
			
			// If the projected points are not equal, then we are not collinear
			if (!ProjectedPointOnA.Equals(ProjectedPointOnB))
			{
				return false;
			}
			
			const FVector2D& MinA = SegmentStartA.X < SegmentEndA.X ? SegmentStartA : SegmentEndA;
			const FVector2D& MaxA = SegmentStartA.X > SegmentEndA.X ? SegmentStartA : SegmentEndA;
			const FVector2D& MinB = SegmentStartB.X < SegmentEndB.X ? SegmentStartB : SegmentEndB;
			const FVector2D& MaxB = SegmentStartB.X > SegmentEndB.X ? SegmentStartB : SegmentEndB;
			
			if (MinA.X > MinB.X && MinA.X < MaxB.X)
			{
				OutIntersectionPoint = MinA;
				return true;
			}
			else if (MaxA.X > MinB.X && MaxA.X < MaxB.X)
			{
				OutIntersectionPoint = MaxA;
				return true;
			}
			if (MinB.X > MinA.X && MinB.X < MaxA.X)
			{
				OutIntersectionPoint = MinB;
				return true;
			}
			else if (MaxB.X > MinA.X && MaxB.X < MaxA.X)
			{
				OutIntersectionPoint = MaxB;
				return true;
			}
			
			return false;
		}

		const FVector::FReal DeltaX = SegmentStartA.X - SegmentStartB.X;
		const FVector::FReal DeltaY = SegmentStartA.Y - SegmentStartB.Y;

		const FVector::FReal S = (VectorA.X * DeltaY - VectorA.Y * DeltaX) / Determinant;
		const FVector::FReal T = (VectorB.X * DeltaY - VectorB.Y * DeltaX) / Determinant;

		if (S >= 0 && S <= 1 && T >= 0 && T <= 1)
		{
			OutIntersectionPoint = SegmentStartA + T * VectorA;
			return true;
		}

		return false;
	}

	/** Intersects a line segment with all line segments of a polygon. May return duplicates if the segment intersects a vertex of the polygon. */
	void SegmentPolygonIntersection2D(const FVector2D& SegmentStart, const FVector2D& SegmentEnd, const TArray<FVector2D>& PolygonPoints, TArray<FVector2D>& OutIntersectionPoints)
	{
		const int32 PointCount = PolygonPoints.Num();

		if (PointCount < 3)
		{
			return;
		}

		// Checks if a duplicate point should be counted as a valid intersection and adds it to the intersection list if so. Collinear points are also considered duplicates.
		auto TestDuplicatePoint = [&](const FVector2D& IntersectionPoint, int32 Index) -> bool
		{
			const int32 BackwardIndex = Index - 1;
			const int32 ForwardIndex = Index + 1;

			// Find DeltaY for the nearest backward point
			FVector::FReal DeltaY1 = 0.f;
			for (int32 Offset = 0; Offset < PointCount; ++Offset)
			{
				DeltaY1 = PolygonPoints[(BackwardIndex - Offset + PointCount) % PointCount].Y - IntersectionPoint.Y;

				// Choose the first non-zero delta
				if (!FMath::IsNearlyZero(DeltaY1))
				{
					break;
				}
			}

			// Find DeltaY for the nearest forward point
			FVector::FReal DeltaY2 = 0.f;
			for (int32 Offset = 0; Offset < PointCount; ++Offset)
			{
				DeltaY2 = PolygonPoints[(ForwardIndex + Offset) % PointCount].Y - IntersectionPoint.Y;

				// Choose the first non-zero delta
				if (!FMath::IsNearlyZero(DeltaY2))
				{
					break;
				}
			}

			// Allow double counting when adjacent points lie on opposite sides of our ray
			return (DeltaY1 > 0 && DeltaY2 > 0) || (DeltaY1 < 0 && DeltaY2 < 0);
		};

		auto AreSegmentsCollinear = [](const FVector2D& SegmentStartA, const FVector2D& SegmentEndA, const FVector2D& SegmentStartB, const FVector2D& SegmentEndB) -> bool
		{
			const FVector2D VectorA = SegmentEndA - SegmentStartA;
			const FVector2D VectorB = SegmentEndB - SegmentStartB;

			const FVector::FReal SquareMagnitudeA = VectorA.SquaredLength();
			const FVector::FReal SquareMagnitudeB = VectorB.SquaredLength();

			if (!FMath::IsNearlyZero(FVector2D::CrossProduct(VectorA, VectorB)) || FMath::IsNearlyZero(SquareMagnitudeA) || FMath::IsNearlyZero(SquareMagnitudeB))
			{
				return false;
			}

			// Taking the ray from SegmentStart to (0, 0) for both segments, and projecting the ray onto its
			// respective line segment should yield two coincident points if the segments are collinear
			const FVector2D AToZero = -SegmentStartA; // (0, 0) - SegmentStartA
			const FVector2D ProjectionOnA = VectorA * VectorA.Dot(AToZero) / SquareMagnitudeA;
			const FVector2D ProjectedPointOnA = SegmentStartA + ProjectionOnA;

			const FVector2D BToZero = -SegmentStartB; // (0, 0) - SegmentStartB
			const FVector2D ProjectionOnB = VectorB * VectorB.Dot(BToZero) / SquareMagnitudeB;
			const FVector2D ProjectedPointOnB = SegmentStartB + ProjectionOnB;

			// If the projected points are not equal, then we are not collinear
			return ProjectedPointOnA.Equals(ProjectedPointOnB);
		};

		FVector2D IntersectionPoint;
		FVector2D PreviousIntersectionPoint;
		bool bLastSegmentWasCollinear = false;

		for (int32 PointIndex = 0; PointIndex < PointCount - 1; ++PointIndex)
		{
			const FVector2D& PolySegmentStart = PolygonPoints[PointIndex];
			const FVector2D& PolySegmentEnd = PolygonPoints[PointIndex + 1];

			// Discard zero length segments
			if (PolySegmentStart.Equals(PolySegmentEnd))
			{
				continue;
			}

			const bool bSegmentIsCollinear = AreSegmentsCollinear(PolySegmentEnd, PolySegmentStart, SegmentStart, SegmentEnd);

			// Discard collinear segments and test if duplicate points should be included
			if (!bSegmentIsCollinear && SegmentIntersection2D(SegmentStart, SegmentEnd, PolySegmentStart, PolySegmentEnd, IntersectionPoint))
			{
				if ((OutIntersectionPoints.Num() > 0 && IntersectionPoint.Equals(PreviousIntersectionPoint)) || bLastSegmentWasCollinear)
				{
					if (TestDuplicatePoint(IntersectionPoint, PointIndex))
					{
						OutIntersectionPoints.Add(IntersectionPoint);
					}
				}
				else
				{
					OutIntersectionPoints.Add(IntersectionPoint);
				}

				PreviousIntersectionPoint = IntersectionPoint;
			}

			bLastSegmentWasCollinear = bSegmentIsCollinear;
		}

		// Special case to wrap the last segment
		{
			const FVector2D& PolySegmentStart = PolygonPoints.Last();
			const FVector2D& PolySegmentEnd = PolygonPoints[0];

			// Discard zero length segments
			if (PolySegmentStart.Equals(PolySegmentEnd))
			{
				return;
			}

			const bool bSegmentIsCollinear = AreSegmentsCollinear(PolySegmentEnd, PolySegmentStart, SegmentStart, SegmentEnd);

			if (bSegmentIsCollinear && OutIntersectionPoints.Num() > 0 && OutIntersectionPoints[0].Equals(PolygonPoints[0]))
			{
				// If the last segment is collinear, then we will double count the segment unless we cull the first polygon point
				OutIntersectionPoints.RemoveAt(0);
			}
			else if (!bSegmentIsCollinear && SegmentIntersection2D(SegmentStart, SegmentEnd, PolySegmentStart, PolySegmentEnd, IntersectionPoint))
			{
				if (OutIntersectionPoints.Num() > 0 || bLastSegmentWasCollinear)
				{
					if (IntersectionPoint.Equals(PolygonPoints[0]))
					{
						if (TestDuplicatePoint(IntersectionPoint, PointCount))
						{
							OutIntersectionPoints.Add(IntersectionPoint);
						}
					}
					else if (IntersectionPoint.Equals(PolygonPoints.Last()))
					{
						if (TestDuplicatePoint(IntersectionPoint, PointCount - 1))
						{
							OutIntersectionPoints.Add(IntersectionPoint);
						}
					}
					else
					{
						OutIntersectionPoints.Add(IntersectionPoint);
					}
				}
				else
				{
					OutIntersectionPoints.Add(IntersectionPoint);
				}
			}
		}
	}

	/** Tests if a point lies inside the given polygon by casting a ray to MaxDistance and counting the intersections */
	bool PointInsidePolygon2D(const TArray<FVector2D>& PolygonPoints, const FVector2D& Point, FVector::FReal MaxDistance) 
	{
		if (PolygonPoints.Num() < 3)
		{
			return false;
		}

		TArray<FVector2D> IntersectionPoints;
		SegmentPolygonIntersection2D(Point, Point + FVector2D(MaxDistance, 0), PolygonPoints, IntersectionPoints);

		// Odd number of intersections means we are inside the polygon
		return (IntersectionPoints.Num() % 2) == 1;
	}
}

namespace PCGSplineSampler
{
	struct FStepSampler
	{
		FStepSampler(const UPCGPolyLineData* InLineData)
			: LineData(InLineData)
		{
			check(LineData);
			CurrentSegmentIndex = 0;
		}

		virtual void Step(FTransform& OutTransform, FBox& OutBox) = 0;

		bool IsDone() const
		{
			return CurrentSegmentIndex >= LineData->GetNumSegments();
		}

		const UPCGPolyLineData* LineData;
		int CurrentSegmentIndex;
	};

	struct FSubdivisionStepSampler : public FStepSampler
	{
		FSubdivisionStepSampler(const UPCGPolyLineData* InLineData, const FPCGSplineSamplerParams& Params)
			: FStepSampler(InLineData)
		{
			NumSegments = LineData->GetNumSegments();
			SubdivisionsPerSegment = Params.SubdivisionsPerSegment;

			CurrentSegmentIndex = 0;
			SubpointIndex = 0;
		}

		virtual void Step(FTransform& OutTransform, FBox& OutBox) override
		{
			const FVector::FReal SegmentLength = LineData->GetSegmentLength(CurrentSegmentIndex);
			const FVector::FReal SegmentStep = SegmentLength / (SubdivisionsPerSegment + 1);

			OutTransform = LineData->GetTransformAtDistance(CurrentSegmentIndex, SubpointIndex * SegmentStep, &OutBox);

			if (SubpointIndex == 0)
			{
				const int PreviousSegmentIndex = (CurrentSegmentIndex > 0 ? CurrentSegmentIndex : NumSegments) - 1;
				const FVector::FReal PreviousSegmentLength = LineData->GetSegmentLength(PreviousSegmentIndex);
				FTransform PreviousSegmentEndTransform = LineData->GetTransformAtDistance(PreviousSegmentIndex, PreviousSegmentLength);

				if ((PreviousSegmentEndTransform.GetLocation() - OutTransform.GetLocation()).Length() <= KINDA_SMALL_NUMBER)
				{
					OutBox.Min.X *= 0.5 * PreviousSegmentLength / (PreviousSegmentEndTransform.GetScale3D().X * (SubdivisionsPerSegment + 1));
				}
				else
				{
					OutBox.Min.X *= 0.5 * SegmentStep / OutTransform.GetScale3D().X;
				}
			}
			else
			{
				OutBox.Min.X *= 0.5 * SegmentStep / OutTransform.GetScale3D().X;
			}

			OutBox.Max.X *= 0.5 * SegmentStep / OutTransform.GetScale3D().X;

			++SubpointIndex;
			if (SubpointIndex > SubdivisionsPerSegment)
			{
				SubpointIndex = 0;
				++CurrentSegmentIndex;
			}
		}

		int NumSegments;
		int SubdivisionsPerSegment;
		int SubpointIndex;
	};

	struct FDistanceStepSampler : public FStepSampler
	{
		FDistanceStepSampler(const UPCGPolyLineData* InLineData, const FPCGSplineSamplerParams& Params)
			: FStepSampler(InLineData)
		{
			DistanceIncrement = Params.DistanceIncrement;
			CurrentDistance = 0;
		}

		virtual void Step(FTransform& OutTransform, FBox& OutBox) override
		{
			FVector::FReal CurrentSegmentLength = LineData->GetSegmentLength(CurrentSegmentIndex);
			OutTransform = LineData->GetTransformAtDistance(CurrentSegmentIndex, CurrentDistance, &OutBox);

			OutBox.Min.X *= DistanceIncrement / OutTransform.GetScale3D().X;
			OutBox.Max.X *= DistanceIncrement / OutTransform.GetScale3D().X;

			CurrentDistance += DistanceIncrement;
			while(CurrentDistance > CurrentSegmentLength)
			{
				CurrentDistance -= CurrentSegmentLength;
				++CurrentSegmentIndex;
				if (!IsDone())
				{
					CurrentSegmentLength = LineData->GetSegmentLength(CurrentSegmentIndex);
				}
				else
				{
					break;
				}
			}
		}

		FVector::FReal DistanceIncrement;
		FVector::FReal CurrentDistance;
	};

	struct FDimensionSampler
	{
		FDimensionSampler(const UPCGPolyLineData* InLineData, const UPCGSpatialData* InSpatialData)
		{
			check(InLineData);
			LineData = InLineData;
			SpatialData = InSpatialData;
		}

		virtual void Sample(const FTransform& InTransform, const FBox& InBox, UPCGPointData* OutPointData)
		{
			FPCGPoint TrivialPoint;
			if(SpatialData->SamplePoint(InTransform, InBox, TrivialPoint, OutPointData->Metadata))
			{
				OutPointData->GetMutablePoints().Add(TrivialPoint);
			}
		}

		const UPCGPolyLineData* LineData;
		const UPCGSpatialData* SpatialData;
	};

	struct FVolumeSampler : public FDimensionSampler
	{
		FVolumeSampler(const UPCGPolyLineData* InLineData, const UPCGSpatialData* InSpatialData, const FPCGSplineSamplerParams& Params)
			: FDimensionSampler(InLineData, InSpatialData)
		{
			Fill = Params.Fill;
			NumPlanarSteps = 1 + ((Params.Dimension == EPCGSplineSamplingDimension::OnVertical) ? 0 : Params.NumPlanarSubdivisions);
			NumHeightSteps = 1 + ((Params.Dimension == EPCGSplineSamplingDimension::OnHorizontal) ? 0 : Params.NumHeightSubdivisions);
		}

		virtual void Sample(const FTransform& InTransform, const FBox& InBox, UPCGPointData* OutPointData) override
		{
			// We're assuming that we can scale against the origin in this method so this should always be true.
			// We will also assume that we can separate the curve into 4 ellipse sections for radius checks
			check(InBox.Max.Y > 0 && InBox.Min.Y < 0 && InBox.Max.Z > 0 && InBox.Min.Z < 0);

			const FVector::FReal YHalfStep = 0.5 * (InBox.Max.Y - InBox.Min.Y) / (FVector::FReal)NumPlanarSteps;
			const FVector::FReal ZHalfStep = 0.5 * (InBox.Max.Z - InBox.Min.Z) / (FVector::FReal)NumHeightSteps;

			FBox SubBox = InBox;
			SubBox.Min /= FVector(1.0, NumPlanarSteps, NumHeightSteps);
			SubBox.Max /= FVector(1.0, NumPlanarSteps, NumHeightSteps);

			FPCGPoint SeedPoint;
			if (SpatialData->SamplePoint(InTransform, InBox, SeedPoint, nullptr))
			{
				// Assuming the the normal to the curve is on the Y axis
				const FVector YAxis = SeedPoint.Transform.GetScaledAxis(EAxis::Y);
				const FVector ZAxis = SeedPoint.Transform.GetScaledAxis(EAxis::Z);

				// TODO: we can optimize this if we are in the "edges only case" to pick only values that
				FVector::FReal CurrentZ = (InBox.Min.Z + ZHalfStep);
				while(CurrentZ <= InBox.Max.Z - ZHalfStep + KINDA_SMALL_NUMBER)
				{
					// Compute inner/outer distance Z contribution (squared value since we'll compare against 1)
					const FVector::FReal InnerZ = ((NumHeightSteps > 1) ? (CurrentZ - FMath::Sign(CurrentZ) * ZHalfStep) : 0);
					const FVector::FReal OuterZ = ((NumHeightSteps > 1) ? (CurrentZ + FMath::Sign(CurrentZ) * ZHalfStep) : 0);

					// TODO: based on the current Z, we can compute the "unit" circle (as seen below) so we don't run outside of it by design, which would be a bit more efficient
					//  care needs to be taken to make sure we are on the same "steps" though
					FVector::FReal CurrentY = (InBox.Min.Y + YHalfStep);
					while(CurrentY <= InBox.Max.Y - YHalfStep + KINDA_SMALL_NUMBER)
					{
						// Compute inner/outer distance Y contribution
						const FVector::FReal InnerY = ((NumPlanarSteps > 1) ? (CurrentY - FMath::Sign(CurrentY) * YHalfStep) : 0);
						const FVector::FReal OuterY = ((NumPlanarSteps > 1) ? (CurrentY + FMath::Sign(CurrentY) * YHalfStep) : 0);

						bool bTestPoint = true;
						const FVector::FReal InnerDistance = FMath::Square((CurrentZ >= 0) ? (InnerZ / InBox.Max.Z) : (InnerZ / InBox.Min.Z)) + FMath::Square((CurrentY >= 0) ? (InnerY / InBox.Max.Y) : (InnerY / InBox.Min.Y));
						const FVector::FReal OuterDistance = FMath::Square((CurrentZ >= 0) ? (OuterZ / InBox.Max.Z) : (OuterZ / InBox.Min.Z)) + FMath::Square((CurrentY >= 0) ? (OuterY / InBox.Max.Y) : (OuterY / InBox.Min.Y));

						// Check if we should consider this point based on the fill mode / the position in the iteration
						// If the normalized z^2 + y^2 > 1, then there's no point in testing this point
						if (InnerDistance >= 1.0 + KINDA_SMALL_NUMBER)
						{
							bTestPoint = false; // fully outside the unit circle
						}
						else if (Fill == EPCGSplineSamplingFill::EdgesOnly && OuterDistance < 1.0 - KINDA_SMALL_NUMBER)
						{
							bTestPoint = false; // Not the edge point
						}

						if (bTestPoint)
						{
							FVector TentativeLocation = InTransform.GetLocation() + YAxis * CurrentY + ZAxis * CurrentZ;

							FTransform TentativeTransform = InTransform;
							TentativeTransform.SetLocation(TentativeLocation);

							FPCGPoint OutPoint;
							if (SpatialData->SamplePoint(TentativeTransform, SubBox, OutPoint, OutPointData->Metadata))
							{
								OutPointData->GetMutablePoints().Add(OutPoint);
							}
						}

						CurrentY += 2.0 * YHalfStep;
					}

					CurrentZ += 2.0 * ZHalfStep;
				}
			}
		}

		EPCGSplineSamplingFill Fill;
		int NumPlanarSteps;
		int NumHeightSteps;
	};

	void SampleLineData(const UPCGPolyLineData* LineData, const UPCGSpatialData* SpatialData, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData)
	{
		check(LineData && OutPointData);

		FSubdivisionStepSampler SubdivisionSampler(LineData, Params);
		FDistanceStepSampler DistanceSampler(LineData, Params);

		FStepSampler* Sampler = ((Params.Mode == EPCGSplineSamplingMode::Subdivision) ? static_cast<FStepSampler*>(&SubdivisionSampler) : static_cast<FStepSampler*>(&DistanceSampler));

		FDimensionSampler TrivialDimensionSampler(LineData, SpatialData);
		FVolumeSampler VolumeSampler(LineData, SpatialData, Params);

		FDimensionSampler* ExtentsSampler = ((Params.Dimension == EPCGSplineSamplingDimension::OnSpline) ? &TrivialDimensionSampler : static_cast<FDimensionSampler*>(&VolumeSampler));

		FTransform SeedTransform;

		while (!Sampler->IsDone())
		{
			FBox SeedBox = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector);
			// Get seed transform/box
			Sampler->Step(SeedTransform, SeedBox);
			// From seed point, sample in other dimensions as needed
			ExtentsSampler->Sample(SeedTransform, SeedBox, OutPointData);
		}

		// Finally, set seed on points based on position
		for (FPCGPoint& Point : OutPointData->GetMutablePoints())
		{
			Point.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Point.Transform.GetLocation());
		}
	}
 
	void SampleInteriorData(const UPCGPolyLineData* LineData, const UPCGSpatialData* SpatialData, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData)
	{
		check(LineData && OutPointData);

		TSoftObjectPtr<USplineComponent> Spline;

		// TODO: handle projected splines
		if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(LineData))
		{
			Spline = SplineData->Spline;
		}
		else if (const UPCGLandscapeSplineData* LandscapeSplineData = Cast<UPCGLandscapeSplineData>(LineData))
		{
			UE_LOG(LogPCG, Error, TEXT("LandscapeSplines are not supported for interior sampling"));
			return;
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Could not create UPCGSplineData from LineData"));
			return;
		}

		check(Spline);

		if (!Spline->IsClosedLoop())
		{
			UE_LOG(LogPCG, Error, TEXT("Interior sampling only generates for closed shapes"));
			return;
		}

		const FVector MinPoint = Spline->Bounds.Origin - Spline->Bounds.BoxExtent;
		const FVector MaxPoint = Spline->Bounds.Origin + Spline->Bounds.BoxExtent;

		const FVector::FReal MaxDimension = FMath::Max(Spline->Bounds.BoxExtent.X, Spline->Bounds.BoxExtent.Y) * 2.f;
		const FVector::FReal MaxDimensionSquared = MaxDimension * MaxDimension;

		const FRichCurve* DensityFalloffCurve = Params.InteriorDensityFalloffCurve.GetRichCurveConst();
		const bool bGenerateMedialAxis = DensityFalloffCurve != nullptr && DensityFalloffCurve->GetNumKeys() > 0;
		const bool bFindNearestSplineKey = Params.InteriorOrientation == EPCGSplineSamplingInteriorOrientation::FollowCurvature || (bGenerateMedialAxis && !Params.bTreatSplineAsPolyline);
		const bool bProjectOntoSurface = Params.bProjectOntoSurface || bFindNearestSplineKey;

		const FVector::FReal BoundExtents = Params.InteriorBorderSampleSpacing * 0.5f;
		const FVector BoundsMin = FVector::One() * -BoundExtents;
		const FVector BoundsMax = FVector::One() * BoundExtents;

		const FVector::FReal SplineLength = Spline->GetSplineLength();

		TArray<FVector> SplineSamplePoints;

		if (Params.bTreatSplineAsPolyline)
		{
			// Treat spline interface points as vertices of a polyline
			const int NumSegments = LineData->GetNumSegments();
			for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
			{
				SplineSamplePoints.Add(LineData->GetLocationAtDistance(SegmentIndex, 0));
			}
		}
		else
		{
			// Get sample points along the spline that are higher resolution than our PolyLine
			for (FVector::FReal Length = 0.f; Length < SplineLength; Length += Params.InteriorBorderSampleSpacing)
			{
				SplineSamplePoints.Add(Spline->GetLocationAtDistanceAlongSpline(Length, ESplineCoordinateSpace::World));
			}
		}

		// Flat polygon representation of our spline points
		TArray<FVector2D> SplineSamplePoints2D;
		for (const FVector& Point : SplineSamplePoints)
		{
			SplineSamplePoints2D.Add(FVector2D(Point));
		}

		TArray<TTuple<FVector2D, FVector2D>> MedialAxisEdges;

		// Compute the Medial Axis as a subset of the Voronoi Diagram of the PolyLine points
		if (bGenerateMedialAxis)
		{
			TArray<FVector> PolygonPoints; // Top-down 2D projection polygon of the spline points
			if (Params.bTreatSplineAsPolyline)
			{
				// If we already computed the polygon, use a copy instead of generating it again
				for (const FVector& SplinePoint : SplineSamplePoints)
				{
					FVector& PolygonPoint = PolygonPoints.Add_GetRef(SplinePoint);
					PolygonPoint.Z = MinPoint.Z;
				}
			}
			else
			{
				const int NumSegments = LineData->GetNumSegments();
				for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
				{
					FVector& PolygonPoint = PolygonPoints.Add_GetRef(LineData->GetLocationAtDistance(SegmentIndex, 0));
					PolygonPoint.Z = MinPoint.Z;
				}
			}

			TArray<FVector2D> PolygonPoints2D;
			for (const FVector& Point : PolygonPoints)
			{
				PolygonPoints2D.Add(FVector2D(Point));
			}

			TArray<TTuple<FVector, FVector>> VoronoiEdges;
			TArray<int32> CellMember;

			GetVoronoiEdges(PolygonPoints, FBox(MinPoint, FVector(MaxPoint.X, MaxPoint.Y, MinPoint.Z)), VoronoiEdges, CellMember);

			// Find the subset of the Voronoi Diagram which composes the Medial Axis
			for (const TTuple<FVector, FVector>& Edge : VoronoiEdges)
			{
				// Discard any edges which intersect the polygon
				bool bDiscard = false;
				for (int32 PointIndex = 0; PointIndex < PolygonPoints.Num(); ++PointIndex)
				{
					FVector2D IntersectionPoint;
					if (PCGSplineSamplerHelpers::SegmentIntersection2D(PolygonPoints2D[PointIndex], PolygonPoints2D[(PointIndex + 1) % PolygonPoints2D.Num()], FVector2D(Edge.Get<0>()), FVector2D(Edge.Get<1>()), IntersectionPoint))
					{
						bDiscard = true;
						break;
					}
				}

				if (bDiscard)
				{
					continue;
				}

				// If either of the points lies within the polygon, the segment must lie within the polygon
				if (PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints2D, FVector2D(Edge.Get<0>()), MaxDimension))
				{
					MedialAxisEdges.Add(Edge);
				}
			}
		}

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		TArray<FVector::FReal> DistancesSquaredOnXY;

		// RayPadding should be large enough to avoid potential misclassifications in SegmentPolygonIntersection2D
		const FVector::FReal RayPadding = 100.f;
		const FVector::FReal MinY = FMath::CeilToDouble(MinPoint.Y / Params.InteriorSampleSpacing) * Params.InteriorSampleSpacing;
		const FVector::FReal MaxY = FMath::FloorToDouble(MaxPoint.Y / Params.InteriorSampleSpacing) * Params.InteriorSampleSpacing;

		// Point sampling
		for (FVector::FReal Y = MinY; Y < MaxY + UE_KINDA_SMALL_NUMBER; Y += Params.InteriorSampleSpacing)
		{
			const FVector2D RayMin(MinPoint.X - RayPadding, Y);
			const FVector2D RayMax(MaxPoint.X + RayPadding, Y);

			// Get the intersections along this ray, sorted by Y value
			TArray<FVector2D> Intersections;
			PCGSplineSamplerHelpers::SegmentPolygonIntersection2D(RayMin, RayMax, SplineSamplePoints2D, Intersections);

			if (Intersections.Num() % 2 != 0)
			{
				UE_LOG(LogPCG, Error, TEXT("Intersection test failed, skipping samples for this row"));
				continue;
			}

			Intersections.Sort([](const FVector2D& LHS, const FVector2D& RHS) { return LHS.X < RHS.X; });

			// TODO: async processing
			// Each pair of intersections defines a range in which point samples may lie
			for (int32 RangeIndex = 0; RangeIndex < Intersections.Num(); RangeIndex += 2)
			{
				const FVector::FReal MinX = FMath::CeilToDouble(Intersections[RangeIndex].X / Params.InteriorSampleSpacing) * Params.InteriorSampleSpacing;
				const FVector::FReal MaxX = FMath::FloorToDouble(Intersections[RangeIndex + 1].X / Params.InteriorSampleSpacing) * Params.InteriorSampleSpacing;

				if (MaxX - MinX < Params.InteriorSampleSpacing)
				{
					continue;
				}

				for (FVector::FReal X = MinX; X < MaxX + UE_KINDA_SMALL_NUMBER; X += Params.InteriorSampleSpacing)
				{
					FPCGPoint& Point = OutPoints.Emplace_GetRef();

					const FVector2D SampleLocation = FVector2D(X, Y);
					FVector SurfaceLocation = FVector(SampleLocation, MinPoint.Z);

					if (bProjectOntoSurface)
					{
						// Precompute 2D distance to every spline point
						DistancesSquaredOnXY.Reset(SplineSamplePoints.Num());
						for (const FVector& SplinePoint : SplineSamplePoints)
						{
							FVector::FReal DistanceSquared = FVector::DistSquaredXY(SurfaceLocation, SplinePoint);
							DistancesSquaredOnXY.Add(DistanceSquared);
						}

						// Compute average Z value weighted by 1 / Distance^2
						FVector::FReal SumZ = 0.f;
						FVector::FReal SumWeights = 0.f;
						for (int32 PointIndex = 0; PointIndex < SplineSamplePoints.Num(); ++PointIndex)
						{
							const FVector::FReal DistanceSquared = DistancesSquaredOnXY[PointIndex];

							// If sample point overlaps exactly with a border point, then that must be the height
							if (FMath::IsNearlyZero(DistanceSquared))
							{
								SumZ = SplineSamplePoints[PointIndex].Z;
								SumWeights = 1.f;
								break;
							}

							// TODO: it would be more accurate to use distance to the polyline instead of distance to the polyline points,
							// however it would also be much more expensive. Perhaps worth investigating when Params.bTreatSplineAsPolyline is true
							const FVector::FReal Weight = 1.f / DistanceSquared;

							SumWeights += Weight;
							SumZ += SplineSamplePoints[PointIndex].Z * Weight;
						}

						SurfaceLocation.Z = SumZ / SumWeights;
					}

					// if bTreatAsPolyline, then we shouldnt use this, we should use nearest point on the polygon line segments
					const float NearestSplineKey = bFindNearestSplineKey ? Spline->FindInputKeyClosestToWorldLocation(SurfaceLocation) : 0.f;

					if (Params.bProjectOntoSurface)
					{
						Point.Transform.SetLocation(SurfaceLocation);
					}
					else
					{
						Point.Transform.SetLocation(FVector(SampleLocation, MinPoint.Z));
					}

					if (Params.InteriorOrientation == EPCGSplineSamplingInteriorOrientation::FollowCurvature)
					{
						Point.Transform.SetRotation(Spline->GetQuaternionAtSplineInputKey(NearestSplineKey, ESplineCoordinateSpace::World));
					}

					// Calculate density fall off
					if (bGenerateMedialAxis && MedialAxisEdges.Num())
					{
						FVector::FReal SmallestDistSquared = MaxDimensionSquared;

						// Find distance from SampleLocation to MedialAxis
						for (const TTuple<FVector2D, FVector2D>& Edge : MedialAxisEdges)
						{
							const FVector::FReal DistSquared = FMath::PointDistToSegmentSquared(FVector(SampleLocation, 0), FVector(Edge.Get<0>(), 0), FVector(Edge.Get<1>(), 0));

							if (DistSquared < SmallestDistSquared)
							{
								SmallestDistSquared = DistSquared;
							}
						}

						const FVector::FReal SmallestDist = FMath::Sqrt(SmallestDistSquared);

						FVector::FReal PointToSplineDist = 0.f;

						if (Params.bTreatSplineAsPolyline)
						{
							SmallestDistSquared = MaxDimensionSquared;
							for (int32 PointIndex = 0; PointIndex < SplineSamplePoints.Num(); ++PointIndex)
							{
								const FVector::FReal DistSquared = FMath::PointDistToSegmentSquared(SurfaceLocation, SplineSamplePoints[PointIndex], SplineSamplePoints[(PointIndex + 1) % SplineSamplePoints.Num()]);

								if (DistSquared < SmallestDistSquared)
								{
									SmallestDistSquared = DistSquared;
								}
							}
							
							PointToSplineDist = FMath::Sqrt(SmallestDistSquared);
						}
						else
						{
							const FVector NearestSplineLocation = Spline->GetLocationAtSplineInputKey(NearestSplineKey, ESplineCoordinateSpace::World);
							PointToSplineDist = FVector2D::Distance(SampleLocation, FVector2D(NearestSplineLocation.X, NearestSplineLocation.Y));
						}

						// Linear fall off in the range [0, 1]
						const FVector::FReal T = SmallestDist / (SmallestDist + PointToSplineDist + UE_KINDA_SMALL_NUMBER);

						Point.Density = DensityFalloffCurve->Eval(T);
					}
					else
					{
						Point.Density = 1.f;
					}

					Point.BoundsMin = BoundsMin;
					Point.BoundsMax = BoundsMax;
					Point.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Point.Transform.GetLocation());
				}
			}
		}
	}

	const UPCGPolyLineData* GetPolyLineData(const UPCGSpatialData* InSpatialData)
	{
		if (!InSpatialData)
		{
			return nullptr;
		}

		if (const UPCGPolyLineData* LineData = Cast<const UPCGPolyLineData>(InSpatialData))
		{
			return LineData;
		}
		else if (const UPCGSplineProjectionData* SplineProjectionData = Cast<const UPCGSplineProjectionData>(InSpatialData))
		{
			return SplineProjectionData->GetSpline();
		}
		else if (const UPCGIntersectionData* Intersection = Cast<const UPCGIntersectionData>(InSpatialData))
		{
			if (const UPCGPolyLineData* IntersectionA = GetPolyLineData(Intersection->A))
			{
				return IntersectionA;
			}
			else if (const UPCGPolyLineData* IntersectionB = GetPolyLineData(Intersection->B))
			{
				return IntersectionB;
			}
		}

		return nullptr;
	}
}

FPCGElementPtr UPCGSplineSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGSplineSamplerElement>();
}

TArray<FPCGPinProperties> UPCGSplineSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, /*EPCGDataType::Point |*/ EPCGDataType::Spline | EPCGDataType::LandscapeSpline);

	return PinProperties;
}

bool FPCGSplineSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSplineSamplerElement::Execute);

	const UPCGSplineSamplerSettings* Settings = Context->GetInputSettings<UPCGSplineSamplerSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	FPCGSplineSamplerParams SamplerParams = Settings->Params;
	SamplerParams.Mode = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, Mode, Params);
	SamplerParams.Dimension = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, Dimension, Params);
	SamplerParams.Fill = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, Fill, Params);
	SamplerParams.SubdivisionsPerSegment = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, SubdivisionsPerSegment, Params);
	SamplerParams.DistanceIncrement = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, DistanceIncrement, Params);
	SamplerParams.NumPlanarSubdivisions = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, NumPlanarSubdivisions, Params);
	SamplerParams.NumHeightSubdivisions = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, NumHeightSubdivisions, Params);
	SamplerParams.InteriorSampleSpacing = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, InteriorSampleSpacing, Params);
	SamplerParams.InteriorBorderSampleSpacing = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, InteriorBorderSampleSpacing, Params);
	SamplerParams.InteriorOrientation = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, InteriorOrientation, Params);
	SamplerParams.bProjectOntoSurface = PCG_GET_OVERRIDEN_VALUE(&SamplerParams, bProjectOntoSurface, Params);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<const UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			continue;
		}

		// TODO: do something for point data approximations
		const UPCGPolyLineData* LineData = PCGSplineSampler::GetPolyLineData(SpatialData);

		if (!LineData)
		{
			continue;
		}

		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output = Input;

		UPCGPointData* SampledPointData = NewObject<UPCGPointData>();
		SampledPointData->InitializeFromData(SpatialData);
		Output.Data = SampledPointData;

		if (SamplerParams.Dimension == EPCGSplineSamplingDimension::OnInterior)
		{
			PCGSplineSampler::SampleInteriorData(LineData, SpatialData, SamplerParams, SampledPointData);
		}
		else
		{
			PCGSplineSampler::SampleLineData(LineData, SpatialData, SamplerParams, SampledPointData);
		}
	}

	return true;
}
