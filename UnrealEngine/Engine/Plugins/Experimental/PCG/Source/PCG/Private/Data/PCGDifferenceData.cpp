// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGDifferenceData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGUnionData.h"
#include "Helpers/PCGAsync.h"

#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGDifferenceData)

namespace PCGDifferenceDataUtils
{
	EPCGUnionDensityFunction ToUnionDensityFunction(EPCGDifferenceDensityFunction InDensityFunction)
	{
		if (InDensityFunction == EPCGDifferenceDensityFunction::ClampedSubstraction)
		{
			return EPCGUnionDensityFunction::ClampedAddition;
		}
		else if (InDensityFunction == EPCGDifferenceDensityFunction::Binary)
		{
			return EPCGUnionDensityFunction::Binary;
		}
		else
		{
			return EPCGUnionDensityFunction::Maximum;
		}
	}
}

void UPCGDifferenceData::Initialize(const UPCGSpatialData* InData)
{
	check(InData);
	Source = InData;
	TargetActor = InData->TargetActor;

#if WITH_EDITOR
	RawPointerSource = Source;
#endif

	check(Metadata);
	Metadata->Initialize(Source->Metadata);
}

void UPCGDifferenceData::AddDifference(const UPCGSpatialData* InDifference)
{
	check(InDifference);

	// In the eventuality that the difference has no overlap with the source, then we can drop it directly
	if (!GetBounds().Intersect(InDifference->GetBounds()))
	{
		return;
	}

	// First difference element we'll keep as is, but subsequent ones will be pushed into a union
	if (!Difference)
	{
		Difference = InDifference;

#if WITH_EDITOR
		RawPointerDifference = InDifference;
#endif
	}
	else
	{
		if (!DifferencesUnion)
		{
			DifferencesUnion = NewObject<UPCGUnionData>();
			DifferencesUnion->AddData(Difference);
			DifferencesUnion->SetDensityFunction(PCGDifferenceDataUtils::ToUnionDensityFunction(DensityFunction));
			Difference = DifferencesUnion;

#if WITH_EDITOR
			RawPointerDifference = Difference;
			RawPointerDifferencesUnion = DifferencesUnion;
#endif
		}

		check(Difference == DifferencesUnion);
		DifferencesUnion->AddData(InDifference);
	}
}

void UPCGDifferenceData::SetDensityFunction(EPCGDifferenceDensityFunction InDensityFunction)
{
	DensityFunction = InDensityFunction;

	if (GetDifferencesUnion())
	{
		GetDifferencesUnion()->SetDensityFunction(PCGDifferenceDataUtils::ToUnionDensityFunction(DensityFunction));
	}
}

#if WITH_EDITOR
void UPCGDifferenceData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGDifferenceData, DensityFunction))
	{
		SetDensityFunction(DensityFunction);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPCGDifferenceData::PostLoad()
{
	Super::PostLoad();

	RawPointerSource = Source;
	RawPointerDifference = Difference;
	RawPointerDifferencesUnion = DifferencesUnion;
}
#endif

void UPCGDifferenceData::VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const
{
	check(GetSource());
	GetSource()->VisitDataNetwork(Action);

	if (GetDifference())
	{
		GetDifference()->VisitDataNetwork(Action);
	}
}

FPCGCrc UPCGDifferenceData::ComputeCrc(bool bFullDataCrc) const
{
	FArchiveCrc32 Ar;

	if (PropagateCrcThroughBooleanData())
	{
		AddToCrc(Ar, bFullDataCrc);

		// Chain together CRCs of operands
		check(GetSource());
		uint32 SourceCrc = GetSource()->GetOrComputeCrc(bFullDataCrc).GetValue();
		Ar << SourceCrc;

		if (GetDifference())
		{
			uint32 DifferenceCrc = GetDifference()->GetOrComputeCrc(bFullDataCrc).GetValue();
			Ar << DifferenceCrc;
		}
	}
	else
	{
		UPCGData::AddToCrc(Ar, bFullDataCrc);
	}

	return FPCGCrc(Ar.GetCrc());
}

void UPCGDifferenceData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	uint32 UniqueTypeID = StaticClass()->GetDefaultObject()->GetUniqueID();
	Ar << UniqueTypeID;

	uint32 DensityFunctionValue = static_cast<uint32>(DensityFunction);
	Ar << DensityFunctionValue;
}

int UPCGDifferenceData::GetDimension() const
{
	return GetSource()->GetDimension();
}

FBox UPCGDifferenceData::GetBounds() const
{
	return GetSource()->GetBounds();
}

FBox UPCGDifferenceData::GetStrictBounds() const
{
	return GetDifference() ? FBox(EForceInit::ForceInit) : GetSource()->GetStrictBounds();
}

bool UPCGDifferenceData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDifferenceData::SamplePoint);
	check(GetSource());

	FPCGPoint PointFromSource;
	if(!GetSource()->SamplePoint(InTransform, InBounds, PointFromSource, OutMetadata))
	{
		return false;
	}

	OutPoint = PointFromSource;

	FPCGPoint PointFromDiff;
	// Important note: here we will not use the point we got from the source, otherwise we are introducing severe bias
	if (GetDifference() && GetDifference()->SamplePoint(InTransform, InBounds, PointFromDiff, (bDiffMetadata ? OutMetadata : nullptr)))
	{
		const bool bBinaryDensity = (DensityFunction == EPCGDifferenceDensityFunction::Binary);
		
		// Apply difference
		OutPoint.Density = bBinaryDensity ? 0 : FMath::Max(0, PointFromSource.Density - PointFromDiff.Density);
		// Color?
		if (bDiffMetadata && OutMetadata && OutPoint.Density > 0 && PointFromDiff.MetadataEntry != PCGInvalidEntryKey)
		{
			// Safe to also cache GetSource()->Metadata ? I'm not sure it is, but if it is it could also benefit UnionData which sometimes accesses input metadata, and also intersection data
			OutMetadata->MergePointAttributesSubset(PointFromSource, OutMetadata, GetSource()->Metadata, PointFromDiff, OutMetadata, GetDifference()->Metadata, OutPoint, EPCGMetadataOp::Sub);
		}

		return OutPoint.Density > 0;
	}
	else
	{
		return true;
	}
}

bool UPCGDifferenceData::HasNonTrivialTransform() const
{
	check(GetSource());
	return GetSource()->HasNonTrivialTransform();
}

const UPCGPointData* UPCGDifferenceData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDifferenceData::CreatePointData);
	
	// This is similar to what we are doing in UPCGUnionData::CreatePointData
	const UPCGPointData* SourcePointData = GetSource()->ToPointData(Context);

	if (!SourcePointData)
	{
		UE_LOG(LogPCG, Error, TEXT("Difference unable to get source points"));
		return SourcePointData;
	}

	if (!GetDifference())
	{
		UE_LOG(LogPCG, Verbose, TEXT("Difference is trivial"));
		return SourcePointData;
	}

	const UPCGMetadata* SourceMetadata = SourcePointData->Metadata;

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this, SourceMetadata);
	
	UPCGMetadata* OutMetadata = Data->Metadata;

	const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
	TArray<FPCGPoint>& TargetPoints = Data->GetMutablePoints();

	const UPCGMetadata* DifferenceMetadata = GetDifference()->Metadata;
	UPCGMetadata* TempDiffMetadata = nullptr;
	if (bDiffMetadata && OutMetadata && DifferenceMetadata)
	{
		TempDiffMetadata = NewObject<UPCGMetadata>();
		TempDiffMetadata->Initialize(DifferenceMetadata);
	}

	FPCGAsync::AsyncPointProcessing(Context, SourcePoints.Num(), TargetPoints, [this, Data, OutMetadata, SourcePointData, SourceMetadata, TempDiffMetadata, &SourcePoints](int32 Index, FPCGPoint& OutPoint)
	{
		const FPCGPoint& Point = SourcePoints[Index];

		FPCGPoint PointFromDiff;
		if (GetDifference() && GetDifference()->SamplePoint(Point.Transform, Point.GetLocalBounds(), PointFromDiff, TempDiffMetadata))
		{
			const bool bBinaryDensity = (DensityFunction == EPCGDifferenceDensityFunction::Binary);

			OutPoint = Point;
			OutPoint.Density = bBinaryDensity ? 0 : FMath::Max(0, Point.Density - PointFromDiff.Density);

			if (TempDiffMetadata && OutPoint.Density > 0 && PointFromDiff.MetadataEntry != PCGInvalidEntryKey)
			{
				OutMetadata->MergePointAttributesSubset(Point, SourceMetadata, SourceMetadata, PointFromDiff, TempDiffMetadata, TempDiffMetadata, OutPoint, EPCGMetadataOp::Sub);
			}

#if WITH_EDITOR
			return OutPoint.Density > 0 || bKeepZeroDensityPoints;
#else
			return OutPoint.Density > 0;
#endif
		}
		else
		{
			OutPoint = Point;
			return true;
		}
	});

	UE_LOG(LogPCG, Verbose, TEXT("Difference generated %d points from %d source points"), TargetPoints.Num(), SourcePointData->GetPoints().Num());

	return Data;
}

UPCGSpatialData* UPCGDifferenceData::CopyInternal() const
{
	UPCGDifferenceData* NewDifferenceData = NewObject<UPCGDifferenceData>();

	NewDifferenceData->Source = Source;
	NewDifferenceData->Difference = Difference;
	NewDifferenceData->DensityFunction = DensityFunction;
	if (DifferencesUnion)
	{
		NewDifferenceData->DifferencesUnion = static_cast<UPCGUnionData*>(DifferencesUnion->DuplicateData());

#if WITH_EDITOR
		NewDifferenceData->RawPointerDifferencesUnion = NewDifferenceData->DifferencesUnion;
#endif
	}

#if WITH_EDITOR
	NewDifferenceData->RawPointerSource = NewDifferenceData->Source;
	NewDifferenceData->RawPointerDifference = NewDifferenceData->Difference;
#endif

	return NewDifferenceData;
}
