// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPointData.h"

#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadataAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPointData)

static TAutoConsoleVariable<bool> CVarCacheFullPointDataCrc(
	TEXT("pcg.Cache.FullPointDataCrc"),
	true,
	TEXT("Enable fine-grained CRC of point data for change tracking on elements that request it, rather than using data UID."));

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

	FVector::FReal ManhattanDensity(const FPCGPoint& InPoint, const FVector& InPosition)
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

	FVector::FReal InverseEuclidianDistance(const FPCGPoint& InPoint, const FVector& InPosition)
	{
		FVector Ratios;
		if (GetDistanceRatios(InPoint, InPosition, Ratios))
		{
			return 1.0 - Ratios.Length();
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
	FVector::FReal ComputeOverlapRatio(const FBox& Numerator, const FBox& Denominator)
	{
		const FVector NumeratorExtent = Numerator.GetExtent();
		const FVector DenominatorExtent = Denominator.GetExtent();

		return (FVector::FReal)((DenominatorExtent.X > 0 ? NumeratorExtent.X / DenominatorExtent.X : 1.0) *
			(DenominatorExtent.Y > 0 ? NumeratorExtent.Y / DenominatorExtent.Y : 1.0) *
			(DenominatorExtent.Z > 0 ? NumeratorExtent.Z / DenominatorExtent.Z : 1.0));
	}

	FVector::FReal VolumeOverlap(const FPCGPoint& InPoint, const FBox& InBounds, const FTransform& InTransform)
	{
		// This is similar in idea to SAT considering we have two boxes - since we will test all 6 axes.
		// However, there is some uncertainty due to rotation, and using the overlap value as-is is an overestimation, which might not be critical in this case
		// TODO: investigate if we should do a 8-pt test instead (would be more precise, but significantly more costly).
		const FBox PointBounds = InPoint.GetLocalDensityBounds();
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
		
		return FMath::Min(ComputeOverlapRatio(FirstOverlap, InBounds), ComputeOverlapRatio(SecondOverlap, InBounds));
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
			PCGMetadataEntryKey OutPointEntryKey = OutPoint.MetadataEntry;
			OutPoint = PointToCopy;
			OutPoint.MetadataEntry = OutPointEntryKey;

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

void UPCGPointData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Points.GetAllocatedSize() + Octree.GetSizeBytes() + sizeof(Bounds));
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

void UPCGPointData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	// The code below has non-trivial cost, and can be disabled from console.
	if (!bFullDataCrc || !CVarCacheFullPointDataCrc.GetValueOnAnyThread())
	{
		// Fallback to UID
		Super::AddToCrc(Ar, bFullDataCrc);
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::AddToCrc);

	uint32 UniqueTypeID = StaticClass()->GetDefaultObject()->GetUniqueID();
	Ar << UniqueTypeID;

	if (Points.Num() == 0)
	{
		return;
	}

	// Crc point data.
	{
		// Create copy so we can zero-out the metadata keys which are non-deterministic.
		TArray<FPCGPoint> PointsCopy = Points;
		for (FPCGPoint& Point : PointsCopy)
		{
			Point.MetadataEntry = 0;
		}

		Ar.Serialize(PointsCopy.GetData(), PointsCopy.Num() * PointsCopy.GetTypeSize());
	}

	// Crc metadata.
	if (const UPCGMetadata* PCGMetadata = ConstMetadata())
	{
		FPCGAttributeAccessorKeysPoints AccessorKeys(Points);

		TArray<FName> AttributeNames;
		{
			TArray<EPCGMetadataTypes> AttributeTypes;
			PCGMetadata->GetAttributes(AttributeNames, AttributeTypes);
		}

		// Attribute names might come in different orders for e.g. if edge order changes.
		Algo::Sort(AttributeNames, [this](const FName& A, const FName& B) { return A.LexicalLess(B); });

		for (FName AttributeName : AttributeNames)
		{
			Ar << AttributeName;

			if (const FPCGMetadataAttributeBase* Attribute = PCGMetadata->GetConstAttribute(AttributeName))
			{
				for (const FPCGPoint& Point : Points)
				{
					auto Callback = [Attribute, PCGMetadata, &Ar, &Point](auto ValueWithType)
					{
						using AttributeType = decltype(ValueWithType);

						if (const FPCGMetadataAttribute<AttributeType>* TypedAttribute = static_cast<const FPCGMetadataAttribute<AttributeType>*>(Attribute))
						{
							ValueWithType = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
							Ar << ValueWithType;
						}
					};

					PCGMetadataAttribute::CallbackWithRightType(Attribute->GetTypeId(), Callback);
				}
			}
		}
	}
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

	const FBox LocalBounds = PCGHelpers::GetActorLocalBounds(InActor);
	Points[0].BoundsMin = LocalBounds.Min;
	Points[0].BoundsMax = LocalBounds.Max;

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
	// Run a projection but don't change the point transform. There is a large overlap in code/functionality so this shares one code path.
	FPCGProjectionParams Params{};
	Params.bProjectPositions = Params.bProjectRotations = Params.bProjectScales = Params.bProjectColors = false;

	// The ProjectPoint implementation in this class returns true if the query point is overlapping the point data, which is what SamplePoint should return, so forward the return value.
	return ProjectPoint(InTransform, InBounds, Params, OutPoint, OutMetadata);
}

bool UPCGPointData::ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::SamplePoint);
	if (bOctreeIsDirty)
	{
		RebuildOctree();
	}

	TArray<TPair<const FPCGPoint*, FVector::FReal>, TInlineAllocator<4>> Contributions;
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

	FVector::FReal SumContributions = 0;
	FVector::FReal MaxContribution = 0;
	const FPCGPoint* MaxContributor = nullptr;

	for (const TPair<const FPCGPoint*, FVector::FReal>& Contribution : Contributions)
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

	// Rationale: 
	// When doing volume-to-volume intersection, we want the final density to reflect the amount of overlap
	// if any - hence the volume overlap computation before.
	// But, considering that some points may/will overlap (incl. due to steepness), we want to make sure we do not
	// sum up to more than the total volume. 
	// Note that this might create some artifacts on the edges in some instances, but we will revisit this once we have a
	// better and sufficiently efficient solution.
	const FVector::FReal DensityNormalizationFactor = ((SumContributions > 1.0) ? (1.0 / SumContributions) : 1.0);

	TArray<TPair<const FPCGPoint*, float>, TInlineAllocator<4>> ContributionsForMetadata;

	// Computed weighted average of spatial properties
	FVector WeightedPosition = FVector::ZeroVector;
	FQuat WeightedQuat = FQuat::Identity;
	FVector WeightedScale = FVector::ZeroVector;
	float WeightedDensity = 0;
	FVector WeightedBoundsMin = FVector::ZeroVector;
	FVector WeightedBoundsMax = FVector::ZeroVector;
	FVector4 WeightedColor = FVector4::Zero();
	float WeightedSteepness = 0;

	for (const TPair<const FPCGPoint*, FVector::FReal> Contribution : Contributions)
	{
		const FPCGPoint& SourcePoint = *Contribution.Key;
		const FVector::FReal Weight = Contribution.Value / SumContributions;

		WeightedPosition += SourcePoint.Transform.GetLocation() * Weight;
		WeightedQuat = PCGPointHelpers::AddQuatWithWeight(WeightedQuat, SourcePoint.Transform.GetRotation(), Weight);
		WeightedScale += SourcePoint.Transform.GetScale3D() * Weight;

		if (!bSampleInVolume)
		{
			WeightedDensity += PCGPointHelpers::ManhattanDensity(SourcePoint, InTransform.GetLocation());
		}
		else
		{
			WeightedDensity += SourcePoint.Density * Contribution.Value * DensityNormalizationFactor;
		}

		WeightedBoundsMin += SourcePoint.BoundsMin * Weight;
		WeightedBoundsMax += SourcePoint.BoundsMax * Weight;
		WeightedColor += SourcePoint.Color * Weight;
		WeightedSteepness += SourcePoint.Steepness * Weight;

		ContributionsForMetadata.Emplace(Contribution.Key, static_cast<float>(Weight));
	}

	// Finally, apply changes to point, based on the projection settings
	if (InParams.bProjectPositions)
	{
		OutPoint.Transform.SetLocation(bSampleInVolume ? WeightedPosition : InTransform.GetLocation());
	}
	else
	{
		OutPoint.Transform.SetLocation(InTransform.GetLocation());
	}

	if (InParams.bProjectRotations)
	{
		WeightedQuat.Normalize();
		OutPoint.Transform.SetRotation(WeightedQuat);
	}
	else
	{
		OutPoint.Transform.SetRotation(InTransform.GetRotation());
	}

	if (InParams.bProjectScales)
	{
		OutPoint.Transform.SetScale3D(WeightedScale);
	}
	else
	{
		OutPoint.Transform.SetScale3D(InTransform.GetScale3D());
	}

	OutPoint.Density = WeightedDensity;
	OutPoint.BoundsMin = WeightedBoundsMin;
	OutPoint.BoundsMax = WeightedBoundsMax;
	OutPoint.Color = WeightedColor;
	OutPoint.Steepness = WeightedSteepness;

	if (OutMetadata)
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPointData::SamplePoint::SetupMetadata);
		// Initialise metadata entry for this temporary point
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

UPCGSpatialData* UPCGPointData::CopyInternal() const
{
	UPCGPointData* NewPointData = NewObject<UPCGPointData>();
	NewPointData->GetMutablePoints() = GetPoints();

	return NewPointData;
}
