// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSplineSampler.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGLandscapeSplineData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Async/ParallelFor.h"
#include "Components/SplineComponent.h"
#include "Voronoi/Voronoi.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSplineSampler)

#define LOCTEXT_NAMESPACE "PCGSplineSamplerElement"

namespace PCGSplineSamplerConstants
{
	const FName SplineLabel = TEXT("Spline");
	const FName BoundingShapeLabel = TEXT("Bounding Shape");
}

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
		auto TestDuplicatePoint = [PointCount, &PolygonPoints](const FVector2D& IntersectionPoint, int32 Index) -> bool
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
	struct FSamplerResult
	{
		FTransform LocalTransform;
		FBox Box = FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector);
		FVector::FReal Curvature = 0;
		FVector::FReal PreviousDeltaAngle = 0;
		FVector::FReal NextDeltaAngle = 0;
	};

	void SetSeed(FPCGPoint& Point, const FVector& LSPosition, const FPCGSplineSamplerParams& Params)
	{
		if (!Params.bSeedFromLocalPosition)
		{
			if (!Params.bSeedFrom2DPosition)
			{
				Point.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Point.Transform.GetLocation());
			}
			else
			{
				const FVector WSPosition = Point.Transform.GetLocation();
				Point.Seed = PCGHelpers::ComputeSeed((int)WSPosition.X, (int)WSPosition.Y);
			}
		}
		else // Use provided local position
		{
			if (!Params.bSeedFrom2DPosition)
			{
				Point.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(LSPosition);
			}
			else
			{
				Point.Seed = PCGHelpers::ComputeSeed((int)LSPosition.X, (int)LSPosition.Y);
			}
		}
	}

	struct FStepSampler
	{
		FStepSampler(const UPCGPolyLineData* InLineData, const FPCGSplineSamplerParams& Params)
			: LineData(InLineData), bComputeCurvature(Params.bComputeCurvature)
		{
			check(LineData);
			CurrentSegmentIndex = 0;
		}

		virtual void Step(FSamplerResult& OutSamplerResult) = 0;

		bool IsDone() const
		{
			return CurrentSegmentIndex >= LineData->GetNumSegments();
		}

		const UPCGPolyLineData* LineData = nullptr;
		int CurrentSegmentIndex = 0;
		bool bComputeCurvature = false;
	};

	struct FSubdivisionStepSampler : public FStepSampler
	{
		FSubdivisionStepSampler(const UPCGPolyLineData* InLineData, const FPCGSplineSamplerParams& Params)
			: FStepSampler(InLineData, Params)
		{
			NumSegments = LineData->GetNumSegments();
			SubdivisionsPerSegment = Params.SubdivisionsPerSegment;

			CurrentSegmentIndex = 0;
			SubpointIndex = 0;
		}

		virtual void Step(FSamplerResult& OutResult) override
		{
			const FVector::FReal SegmentLength = LineData->GetSegmentLength(CurrentSegmentIndex);
			const FVector::FReal SegmentStep = SegmentLength / (SubdivisionsPerSegment + 1);

			FBox& OutBox = OutResult.Box;
			FTransform& OutTransform = OutResult.LocalTransform;
			OutTransform = LineData->GetTransformAtDistance(CurrentSegmentIndex, SubpointIndex * SegmentStep, /*bWorldSpace=*/false, &OutBox);

			if (bComputeCurvature)
			{
				OutResult.Curvature = LineData->GetCurvatureAtDistance(CurrentSegmentIndex, SubpointIndex * SegmentStep);
			}

			if (SubpointIndex == 0)
			{
				const int PreviousSegmentIndex = (CurrentSegmentIndex > 0 ? CurrentSegmentIndex : NumSegments) - 1;
				const FVector::FReal PreviousSegmentLength = LineData->GetSegmentLength(PreviousSegmentIndex);
				FTransform PreviousSegmentEndTransform = LineData->GetTransformAtDistance(PreviousSegmentIndex, PreviousSegmentLength, /*bWorldSpace=*/false);

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
			: FStepSampler(InLineData, Params)
		{
			DistanceIncrement = Params.DistanceIncrement;
			CurrentDistance = 0;
		}

		virtual void Step(FSamplerResult& OutResult) override
		{
			FVector::FReal CurrentSegmentLength = LineData->GetSegmentLength(CurrentSegmentIndex);
			FTransform& OutTransform = OutResult.LocalTransform;
			FBox& OutBox = OutResult.Box;
			OutTransform = LineData->GetTransformAtDistance(CurrentSegmentIndex, CurrentDistance, /*bWorldSpace=*/false, &OutBox);

			// Set min/max to half of extent
			OutBox.Min.X *= 0.5 * DistanceIncrement / OutTransform.GetScale3D().X;
			OutBox.Max.X *= 0.5 * DistanceIncrement / OutTransform.GetScale3D().X;

			if (bComputeCurvature)
			{
				OutResult.Curvature = LineData->GetCurvatureAtDistance(CurrentSegmentIndex, CurrentDistance);
			}

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
		FDimensionSampler(const UPCGPolyLineData* InLineData, const UPCGSpatialData* InBoundingShapeData, const UPCGSpatialData* InProjectionTarget, const FPCGProjectionParams& InProjectionParams, const FPCGSplineSamplerParams& InParams, UPCGPointData* OutPointData)
			: Params(InParams)
		{
			check(InLineData);
			LineData = InLineData;
			BoundingShapeData = InBoundingShapeData;
			ProjectionTargetData = InProjectionTarget;
			ProjectionParams = InProjectionParams;

			// Initialize metadata accessors if needed
			if (OutPointData && OutPointData->Metadata && (Params.bComputeDirectionDelta || Params.bComputeCurvature))
			{
				constexpr double DefaultValue = 0.0;
				if (Params.bComputeDirectionDelta)
				{
					NextDirectionDeltaAttribute = OutPointData->Metadata->FindOrCreateAttribute(Params.NextDirectionDeltaAttribute, DefaultValue);
					bSetMetadata |= (NextDirectionDeltaAttribute != nullptr);
				}

				if (Params.bComputeCurvature)
				{
					CurvatureAttribute = OutPointData->Metadata->FindOrCreateAttribute(Params.CurvatureAttribute, DefaultValue);
					bSetMetadata |= (CurvatureAttribute != nullptr);
				}
			}
		}

		virtual ~FDimensionSampler() = default;

		virtual void SetMetadata(const FSamplerResult& InResult, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata)
		{
			if (bSetMetadata)
			{
				OutMetadata->InitializeOnSet(OutPoint.MetadataEntry);
			}

			if (NextDirectionDeltaAttribute)
			{
				NextDirectionDeltaAttribute->SetValue(OutPoint.MetadataEntry, InResult.NextDeltaAngle);
			}

			if (CurvatureAttribute)
			{
				CurvatureAttribute->SetValue(OutPoint.MetadataEntry, InResult.Curvature);
			}
		}

		virtual void Sample(const FSamplerResult& InResult, UPCGPointData* OutPointData)
		{
			const FTransform Transform = InResult.LocalTransform * LineData->GetTransform();

			bool bValid = true;

			FPCGPoint OutPoint;
			OutPoint.Density = 1.0f;
			OutPoint.SetLocalBounds(InResult.Box);

			if (ProjectionTargetData)
			{
				FPCGPoint ProjPoint;
				bValid = ProjectionTargetData->ProjectPoint(Transform, InResult.Box, ProjectionParams, ProjPoint, nullptr);
				OutPoint.Transform = ProjPoint.Transform;
			}
			else
			{
				OutPoint.Transform = Transform;
			}

			FPCGPoint BoundsTestPoint;
			if (bValid && (!BoundingShapeData || BoundingShapeData->SamplePoint(Transform, InResult.Box, BoundsTestPoint, nullptr)))
			{
				SetSeed(OutPoint, InResult.LocalTransform.GetLocation(), Params);
				SetMetadata(InResult, OutPoint, OutPointData->Metadata);
				OutPointData->GetMutablePoints().Add(OutPoint);
			}
		}

		const FPCGSplineSamplerParams Params;
		const UPCGPolyLineData* LineData = nullptr;
		const UPCGSpatialData* BoundingShapeData = nullptr;
		const UPCGSpatialData* ProjectionTargetData = nullptr;
		FPCGProjectionParams ProjectionParams;

		bool bSetMetadata = false;
		FPCGMetadataAttribute<double>* NextDirectionDeltaAttribute = nullptr;
		FPCGMetadataAttribute<double>* CurvatureAttribute = nullptr;
	};

	/** Samples in a volume surrounding the poly line. */
	struct FVolumeSampler : public FDimensionSampler
	{
		FVolumeSampler(const UPCGPolyLineData* InLineData, const UPCGSpatialData* InBoundingShapeData, const UPCGSpatialData* InProjectionTarget, const FPCGProjectionParams& InProjectionParams, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData)
			: FDimensionSampler(InLineData, InBoundingShapeData, InProjectionTarget, InProjectionParams, Params, OutPointData)
		{
			Fill = Params.Fill;
			NumPlanarSteps = 1 + ((Params.Dimension == EPCGSplineSamplingDimension::OnVertical) ? 0 : Params.NumPlanarSubdivisions);
			NumHeightSteps = 1 + ((Params.Dimension == EPCGSplineSamplingDimension::OnHorizontal) ? 0 : Params.NumHeightSubdivisions);
		}

		virtual void Sample(const FSamplerResult& InResult, UPCGPointData* OutPointData) override
		{
			const FTransform& TransformLS = InResult.LocalTransform;
			FTransform TransformWS = TransformLS * LineData->GetTransform();
			FBox InBox = InResult.Box;

			// We're assuming that we can scale against the origin in this method so this should always be true.
			// We will also assume that we can separate the curve into 4 ellipse sections for radius checks
			check(InBox.Max.Y > 0 && InBox.Min.Y < 0 && InBox.Max.Z > 0 && InBox.Min.Z < 0);

			const FVector::FReal YHalfStep = 0.5 * (InBox.Max.Y - InBox.Min.Y) / (FVector::FReal)NumPlanarSteps;
			const FVector::FReal ZHalfStep = 0.5 * (InBox.Max.Z - InBox.Min.Z) / (FVector::FReal)NumHeightSteps;

			FBox SubBox = InBox;
			SubBox.Min /= FVector(1.0, NumPlanarSteps, NumHeightSteps);
			SubBox.Max /= FVector(1.0, NumPlanarSteps, NumHeightSteps);

			// TODO: we can optimize this if we are in the "edges only case" to only pick boundary values.
			FVector::FReal CurrentZ = (InBox.Min.Z + ZHalfStep);
			while (CurrentZ <= InBox.Max.Z - ZHalfStep + KINDA_SMALL_NUMBER)
			{
				// Compute inner/outer distance Z contribution (squared value since we'll compare against 1)
				const FVector::FReal InnerZ = ((NumHeightSteps > 1) ? (CurrentZ - FMath::Sign(CurrentZ) * ZHalfStep) : 0);
				const FVector::FReal OuterZ = ((NumHeightSteps > 1) ? (CurrentZ + FMath::Sign(CurrentZ) * ZHalfStep) : 0);

				// TODO: based on the current Z, we can compute the "unit" circle (as seen below) so we don't run outside of it by design, which would be a bit more efficient
				//  care needs to be taken to make sure we are on the same "steps" though
				for (FVector::FReal CurrentY = (InBox.Min.Y + YHalfStep); CurrentY <= (InBox.Max.Y - YHalfStep + KINDA_SMALL_NUMBER); CurrentY += 2.0 * YHalfStep)
				{
					// Compute inner/outer distance Y contribution
					const FVector::FReal InnerY = ((NumPlanarSteps > 1) ? (CurrentY - FMath::Sign(CurrentY) * YHalfStep) : 0);
					const FVector::FReal OuterY = ((NumPlanarSteps > 1) ? (CurrentY + FMath::Sign(CurrentY) * YHalfStep) : 0);

					const FVector::FReal InnerDistance = FMath::Square((CurrentZ >= 0) ? (InnerZ / InBox.Max.Z) : (InnerZ / InBox.Min.Z)) + FMath::Square((CurrentY >= 0) ? (InnerY / InBox.Max.Y) : (InnerY / InBox.Min.Y));
					const FVector::FReal OuterDistance = FMath::Square((CurrentZ >= 0) ? (OuterZ / InBox.Max.Z) : (OuterZ / InBox.Min.Z)) + FMath::Square((CurrentY >= 0) ? (OuterY / InBox.Max.Y) : (OuterY / InBox.Min.Y));

					// Check if we should consider this point based on the fill mode / the position in the iteration
					// If the normalized z^2 + y^2 > 1, then there's no point in testing this point
					if (InnerDistance >= 1.0 + KINDA_SMALL_NUMBER)
					{
						continue; // fully outside the unit circle
					}
					else if (Fill == EPCGSplineSamplingFill::EdgesOnly && OuterDistance < 1.0 - KINDA_SMALL_NUMBER)
					{
						continue; // Not the edge point
					}

					FVector TentativeLocationLS = FVector(0.0, CurrentY, CurrentZ);
					FTransform TentativeTransform = TransformWS;
					TentativeTransform.SetLocation(TransformWS.TransformPosition(TentativeLocationLS));

					// Sample spline to get density.
					FPCGPoint OutPoint;
					if (!LineData->SamplePoint(TentativeTransform, SubBox, OutPoint, nullptr))
					{
						continue;
					}

					// Project point if projection target provided.
					if (ProjectionTargetData)
					{
						FPCGPoint ProjPoint;
						if (!ProjectionTargetData->ProjectPoint(OutPoint.Transform, OutPoint.GetLocalBounds(), ProjectionParams, ProjPoint, nullptr))
						{
							continue;
						}
						OutPoint.Transform = ProjPoint.Transform;
					}

					// Test point against bounds.
					FPCGPoint BoundsTestPoint;
					if (BoundingShapeData && !BoundingShapeData->SamplePoint(OutPoint.Transform, OutPoint.GetLocalBounds(), BoundsTestPoint, nullptr))
					{
						continue;
					}

					SetSeed(OutPoint, TentativeLocationLS, Params);
					SetMetadata(InResult, OutPoint, OutPointData->Metadata);
					OutPointData->GetMutablePoints().Add(OutPoint);
				}

				CurrentZ += 2.0 * ZHalfStep;
			}
		}

		EPCGSplineSamplingFill Fill;
		int NumPlanarSteps;
		int NumHeightSteps;
	};

	/** Samples on spline or within volume around it. */
	void SampleLineData(const UPCGPolyLineData* LineData, const UPCGSpatialData* InBoundingShapeData, const UPCGSpatialData* InProjectionTarget, const FPCGProjectionParams& InProjectionParams, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData)
	{
		check(LineData && OutPointData);

		FSubdivisionStepSampler SubdivisionSampler(LineData, Params);
		FDistanceStepSampler DistanceSampler(LineData, Params);

		FStepSampler* Sampler = ((Params.Mode == EPCGSplineSamplingMode::Subdivision) ? static_cast<FStepSampler*>(&SubdivisionSampler) : static_cast<FStepSampler*>(&DistanceSampler));

		FDimensionSampler TrivialDimensionSampler(LineData, InBoundingShapeData, InProjectionTarget, InProjectionParams, Params, OutPointData);
		FVolumeSampler VolumeSampler(LineData, InBoundingShapeData, InProjectionTarget, InProjectionParams, Params, OutPointData);

		FDimensionSampler* ExtentsSampler = ((Params.Dimension == EPCGSplineSamplingDimension::OnSpline) ? &TrivialDimensionSampler : static_cast<FDimensionSampler*>(&VolumeSampler));

		bool bHasPreviousPoint = false;
		FSamplerResult Results[2];
		FSamplerResult* PreviousResult = &Results[0];
		FSamplerResult* CurrentResult = &Results[1];

		if (!Sampler->IsDone())
		{
			Sampler->Step(*PreviousResult);
			bHasPreviousPoint = true;
		}
		
		while(!Sampler->IsDone())
		{
			// Sample point on spline proper
			Sampler->Step(*CurrentResult);

			// Get unsigned angle difference between the two points
			const FVector::FReal DeltaSinAngle = ((CurrentResult->LocalTransform.GetUnitAxis(EAxis::X) ^ PreviousResult->LocalTransform.GetUnitAxis(EAxis::X)) | PreviousResult->LocalTransform.GetUnitAxis(EAxis::Z));
			// Normalize value to be between -1 and 1
			FVector::FReal DeltaAngle = FMath::Asin(DeltaSinAngle) / UE_HALF_PI;

			PreviousResult->NextDeltaAngle = DeltaAngle;
			CurrentResult->PreviousDeltaAngle = -DeltaAngle;
			// Perform samples "around" the spline depending on the settings
			ExtentsSampler->Sample(*PreviousResult, OutPointData);

			// Prepare for next iteration
			Swap(PreviousResult, CurrentResult);
			*CurrentResult = FSamplerResult();
		}
		
		if (bHasPreviousPoint)
		{
			ExtentsSampler->Sample(*PreviousResult, OutPointData);
		}
	}
 
	/** Samples 2D region bounded by spline. */
	void SampleInteriorData(FPCGContext* Context, const UPCGPolyLineData* LineData, const UPCGSpatialData* InBoundingShape, const UPCGSpatialData* InProjectionTarget, const FPCGProjectionParams& InProjectionParams, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData)
	{
		check(Context && LineData && OutPointData);

		const FPCGSplineStruct* Spline = nullptr;

		if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(LineData))
		{
			Spline = &SplineData->SplineStruct;
		}
		else if (const UPCGLandscapeSplineData* LandscapeSplineData = Cast<UPCGLandscapeSplineData>(LineData))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("LandscapeSplinesNotSupported", "Input data of type Landscape Spline are not supported for interior sampling"));
			return;
		}
		else
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("CouldNotCreateSplineData", "Could not create UPCGSplineData from LineData"));
			return;
		}

		check(Spline);

		if (!Spline->IsClosedLoop())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("ShapeNotClosed", "Interior sampling only generates for closed shapes, enable the 'Closed Loop' setting on the spline"));
			return;
		}

		const FBoxSphereBounds SplineLocalBounds = Spline->LocalBounds;
		const FVector MinPoint = SplineLocalBounds.Origin - SplineLocalBounds.BoxExtent;
		const FVector MaxPoint = SplineLocalBounds.Origin + SplineLocalBounds.BoxExtent;

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
			SplineSamplePoints.Reserve(NumSegments);

			for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
			{
				SplineSamplePoints.Add(LineData->GetLocationAtDistance(SegmentIndex, 0, /*bWorldSpace=*/false));
			}
		}
		else if(Params.InteriorBorderSampleSpacing > 0)
		{
			SplineSamplePoints.Reserve(1 + SplineLength / Params.InteriorBorderSampleSpacing);

			// Get sample points along the spline that are higher resolution than our PolyLine
			for (FVector::FReal Length = 0.f; Length < SplineLength; Length += Params.InteriorBorderSampleSpacing)
			{
				SplineSamplePoints.Add(Spline->GetLocationAtDistanceAlongSpline(Length, ESplineCoordinateSpace::Local));
			}
		}

		// Flat polygon representation of our spline points
		TArray<FVector2D> SplineSamplePoints2D;
		SplineSamplePoints2D.Reserve(SplineSamplePoints.Num());

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
				PolygonPoints.Reserve(SplineSamplePoints.Num());
				for (const FVector& SplinePoint : SplineSamplePoints)
				{
					FVector& PolygonPoint = PolygonPoints.Add_GetRef(SplinePoint);
					PolygonPoint.Z = MinPoint.Z;
				}
			}
			else
			{
				const int NumSegments = LineData->GetNumSegments();
				PolygonPoints.Reserve(NumSegments);

				for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
				{
					FVector& PolygonPoint = PolygonPoints.Add_GetRef(LineData->GetLocationAtDistance(SegmentIndex, 0, /*bWorldSpace=*/false));
					PolygonPoint.Z = MinPoint.Z;
				}
			}

			TArray<FVector2D> PolygonPoints2D;
			PolygonPoints2D.Reserve(PolygonPoints.Num());

			for (const FVector& Point : PolygonPoints)
			{
				PolygonPoints2D.Add(FVector2D(Point));
			}

			TArray<TTuple<FVector, FVector>> VoronoiEdges;
			TArray<int32> CellMember;

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSplineSamplerElement::Execute::GetVoronoiEdges);
				GetVoronoiEdges(PolygonPoints, FBox(MinPoint, FVector(MaxPoint.X, MaxPoint.Y, MinPoint.Z)), VoronoiEdges, CellMember);
			}

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

			if (MedialAxisEdges.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, LOCTEXT("MedialAxisFailed", "Failed to compute medial axis in interior region, density fall-off will not be applied. This functionality requires a closed spline with at least 4 spline points."));
			}
		}

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();

		// RayPadding should be large enough to avoid potential misclassifications in SegmentPolygonIntersection2D
		const FVector::FReal RayPadding = 100.f;
		const FVector::FReal MinY = FMath::CeilToDouble(MinPoint.Y / Params.InteriorSampleSpacing) * Params.InteriorSampleSpacing;
		const FVector::FReal MaxY = FMath::FloorToDouble(MaxPoint.Y / Params.InteriorSampleSpacing) * Params.InteriorSampleSpacing;

		constexpr int32 MinIterationPerDispatch = 4;
		const int32 NumIterations = (MaxY + UE_KINDA_SMALL_NUMBER - MinY) / Params.InteriorSampleSpacing;
		const int32 NumDispatch = FMath::Max(1, FMath::Min(Context->AsyncState.NumAvailableTasks, NumIterations / MinIterationPerDispatch));
		const int32 NumIterationsPerDispatch = NumIterations / NumDispatch;

		TArray<TArray<TTuple<FTransform, FVector, float>>> InteriorSplinePointData;
		InteriorSplinePointData.SetNum(NumDispatch);

		const FTransform LineDataTransform = LineData->GetTransform();

		ParallelFor(NumDispatch, [&](int32 DispatchIndex)
		{
			const bool bIsLastIteration = (DispatchIndex == (NumDispatch - 1));
			const int32 StartIterationIndex = DispatchIndex * NumIterationsPerDispatch;
			const int32 EndIterationIndex = bIsLastIteration ? NumIterations : (StartIterationIndex + NumIterationsPerDispatch);

			const FVector::FReal LocalMinY = MinY + StartIterationIndex * Params.InteriorSampleSpacing;
			const FVector::FReal LocalMaxY = bIsLastIteration ? (MaxY + UE_KINDA_SMALL_NUMBER) : (MinY + EndIterationIndex * Params.InteriorSampleSpacing);

			TArray<FVector::FReal> DistancesSquaredOnXY;

			// Point sampling
			for(FVector::FReal Y = LocalMinY; Y < LocalMaxY; Y += Params.InteriorSampleSpacing)
			{
				const FVector2D RayMin(MinPoint.X - RayPadding, Y);
				const FVector2D RayMax(MaxPoint.X + RayPadding, Y);

				// Get the intersections along this ray, sorted by Y value
				TArray<FVector2D> Intersections;
				PCGSplineSamplerHelpers::SegmentPolygonIntersection2D(RayMin, RayMax, SplineSamplePoints2D, Intersections);

				if (Intersections.Num() % 2 != 0)
				{
					PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("IntersectionTestFailed", "Intersection test failed, skipping samples for this row"));
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

					InteriorSplinePointData[DispatchIndex].Reserve(2 + FMath::Max(MaxX - MinX, 0) / Params.InteriorSampleSpacing);

					for (FVector::FReal X = MinX; X < MaxX + UE_KINDA_SMALL_NUMBER; X += Params.InteriorSampleSpacing)
					{
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
						float Dummy;
						const float NearestSplineKey = bFindNearestSplineKey ? Spline->SplineCurves.Position.InaccurateFindNearest(SurfaceLocation, Dummy) : 0.f;

						const FVector PointLocationLS = (Params.bProjectOntoSurface ? FVector(SurfaceLocation) : FVector(SampleLocation, MinPoint.Z));

						FTransform TransformLS = FTransform::Identity;
						TransformLS.SetLocation(PointLocationLS);

						if (Params.InteriorOrientation == EPCGSplineSamplingInteriorOrientation::FollowCurvature)
						{
							TransformLS.SetRotation(Spline->GetQuaternionAtSplineInputKey(NearestSplineKey, ESplineCoordinateSpace::Local));
						}

						// Calculate density fall off
						float Density = 1.0f;

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
								const FVector NearestSplineLocation = Spline->GetLocationAtSplineInputKey(NearestSplineKey, ESplineCoordinateSpace::Local);
								PointToSplineDist = FVector2D::Distance(SampleLocation, FVector2D(NearestSplineLocation.X, NearestSplineLocation.Y));
							}

							// Linear fall off in the range [0, 1]
							const FVector::FReal T = SmallestDist / (SmallestDist + PointToSplineDist + UE_KINDA_SMALL_NUMBER);
							Density = DensityFalloffCurve->Eval(T);
						}

						FTransform TransformWS = TransformLS * LineDataTransform;
						if (InProjectionTarget)
						{
							FPCGPoint ProjectedPoint;
							if (!InProjectionTarget->ProjectPoint(TransformWS, FBox(BoundsMin, BoundsMax), InProjectionParams, ProjectedPoint, nullptr))
							{
								continue;
							}

							TransformWS.SetLocation(ProjectedPoint.Transform.GetLocation());
						}

						InteriorSplinePointData[DispatchIndex].Emplace(TransformWS, PointLocationLS, Density);
					}
				}
			}
		});

		// Finally, gather the data and push to the points
		int32 PointCount = 0;
		for (const TArray<TTuple<FTransform, FVector, float>>& InteriorData : InteriorSplinePointData)
		{
			PointCount += InteriorData.Num();
		}

		// TODO: should we parallel for this too?
		OutPoints.Reserve(PointCount);
		for (const TArray<TTuple<FTransform, FVector, float>>& InteriorData : InteriorSplinePointData)
		{
			for (const TTuple<FTransform, FVector, float>& InteriorPoint : InteriorData)
			{
				FPCGPoint& Point = OutPoints.Emplace_GetRef();
				Point.Transform = InteriorPoint.Get<0>();
				SetSeed(Point, InteriorPoint.Get<1>(), Params);
				Point.Density = InteriorPoint.Get<2>();
				Point.BoundsMin = BoundsMin;
				Point.BoundsMax = BoundsMax;
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

#if WITH_EDITOR
FText UPCGSplineSamplerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("SplineSamplerNodeTooltip", "Generates points along the given Spline, and within the Bounding Shape if provided.");
}
#endif

TArray<FPCGPinProperties> UPCGSplineSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGSplineSamplerConstants::SplineLabel, EPCGDataType::PolyLine, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true);
	// Only one connection allowed, user can union multiple shapes.
	PinProperties.Emplace(PCGSplineSamplerConstants::BoundingShapeLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/false, /*bAllowMultipleData=*/false, LOCTEXT("SplineSamplerBoundingShapePinTooltip",
		"Optional. All sampled points must be contained within this shape."
	));

	return PinProperties;
}

FPCGElementPtr UPCGSplineSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGSplineSamplerElement>();
}

bool FPCGSplineSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSplineSamplerElement::Execute);

	const UPCGSplineSamplerSettings* Settings = Context->GetInputSettings<UPCGSplineSamplerSettings>();
	check(Settings);

	TArray<FPCGTaggedData> SplineInputs = Context->InputData.GetInputsByPin(PCGSplineSamplerConstants::SplineLabel);
	TArray<FPCGTaggedData> BoundingShapeInputs = Context->InputData.GetInputsByPin(PCGSplineSamplerConstants::BoundingShapeLabel);

	// Grab the Bounding Shape input if there is one.
	// TODO: Once we support time-slicing, put this in the context and root (see FPCGSurfaceSamplerContext)
	bool bUnionCreated = false;
	const UPCGSpatialData* BoundingShape = Context->InputData.GetSpatialUnionOfInputsByPin(PCGSplineSamplerConstants::BoundingShapeLabel, bUnionCreated);
	if (!BoundingShape && BoundingShapeInputs.Num() > 0)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("BoundingShapeMissing", "Bounding Shape input is missing or of unsupported type and will not be used"));
	}

	const FPCGSplineSamplerParams& SamplerParams = Settings->SamplerParams;

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (FPCGTaggedData& Input : SplineInputs)
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

		const UPCGSpatialData* ProjectionTarget = nullptr;
		FPCGProjectionParams ProjectionParams;
		if (const UPCGSplineProjectionData* SplineProjection = Cast<const UPCGSplineProjectionData>(Input.Data))
		{
			ProjectionTarget = SplineProjection->GetSurface();
			ProjectionParams = SplineProjection->GetProjectionParams();
		}

		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output = Input;

		UPCGPointData* SampledPointData = NewObject<UPCGPointData>();
		SampledPointData->InitializeFromData(SpatialData);
		Output.Data = SampledPointData;

		if (SamplerParams.Dimension == EPCGSplineSamplingDimension::OnInterior)
		{
			PCGSplineSampler::SampleInteriorData(Context, LineData, BoundingShape, ProjectionTarget, ProjectionParams, SamplerParams, SampledPointData);
		}
		else
		{
			PCGSplineSampler::SampleLineData(LineData, BoundingShape, ProjectionTarget, ProjectionParams, SamplerParams, SampledPointData);
		}
	}

	return true;
}

#if WITH_EDITOR
void UPCGSplineSamplerSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	if (DataVersion < FPCGCustomVersion::SplineSamplerUpdatedNodeInputs && ensure(InOutNode))
	{
		if (InputPins.Num() > 0 && InputPins[0])
		{
			// First pin renamed in this version. Rename here so that edges won't get culled in UpdatePins later.
			InputPins[0]->Properties.Label = PCGSplineSamplerConstants::SplineLabel;
		}
	}

	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
