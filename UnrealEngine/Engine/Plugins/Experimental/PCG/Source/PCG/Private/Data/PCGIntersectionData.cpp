// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGIntersectionData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "PCGHelpers.h"
#include "Metadata/PCGMetadataAccessor.h"

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

	CachedBounds = PCGHelpers::OverlapBounds(A->GetBounds(), B->GetBounds());
	CachedStrictBounds = PCGHelpers::OverlapBounds(A->GetStrictBounds(), B->GetStrictBounds());

	check(Metadata);
	// Note: this should behave the same way as the ToPointData
	if (A->GetDimension() <= B->GetDimension())
	{
		Metadata->Initialize(A->Metadata);
		Metadata->AddAttributes(B->Metadata);
	}
	else
	{
		Metadata->Initialize(B->Metadata);
		Metadata->AddAttributes(A->Metadata);
	}
}

int UPCGIntersectionData::GetDimension() const
{
	check(A && B);
	return FMath::Min(A->GetDimension(), B->GetDimension());
}

FBox UPCGIntersectionData::GetBounds() const
{
	check(A && B);
	return CachedBounds;
}

FBox UPCGIntersectionData::GetStrictBounds() const
{
	check(A && B);
	return CachedStrictBounds;
}

bool UPCGIntersectionData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGIntersectionData::SamplePoint);
	check(A && B);
	const UPCGSpatialData* X = (A->HasNonTrivialTransform() || !B->HasNonTrivialTransform()) ? A : B;
	const UPCGSpatialData* Y = (X == A) ? B : A;

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
	check(A && B);
	return A->HasNonTrivialTransform() || B->HasNonTrivialTransform();
}

const UPCGPointData* UPCGIntersectionData::CreatePointData(FPCGContext* Context) const
{
	check(A && B);
	// TODO: this is a placeholder;
	// Here we will get the point data from the lower-dimensionality data
	// and then cull out any of the points that are outside the bounds of the other
	if (A->GetDimension() <= B->GetDimension())
	{
		return CreateAndFilterPointData(Context, A, B);
	}
	else
	{
		return CreateAndFilterPointData(Context, B, A);
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

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this, SourcePointData->Metadata);
	Data->Metadata->AddAttributes(Y->Metadata);

	UPCGMetadata* TempYMetadata = nullptr;
	if (Y->Metadata)
	{
		TempYMetadata = Y->Metadata ? NewObject<UPCGMetadata>() : nullptr;
		TempYMetadata->Initialize(Y->Metadata);
	}

	TArray<FPCGPoint>& TargetPoints = Data->GetMutablePoints();

	FPCGAsync::AsyncPointProcessing(Context, SourcePoints.Num(), TargetPoints, [this, Data, SourcePointData, &SourcePoints, Y, TempYMetadata](int32 Index, FPCGPoint& OutPoint)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGIntersectionData::CreateAndFilterPointData::Iteration);
		const FPCGPoint& Point = SourcePoints[Index];

		FPCGPoint PointFromY;
#if WITH_EDITORONLY_DATA
		if (!Y->SamplePoint(Point.Transform, Point.GetLocalBounds(), PointFromY, TempYMetadata) && !bKeepZeroDensityPoints)
#else
		if (!Y->SamplePoint(Point.Transform, Point.GetLocalBounds(), PointFromY, TempYMetadata))
#endif
		{
			return false;
		}

		OutPoint = Point;
		//UPCGMetadataAccessorHelpers::InitializeMetadata(OutPoint, Data->Metadata, Point);
		OutPoint.Density = PCGIntersectionDataMaths::ComputeDensity(Point.Density, PointFromY.Density, DensityFunction);
		OutPoint.Color = Point.Color * PointFromY.Color;

		if (Data->Metadata)
		{
			if (Point.MetadataEntry != PCGInvalidEntryKey && PointFromY.MetadataEntry != PCGInvalidEntryKey)
			{
				Data->Metadata->MergePointAttributesSubset(Point, SourcePointData->Metadata, SourcePointData->Metadata, PointFromY, TempYMetadata, TempYMetadata, OutPoint, EPCGMetadataOp::Min);
			}
			else if (PointFromY.MetadataEntry != PCGInvalidEntryKey)
			{
				OutPoint.MetadataEntry = PointFromY.MetadataEntry;
			}
		}

		return true;
	});

	UE_LOG(LogPCG, Verbose, TEXT("Intersection generated %d points from %d source points"), TargetPoints.Num(), SourcePoints.Num());

	return Data;
}