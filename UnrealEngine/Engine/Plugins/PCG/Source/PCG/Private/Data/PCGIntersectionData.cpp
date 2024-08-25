// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGIntersectionData.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGSpatialDataTpl.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGIntersectionData)

namespace PCGIntersectionDataMaths
{
	float ComputeDensity(float InDensityA, float InDensityB, EPCGIntersectionDensityFunction InDensityFunction)
	{
		if (InDensityFunction == EPCGIntersectionDensityFunction::Minimum)
		{
			return FMath::Min(InDensityA, InDensityB);
		}
		else // default: Multiply
		{
			return InDensityA * InDensityB;
		}
	}
}

void UPCGIntersectionData::Initialize(const UPCGSpatialData* InA, const UPCGSpatialData* InB)
{
	check(InA && InB);
	A = InA;
	B = InB;
	TargetActor = A->TargetActor;

#if WITH_EDITOR
	RawPointerA = A;
	RawPointerB = B;
#endif

	CachedBounds = PCGHelpers::OverlapBounds(GetA()->GetBounds(), GetB()->GetBounds());
	CachedStrictBounds = PCGHelpers::OverlapBounds(GetA()->GetStrictBounds(), GetB()->GetStrictBounds());

	check(Metadata);
	// Note: this should behave the same way as the ToPointData
	if (GetA()->GetDimension() <= GetB()->GetDimension())
	{
		Metadata->Initialize(GetA()->Metadata);
		Metadata->AddAttributes(GetB()->Metadata);
	}
	else
	{
		Metadata->Initialize(GetB()->Metadata);
		Metadata->AddAttributes(GetA()->Metadata);
	}
}

#if WITH_EDITOR
void UPCGIntersectionData::PostLoad()
{
	Super::PostLoad();

	RawPointerA = A;
	RawPointerB = B;
}
#endif

void UPCGIntersectionData::VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const
{
	check(GetA() && GetB());
	GetA()->VisitDataNetwork(Action);
	GetB()->VisitDataNetwork(Action);
}

FPCGCrc UPCGIntersectionData::ComputeCrc(bool bFullDataCrc) const
{
	FArchiveCrc32 Ar;

	if (PropagateCrcThroughBooleanData())
	{
		AddToCrc(Ar, bFullDataCrc);

		// Chain together CRCs of operands
		check(GetA() && GetB());
		uint32 CrcA = GetA()->GetOrComputeCrc(bFullDataCrc).GetValue();
		uint32 CrcB = GetB()->GetOrComputeCrc(bFullDataCrc).GetValue();
		Ar << CrcA;
		Ar << CrcB;
	}
	else
	{
		AddUIDToCrc(Ar);
	}

	return FPCGCrc(Ar.GetCrc());
}

void UPCGIntersectionData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	uint32 UniqueTypeID = StaticClass()->GetDefaultObject()->GetUniqueID();
	Ar << UniqueTypeID;

	uint32 DensityFunctionValue = static_cast<uint32>(DensityFunction);
	Ar << DensityFunctionValue;
}

int UPCGIntersectionData::GetDimension() const
{
	check(GetA() && GetB());
	return FMath::Min(GetA()->GetDimension(), GetB()->GetDimension());
}

FBox UPCGIntersectionData::GetBounds() const
{
	check(GetA() && GetB());
	return CachedBounds;
}

FBox UPCGIntersectionData::GetStrictBounds() const
{
	check(GetA() && GetB());
	return CachedStrictBounds;
}

bool UPCGIntersectionData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGIntersectionData::SamplePoint);
	check(GetA() && GetB());
	const UPCGSpatialData* X = (GetA()->HasNonTrivialTransform() || !GetB()->HasNonTrivialTransform()) ? GetA() : GetB();
	const UPCGSpatialData* Y = (X == GetA()) ? GetB() : GetA();

	FPCGPoint PointFromX;
	if(!X->SamplePoint(InTransform, InBounds, PointFromX, OutMetadata))
	{
		return false;
	}

	FPCGPoint PointFromY;
	if(!Y->SamplePoint(PointFromX.Transform, InBounds, PointFromY, OutMetadata))
	{
		return false;
	}

	// Merge points into a single point
	OutPoint = PointFromY;
	OutPoint.Density = PCGIntersectionDataMaths::ComputeDensity(PointFromX.Density, PointFromY.Density, DensityFunction);
	OutPoint.Color = PointFromX.Color * PointFromY.Color;

	if (OutMetadata)
	{
		if (PointFromX.MetadataEntry != PCGInvalidEntryKey && PointFromY.MetadataEntry != PCGInvalidEntryKey)
		{
			OutMetadata->MergePointAttributesSubset(PointFromX, OutMetadata, X->Metadata, PointFromY, OutMetadata, Y->Metadata, OutPoint, EPCGMetadataOp::Min);
		}
		else if (PointFromX.MetadataEntry != PCGInvalidEntryKey)
		{
			OutPoint.MetadataEntry = PointFromX.MetadataEntry;
		}
		else
		{
			OutPoint.MetadataEntry = PointFromY.MetadataEntry;
		}
	}

	return true;
}

bool UPCGIntersectionData::HasNonTrivialTransform() const
{
	check(GetA() && GetB());
	return GetA()->HasNonTrivialTransform() || GetB()->HasNonTrivialTransform();
}

const UPCGSpatialData* UPCGIntersectionData::FindFirstConcreteShapeFromNetwork() const
{
	check(GetA() && GetB());

	if (const UPCGSpatialData* CandidateA = GetA()->FindFirstConcreteShapeFromNetwork())
	{
		return CandidateA;
	}

	return GetB()->FindFirstConcreteShapeFromNetwork();
}

const UPCGPointData* UPCGIntersectionData::CreatePointData(FPCGContext* Context) const
{
	check(GetA() && GetB());
	// TODO: this is a placeholder;
	// Here we will get the point data from the lower-dimensionality data
	// and then cull out any of the points that are outside the bounds of the other
	if (GetA()->GetDimension() <= GetB()->GetDimension())
	{
		return CreateAndFilterPointData(Context, GetA(), GetB());
	}
	else
	{
		return CreateAndFilterPointData(Context, GetB(), GetA());
	}
}

UPCGPointData* UPCGIntersectionData::CreateAndFilterPointData(FPCGContext* Context, const UPCGSpatialData* X, const UPCGSpatialData* Y) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGIntersectionData::CreateAndFilterPointData);
	check(X && Y);
	check(X->GetDimension() <= Y->GetDimension());

	const UPCGPointData* SourcePointData = X->ToPointData(Context, CachedBounds);

	if (!SourcePointData)
	{
		UE_LOG(LogPCG, Error, TEXT("Intersection unable to get source points"));
		return nullptr;
	}

	const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
	const UPCGMetadata* SourceMetadata = SourcePointData->Metadata;

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this, SourceMetadata);
	Data->Metadata->AddAttributes(Y->Metadata);

	UPCGMetadata* TempYMetadata = nullptr;
	if (Y->Metadata)
	{
		TempYMetadata = Y->Metadata ? NewObject<UPCGMetadata>() : nullptr;
		TempYMetadata->Initialize(Y->Metadata);
	}

	const bool bPointDataHasCommonAttributes = (SourceMetadata && Y->Metadata && SourceMetadata->HasCommonAttributes(Y->Metadata));

	TArray<FPCGPoint>& TargetPoints = Data->GetMutablePoints();

	constexpr int ChunkSize = FPCGSpatialDataProcessing::DefaultSamplePointsChunkSize;

	auto ChunkSamplePoints = [this, Data, SourceMetadata, Y, TempYMetadata, bPointDataHasCommonAttributes](const TArrayView<TPair<FTransform, FBox>>& Samples, const TArrayView<const FPCGPoint>& SourcePoints, TArray<FPCGPoint, TInlineAllocator<ChunkSize>>& OutPoints)
	{
		const int NumPoints = Samples.Num();

		TArray<FPCGPoint, TInlineAllocator<ChunkSize>> PointsFromY;
		PointsFromY.SetNum(NumPoints);

		Y->SamplePoints(Samples, PointsFromY, TempYMetadata);

		TArray<FPCGPoint, TInlineAllocator<ChunkSize>> KeptPoints;
		TArray<FPCGPoint, TInlineAllocator<ChunkSize>> RejectedPoints;

		// Filter points based on output density
		for (int PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			const FPCGPoint& Point = SourcePoints[PointIndex];
			FPCGPoint& PointFromY = PointsFromY[PointIndex];

			if (PointFromY.Density > 0)
			{
				FPCGPoint& KeptPoint = KeptPoints.Add_GetRef(Point); // note: not the sampled point
				KeptPoint.Density = PCGIntersectionDataMaths::ComputeDensity(Point.Density, PointFromY.Density, DensityFunction);
				KeptPoint.Color = Point.Color * PointFromY.Color;

				// TODO: create an array-based MergePointsAttributeSubset..
				// If either the point from Y has metadata or the merge would be a non-trivial value, then perform the full merge
				if (Data->Metadata && (bPointDataHasCommonAttributes || PointFromY.MetadataEntry != PCGInvalidEntryKey))
				{
					Data->Metadata->MergePointAttributesSubset(Point, SourceMetadata, SourceMetadata, PointFromY, TempYMetadata, TempYMetadata, KeptPoint, EPCGMetadataOp::Min);
				}
			}
			else if (bKeepZeroDensityPoints)
			{
				FPCGPoint& RejectedPoint = RejectedPoints.Add_GetRef(Point); // note: not the sampled point
				RejectedPoint.Density = 0;
			}
		}

		OutPoints.Append(KeptPoints);
		OutPoints.Append(RejectedPoints);
	};

	FPCGSpatialDataProcessing::SampleBasedRangeProcessing<ChunkSize>(Context ? &Context->AsyncState : nullptr, ChunkSamplePoints, SourcePoints, TargetPoints);

	UE_LOG(LogPCG, Verbose, TEXT("Intersection generated %d points from %d source points"), TargetPoints.Num(), SourcePoints.Num());

	return Data;
}

UPCGSpatialData* UPCGIntersectionData::CopyInternal() const
{
	UPCGIntersectionData* NewIntersectionData = NewObject<UPCGIntersectionData>();

	NewIntersectionData->DensityFunction = DensityFunction;
	NewIntersectionData->A = A;
	NewIntersectionData->B = B;
	NewIntersectionData->CachedBounds = CachedBounds;
	NewIntersectionData->CachedStrictBounds = CachedStrictBounds;

#if WITH_EDITOR
	NewIntersectionData->RawPointerA = RawPointerA;
	NewIntersectionData->RawPointerB = RawPointerB;
#endif

	return NewIntersectionData;
}
