// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPointData.h"

#include "PCGHelpers.h"
#include "Metadata/PCGMetadataAccessor.h"

#include "GameFramework/Actor.h"
#include "Misc/ScopeLock.h"

namespace PCGPointHelpers
{
	bool GetDistanceRatios(const FPCGPoint& InPoint, const FVector& InPosition, FVector& OutRatios)
	{
		FVector LocalPosition = InPoint.Transform.InverseTransformPosition(InPosition);
		LocalPosition -= (InPoint.BoundsMax + InPoint.BoundsMin) / 2;
		LocalPosition /= InPoint.GetExtents();

		// ]-2+s, 2-s] is the valid range of values
		const FVector::FReal LowerBound = InPoint.Steepness - 2;
		const FVector::FReal HigherBound = 2 - InPoint.Steepness;

		if (LocalPosition.X <= LowerBound || LocalPosition.X > HigherBound ||
			LocalPosition.Y <= LowerBound || LocalPosition.Y > HigherBound ||
			LocalPosition.Z <= LowerBound || LocalPosition.Z > HigherBound)
		{
			return false;
		}

		// [-s, +s] is the range where the density is 1 on that axis
		const FVector::FReal XDist = FMath::Max(0, FMath::Abs(LocalPosition.X) - InPoint.Steepness);
		const FVector::FReal YDist = FMath::Max(0, FMath::Abs(LocalPosition.Y) - InPoint.Steepness);
		const FVector::FReal ZDist = FMath::Max(0, FMath::Abs(LocalPosition.Z) - InPoint.Steepness);

		const FVector::FReal DistanceScale = FMath::Max(2 - 2 * InPoint.Steepness, KINDA_SMALL_NUMBER);

		OutRatios.X = XDist / DistanceScale;
		OutRatios.Y = YDist / DistanceScale;
		OutRatios.Z = ZDist / DistanceScale;
		return true;
	}

	float ManhattanDensity(const FPCGPoint& InPoint, const FVector& InPosition)
	{
		FVector Ratios;
		if (GetDistanceRatios(InPoint, InPosition, Ratios))
		{
			return InPoint.Density * (1 - Ratios.X) * (1 - Ratios.Y) * (1 - Ratios.Z);
		}
		else
		{
			return 0;
		}
	}

	float InverseEuclidianDistance(const FPCGPoint& InPoint, const FVector& InPosition)
	{
		FVector Ratios;
		if (GetDistanceRatios(InPoint, InPosition, Ratios))
		{
			return 1 - Ratios.Length();
		}
		else
		{
			return 0;
		}
	}

	/** Computes reasonable overlap ratio for point, 1d, 2d and volume overlaps, to be used as weights.
	* Note that this assumes that either data set is homogeneous in its points dimension (either 0d, 1d, 2d, 3d)
	* Otherwise there will be some artifacts from our assumption here (namely using a 1.0 value for the additional coordinates).
	*/
	float ComputeOverlapRatio(const FBox& Numerator, const FBox& Denominator)
	{
		const FVector NumeratorExtent = Numerator.GetExtent();
		const FVector DenominatorExtent = Denominator.GetExtent();

		return (float)((DenominatorExtent.X > 0 ? NumeratorExtent.X / DenominatorExtent.X : 1.0) *
			(DenominatorExtent.Y > 0 ? NumeratorExtent.Y / DenominatorExtent.Y : 1.0) *
			(DenominatorExtent.Z > 0 ? NumeratorExtent.Z / DenominatorExtent.Z : 1.0));
	}

	float VolumeOverlap(const FPCGPoint& InPoint, const FBox& InBounds, const FTransform& InTransform)
	{
		// This is similar in idea to SAT considering we have two boxes - since we will test all 6 axes.
		// However, there is some uncertainty due to rotation, and using the overlap value as-is is an overestimation, which might not be critical in this case
		// TODO: investigate if we should do a 8-pt test instead (would be more precise, but significantly more costly).
		const FBox PointBounds = InPoint.GetLocalBounds();
		const FTransform& PointTransform = InPoint.Transform;

		const FBox FirstOverlap = PointBounds.Overlap(InBounds.TransformBy(InTransform.GetRelativeTransform(PointTransform)));
		if (!FirstOverlap.IsValid)
		{
			return 0;
		}

		const FBox SecondOverlap = InBounds.Overlap(PointBounds.TransformBy(PointTransform.GetRelativeTransform(InTransform)));
		if (!SecondOverlap.IsValid)
		{
			return 0;
		}
		
		return FMath::Min(ComputeOverlapRatio(FirstOverlap, PointBounds), ComputeOverlapRatio(SecondOverlap, InBounds));
	}

	/** Helper function for additive blending of quaternions (copied from ControlRig) */
	FQuat AddQuatWithWeight(const FQuat& Q, const FQuat& V, float Weight)
	{
		FQuat BlendQuat = V * Weight;

		if ((Q | BlendQuat) >= 0.0f)
			return Q + BlendQuat;
		else
			return Q - BlendQuat;
	}

	void Lerp(const FPCGPoint& A, const FPCGPoint& B, float Ratio, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata)
	{
		check(Ratio >= 0 && Ratio <= 1.0f);
		// TODO: this might be incorrect. See UKismetMathLibrary::TLerp instead
		OutPoint.Transform = FTransform(
			FMath::Lerp(A.Transform.GetRotation(), B.Transform.GetRotation(), Ratio),
			FMath::Lerp(A.Transform.GetLocation(), B.Transform.GetLocation(), Ratio),
			FMath::Lerp(A.Transform.GetScale3D(), B.Transform.GetScale3D(), Ratio));
		OutPoint.Density = FMath::Lerp(A.Density, B.Density, Ratio);
		OutPoint.BoundsMin = FMath::Lerp(A.BoundsMin, B.BoundsMin, Ratio);
		OutPoint.BoundsMax = FMath::Lerp(A.BoundsMax, B.BoundsMax, Ratio);
		OutPoint.Color = FMath::Lerp(A.Color, B.Color, Ratio);
		OutPoint.Steepness = FMath::Lerp(A.Steepness, B.Steepness, Ratio);

		if (OutMetadata && SourceMetadata && SourceMetadata->GetAttributeCount() > 0)
		{
			UPCGMetadataAccessorHelpers::InitializeMetadataWithParent(OutPoint, OutMetadata, ((Ratio <= 0.5f) ? A : B), SourceMetadata);

			TArray<TPair<const FPCGPoint*, float>, TInlineAllocator<2>> WeightedPoints;
			WeightedPoints.Emplace(&A, Ratio);
			WeightedPoints.Emplace(&B, 1.0f - Ratio);

			OutMetadata->ComputePointWeightedAttribute(OutPoint, MakeArrayView(WeightedPoints), SourceMetadata);
		}
	}

	void BilerpWithSnapping(const FPCGPoint& X0Y0, const FPCGPoint& X1Y0, const FPCGPoint& X0Y1, const FPCGPoint& X1Y1, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, float XFactor, float YFactor)
	{
		const bool bIsOnLeftEdge = (XFactor < KINDA_SMALL_NUMBER);
		const bool bIsOnRightEdge = (XFactor > 1.0f - KINDA_SMALL_NUMBER);
		const bool bIsOnTopEdge = (YFactor < KINDA_SMALL_NUMBER);
		const bool bIsOnBottomEdge = (YFactor > 1.0f - KINDA_SMALL_NUMBER);

		auto CopyPoint = [&OutPoint, &OutMetadata, &SourceMetadata](const FPCGPoint& PointToCopy)
		{
			OutPoint = PointToCopy;
			if (OutMetadata)
			{
				OutMetadata->SetPointAttributes(PointToCopy, SourceMetadata, OutPoint);
			}
		};

		if (bIsOnLeftEdge || bIsOnRightEdge || bIsOnTopEdge || bIsOnBottomEdge)
		{
			if (bIsOnLeftEdge)
			{
				if (bIsOnTopEdge)
				{
					CopyPoint(X0Y0);
				}
				else if (bIsOnBottomEdge)
				{
					CopyPoint(X0Y1);
				}
				else
				{
					Lerp(X0Y0, X0Y1, YFactor, SourceMetadata, OutPoint, OutMetadata);
				}
			}
			else if (bIsOnRightEdge)
			{
				if (bIsOnTopEdge)
				{
					CopyPoint(X1Y0);
				}
				else if (bIsOnBottomEdge)
				{
					CopyPoint(X1Y1);
				}
				else
				{
					Lerp(X1Y0, X1Y1, YFactor, SourceMetadata, OutPoint, OutMetadata);
				}
			}
			else if (bIsOnTopEdge)
			{
				Lerp(X0Y0, X1Y0, XFactor, SourceMetadata, OutPoint, OutMetadata);
			}
			else // bIsOnBottomEdge
			{
				Lerp(X0Y1, X1Y1, XFactor, SourceMetadata, OutPoint, OutMetadata);
			}
		}
		else
		{
			Bilerp(X0Y0, X1Y0, X0Y1, X1Y1, SourceMetadata, OutPoint, OutMetadata, XFactor, YFactor);
		}
	}

	void Bilerp(const FPCGPoint& X0Y0, const FPCGPoint& X1Y0, const FPCGPoint& X0Y1, const FPCGPoint& X1Y1, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, float XFactor, float YFactor)
	{
		// Interpolate X0Y0-X1Y0 and X0Y1-X1Y1 using XFactor
		FPCGPoint Y0Lerp;
		FPCGPoint Y1Lerp;

		Lerp(X0Y0, X1Y0, XFactor, SourceMetadata, Y0Lerp, OutMetadata);
		Lerp(X0Y1, X1Y1, XFactor, SourceMetadata, Y1Lerp, OutMetadata);
		// Interpolate between the two points using YFactor
		Lerp(Y0Lerp, Y1Lerp, YFactor, SourceMetadata, OutPoint, OutMetadata);
	}
}

FPCGPointRef::FPCGPointRef(const FPCGPoint& InPoint)
{
	Point = &InPoint;
	Bounds = InPoint.GetDensityBounds();
}

FPCGPointRef::FPCGPointRef(const FPCGPointRef& InPointRef)
{
	Point = InPointRef.Point;
	Bounds = InPointRef.Bounds;
}

TArray<FPCGPoint>& UPCGPointData::GetMutablePoints()
{
	bOctreeIsDirty = true;
	bBoundsAreDirty = true;
	return Points;
}

const UPCGPointData::PointOctree& UPCGPointData::GetOctree() const
{
	if (bOctreeIsDirty)
	{
		RebuildOctree();
	}

	return Octree;
}

FBox UPCGPointData::GetBounds() const
{
	if (bBoundsAreDirty)
	{
		RecomputeBounds();
	}

	return Bounds;
}

void UPCGPointData::RecomputeBounds() const
{
	FScopeLock Lock(&CachedDataLock);

	if (!bBoundsAreDirty)
	{
		return;
	}

	FBox NewBounds(EForceInit::ForceInit);
	for (const FPCGPoint& Point : Points)
	{
		FBoxSphereBounds PointBounds = Point.GetDensityBounds();
		NewBounds += FBox::BuildAABB(PointBounds.Origin, PointBounds.BoxExtent);
	}

	Bounds = NewBounds;
	bBoundsAreDirty = false;
}

void UPCGPointData::CopyPointsFrom(const UPCGPointData* InData, const TArray<int>& InDataIndices)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::CopyPointsFrom);
	check(InData);
	Points.SetNum(InDataIndices.Num());

	// TODO: parallel-for this?
	for (int PointIndex = 0; PointIndex < InDataIndices.Num(); ++PointIndex)
	{
		Points[PointIndex] = InData->Points[InDataIndices[PointIndex]];
	}

	bBoundsAreDirty = true;
	bOctreeIsDirty = true;
}

void UPCGPointData::SetPoints(const TArray<FPCGPoint>& InPoints)
{
	GetMutablePoints() = InPoints;
}

void UPCGPointData::InitializeFromActor(AActor* InActor)
{
	check(InActor);

	Points.SetNum(1);
	Points[0].Transform = InActor->GetActorTransform();

	const FVector& Position = Points[0].Transform.GetLocation();
	Points[0].Seed = PCGHelpers::ComputeSeed((int)Position.X, (int)Position.Y, (int)Position.Z);

	TargetActor = InActor;
	Metadata = NewObject<UPCGMetadata>(this);
}

FPCGPoint UPCGPointData::GetPoint(int32 Index) const
{
	if (Points.IsValidIndex(Index))
	{
		return Points[Index];
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid index in GetPoint call"));
		return FPCGPoint();
	}
}

bool UPCGPointData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::SamplePoint);
	if (bOctreeIsDirty)
	{
		RebuildOctree();
	}

	TArray<TPair<const FPCGPoint*, float>, TInlineAllocator<4>> Contributions;
	const bool bSampleInVolume = (InBounds.GetExtent() != FVector::ZeroVector);

	if (!bSampleInVolume)
	{
		const FVector InPosition = InTransform.GetLocation();
		Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(InPosition, FVector::Zero()), [&InPosition, &Contributions](const FPCGPointRef& InPointRef) {
			Contributions.Emplace(InPointRef.Point, PCGPointHelpers::InverseEuclidianDistance(*InPointRef.Point, InPosition));
		});
	}
	else
	{
		FBox TransformedBounds = InBounds.TransformBy(InTransform);
		Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(TransformedBounds.GetCenter(), TransformedBounds.GetExtent()), [&InBounds, &InTransform, &Contributions](const FPCGPointRef& InPointRef) {
			float Contribution = PCGPointHelpers::VolumeOverlap(*InPointRef.Point, InBounds, InTransform);
			if (Contribution > 0)
			{
				Contributions.Emplace(InPointRef.Point, Contribution);
			}
		});
	}

	float SumContributions = 0;
	float MaxContribution = 0;
	const FPCGPoint* MaxContributor = nullptr;

	for (const TPair<const FPCGPoint*, float>& Contribution : Contributions)
	{
		SumContributions += Contribution.Value;

		if (Contribution.Value > MaxContribution)
		{
			MaxContribution = Contribution.Value;
			MaxContributor = Contribution.Key;
		}
	}

	if (SumContributions <= 0)
	{
		return false;
	}

	TArray<TPair<const FPCGPoint*, float>, TInlineAllocator<4>> ContributionsForMetadata;

	// Computed weighted average of spatial properties
	FVector WeightedPosition = FVector::ZeroVector;
	FQuat WeightedQuat = FQuat::Identity;
	FVector WeightedScale = FVector::ZeroVector;
	float WeightedDensity = 0;
	FVector WeightedBoundsMin = FVector::ZeroVector;
	FVector WeightedBoundsMax = FVector::ZeroVector;
	FVector WeightedColor = FVector::ZeroVector;
	float WeightedSteepness = 0;

	for (const TPair<const FPCGPoint*, float> Contribution : Contributions)
	{
		const FPCGPoint& SourcePoint = *Contribution.Key;
		const float Weight = Contribution.Value / SumContributions;

		WeightedPosition += SourcePoint.Transform.GetLocation() * Weight;
		WeightedQuat = PCGPointHelpers::AddQuatWithWeight(WeightedQuat, SourcePoint.Transform.GetRotation(), Weight);
		WeightedScale += SourcePoint.Transform.GetScale3D() * Weight;

		if (!bSampleInVolume)
		{
			WeightedDensity += PCGPointHelpers::ManhattanDensity(SourcePoint, InTransform.GetLocation());
		}
		else
		{
			WeightedDensity += SourcePoint.Density * Weight * Contribution.Value;
		}

		WeightedBoundsMin += SourcePoint.BoundsMin * Weight;
		WeightedBoundsMax += SourcePoint.BoundsMax * Weight;
		WeightedColor += SourcePoint.Color * Weight;
		WeightedSteepness += SourcePoint.Steepness * Weight;

		ContributionsForMetadata.Emplace(Contribution.Key, Weight);
	}

	// Finally, apply changes to point
	WeightedQuat.Normalize();

	OutPoint.Transform.SetRotation(WeightedQuat);
	OutPoint.Transform.SetScale3D(WeightedScale);
	OutPoint.Transform.SetLocation(bSampleInVolume ? WeightedPosition : InTransform.GetLocation());
	OutPoint.Density = WeightedDensity;
	OutPoint.BoundsMin = WeightedBoundsMin;
	OutPoint.BoundsMax = WeightedBoundsMax;
	OutPoint.Color = WeightedColor;
	OutPoint.Steepness = WeightedSteepness;

	if (OutMetadata)
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::SamplePoint::SetupMetadata);
		UPCGMetadataAccessorHelpers::InitializeMetadataWithParent(OutPoint, OutMetadata, *MaxContributor, Metadata);

		if (ContributionsForMetadata.Num() > 1)
		{
			OutMetadata->ComputePointWeightedAttribute(OutPoint, MakeArrayView(ContributionsForMetadata), Metadata);
		}
	}

	return true;
}

void UPCGPointData::RebuildOctree() const
{
	FScopeLock Lock(&CachedDataLock);

	if (!bOctreeIsDirty)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::RebuildOctree)
	check(bOctreeIsDirty);

	FBox PointBounds = GetBounds();
	TOctree2<FPCGPointRef, FPCGPointRefSemantics> NewOctree(PointBounds.GetCenter(), PointBounds.GetExtent().Length());

	for (const FPCGPoint& Point : Points)
	{
		NewOctree.AddElement(FPCGPointRef(Point));
	}

	Octree = NewOctree;
	bOctreeIsDirty = false;
}