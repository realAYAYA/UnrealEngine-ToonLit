// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGUnionData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadataAccessor.h"

#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGUnionData)

namespace PCGUnionDataMaths
{
	float ComputeDensity(float InDensityToUpdate, float InOtherDensity, EPCGUnionDensityFunction DensityFunction)
	{
		if (DensityFunction == EPCGUnionDensityFunction::ClampedAddition)
		{
			return FMath::Min(InDensityToUpdate + InOtherDensity, 1.0f);
		}
		else if (DensityFunction == EPCGUnionDensityFunction::Binary)
		{
			return (InOtherDensity > 0) ? 1.0f : InDensityToUpdate;
		}
		else // Maximum
		{
			return FMath::Max(InDensityToUpdate, InOtherDensity);
		}
	}

	float UpdateDensity(float& InDensityToUpdate, float InOtherDensity, EPCGUnionDensityFunction DensityFunction)
	{
		InDensityToUpdate = ComputeDensity(InDensityToUpdate, InOtherDensity, DensityFunction);
		return InDensityToUpdate;
	}
}

void UPCGUnionData::Initialize(const UPCGSpatialData* InA, const UPCGSpatialData* InB)
{
	check(InA && InB);
	AddData(InA);
	AddData(InB);
}

void UPCGUnionData::AddData(const UPCGSpatialData* InData)
{
	check(InData);
	check(Metadata);

	Data.Add(InData);

	if (Data.Num() == 1)
	{
		TargetActor = InData->TargetActor;
		CachedBounds = InData->GetBounds();
		CachedStrictBounds = InData->GetStrictBounds();
		CachedDimension = InData->GetDimension();
		Metadata->Initialize(InData->Metadata);
	}
	else
	{
		CachedBounds += InData->GetBounds();
		CachedStrictBounds = PCGHelpers::OverlapBounds(CachedStrictBounds, InData->GetStrictBounds());
		CachedDimension = FMath::Max(CachedDimension, InData->GetDimension());
		Metadata->AddAttributes(InData->Metadata);
	}

	if (!FirstNonTrivialTransformData && InData->HasNonTrivialTransform())
	{
		FirstNonTrivialTransformData = InData;
	}
}

void UPCGUnionData::VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const
{
	for (const TObjectPtr<const UPCGSpatialData>& Datum : Data)
	{
		if (Datum)
		{
			Datum->VisitDataNetwork(Action);
		}
	}
}

FPCGCrc UPCGUnionData::ComputeCrc(bool bFullDataCrc) const
{
	FArchiveCrc32 Ar;

	if (PropagateCrcThroughBooleanData())
	{
		AddToCrc(Ar, bFullDataCrc);

		// Chain together CRCs of operands
		int32 NumOperands = Data.Num();
		Ar << NumOperands;

		for (const TObjectPtr<const UPCGSpatialData>& Datum : Data)
		{
			if (Datum)
			{
				uint32 DatumCrc = Datum->GetOrComputeCrc(bFullDataCrc).GetValue();
				Ar << DatumCrc;
			}
		}
	}
	else
	{
		AddUIDToCrc(Ar);
	}

	return FPCGCrc(Ar.GetCrc());
}

void UPCGUnionData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	uint32 UniqueTypeID = StaticClass()->GetDefaultObject()->GetUniqueID();
	Ar << UniqueTypeID;

	uint32 UnionTypeValue = static_cast<uint32>(UnionType);
	Ar << UnionTypeValue;

	uint32 DensityFunctionValue = static_cast<uint32>(DensityFunction);
	Ar << DensityFunctionValue;
}

int UPCGUnionData::GetDimension() const
{
	return CachedDimension;
}

FBox UPCGUnionData::GetBounds() const
{
	return CachedBounds;
}

FBox UPCGUnionData::GetStrictBounds() const
{
	return CachedStrictBounds;
}

bool UPCGUnionData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGUnionData::SamplePoint);
	FTransform PointTransform = InTransform;
	bool bHasSetPoint = false;

	if (FirstNonTrivialTransformData)
	{
		if (FirstNonTrivialTransformData->SamplePoint(InTransform, InBounds, OutPoint, OutMetadata))
		{
			PointTransform = OutPoint.Transform;
			bHasSetPoint = true;

			if (DensityFunction == EPCGUnionDensityFunction::Binary && OutPoint.Density > 0)
			{
				OutPoint.Density = 1.0f;
			}
		}
	}

	TArray<const UPCGSpatialData*> DataRawPtr = Data;

	const bool bSkipLoop = (bHasSetPoint && !OutMetadata && OutPoint.Density >= 1.0f);
	const int32 DataCount = DataRawPtr.Num();
	for (int32 DataIndex = 0; DataIndex < DataCount && !bSkipLoop; ++DataIndex)
	{
		if (DataRawPtr[DataIndex] == FirstNonTrivialTransformData)
		{
			continue;
		}

		FPCGPoint PointInData;
		if(DataRawPtr[DataIndex]->SamplePoint(PointTransform, InBounds, PointInData, OutMetadata))
		{
			if (!bHasSetPoint)
			{
				OutPoint = PointInData;
				bHasSetPoint = true;
			}
			else
			{
				// Update density
				PCGUnionDataMaths::UpdateDensity(OutPoint.Density, PointInData.Density, DensityFunction);

				OutPoint.Color = FVector4(
					FMath::Max(OutPoint.Color.X, PointInData.Color.X),
					FMath::Max(OutPoint.Color.Y, PointInData.Color.Y),
					FMath::Max(OutPoint.Color.Z, PointInData.Color.Z),
					FMath::Max(OutPoint.Color.W, PointInData.Color.W));

				// Merge properties into OutPoint
				if (OutMetadata)
				{
					if (OutPoint.MetadataEntry != PCGInvalidEntryKey && PointInData.MetadataEntry != PCGInvalidEntryKey)
					{
						OutMetadata->MergePointAttributesSubset(OutPoint, OutMetadata, OutMetadata, PointInData, OutMetadata, DataRawPtr[DataIndex]->Metadata, OutPoint, EPCGMetadataOp::Max);
					}
					else if (PointInData.MetadataEntry != PCGInvalidEntryKey)
					{
						OutPoint.MetadataEntry = PointInData.MetadataEntry;
					}
				}
			}
			
			if (bHasSetPoint && !OutMetadata && OutPoint.Density >= 1.0f)
			{
				break;
			}
		}
	}

	return (bHasSetPoint && OutPoint.Density > 0);
}

bool UPCGUnionData::HasNonTrivialTransform() const
{
	return (FirstNonTrivialTransformData != nullptr || Super::HasNonTrivialTransform());
}

const UPCGSpatialData* UPCGUnionData::FindFirstConcreteShapeFromNetwork() const
{
	// Return first concrete candidate data.
	for (const TObjectPtr<const UPCGSpatialData>& Datum : Data)
	{
		const UPCGSpatialData* Candidate = Datum ? Datum->FindFirstConcreteShapeFromNetwork() : nullptr;
		if (Candidate)
		{
			return Candidate;
		}
	}

	return nullptr;
}

const UPCGPointData* UPCGUnionData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGUnionData::CreatePointData);

	const bool bBinaryDensity = (DensityFunction == EPCGUnionDensityFunction::Binary);

	// Trivial results
	if (Data.Num() == 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid union"));
		return nullptr;
	}
	else if (Data.Num() == 1 && !bBinaryDensity)
	{
		UE_LOG(LogPCG, Verbose, TEXT("Union is trivial"));
		return Data[0]->ToPointData(Context);
	}

	// Cache raw pointers for metadata as these are much faster to use
	TArray<const UPCGSpatialData*> DataRawPtr = Data;
	TArray<const UPCGMetadata*> InputMetadatas;
	InputMetadatas.SetNumUninitialized(Data.Num());
	for (int32 i = 0; i < DataRawPtr.Num(); i++)
	{
		InputMetadatas[i] = DataRawPtr[i]->ToPointData(Context)->Metadata;
	}

	UPCGPointData* PointData = NewObject<UPCGPointData>();
	PointData->InitializeFromData(this, InputMetadatas[0]);

	UPCGMetadata* OutMetadata = PointData->Metadata;
	check(OutMetadata);

	// Initialize metadata
	for (const UPCGMetadata* InputMetadata : InputMetadatas)
	{
		OutMetadata->AddAttributes(InputMetadata);
	}

	switch (UnionType)
	{
	case EPCGUnionType::LeftToRightPriority:
	default:
		CreateSequentialPointData(Context, DataRawPtr, InputMetadatas, PointData, OutMetadata, /*bLeftToRight=*/true);
		break;

	case EPCGUnionType::RightToLeftPriority:
		CreateSequentialPointData(Context, DataRawPtr, InputMetadatas, PointData, OutMetadata, /*bLeftToRight=*/false);
		break;

	case EPCGUnionType::KeepAll:
		{
			TArray<FPCGPoint>& TargetPoints = PointData->GetMutablePoints();
			for(int32 DataIndex = 0; DataIndex < DataRawPtr.Num(); ++DataIndex)
			{
				const UPCGSpatialData* Datum = DataRawPtr[DataIndex];
				const UPCGPointData* DatumPointData = Datum->ToPointData(Context);
				const UPCGMetadata* DatumPointMetadata = DatumPointData->Metadata;

				int32 TargetPointOffset = TargetPoints.Num();
				TargetPoints.Append(DatumPointData->GetPoints());

				if (DataIndex > 0 && DatumPointData->GetPoints().Num() > 0)
				{
					// TODO: could optimize case where there is a common parent between Data 0 and current data, for points that still point to common parent metadata.
					TArrayView<FPCGPoint> TargetPointsSubset = MakeArrayView(&TargetPoints[TargetPointOffset], DatumPointData->GetPoints().Num());
					for (FPCGPoint& Point : TargetPointsSubset)
					{
						Point.MetadataEntry = PCGInvalidEntryKey;
					}

					if (OutMetadata && DatumPointMetadata && DatumPointMetadata->GetAttributeCount() > 0)
					{
						OutMetadata->SetPointAttributes(MakeArrayView(DatumPointData->GetPoints()), DatumPointMetadata, TargetPointsSubset, Context);
					}
				}
			}

			// Correct density for binary-style union
			if (bBinaryDensity)
			{
				for (FPCGPoint& TargetPoint : TargetPoints)
				{
					TargetPoint.Density = ((TargetPoint.Density > 0) ? 1.0f : 0);
				}
			}
		}
		break;
	}

	UE_LOG(LogPCG, Verbose, TEXT("Union generated %d points out of %d data sources"), PointData->GetPoints().Num(), Data.Num());

	return PointData;
}

void UPCGUnionData::CreateSequentialPointData(FPCGContext* Context, TArray<const UPCGSpatialData*>& InputDatas, TArray<const UPCGMetadata*>& InputMetadatas, UPCGPointData* PointData, UPCGMetadata* OutMetadata, bool bLeftToRight) const
{
	check(PointData);

	TArray<FPCGPoint>& TargetPoints = PointData->GetMutablePoints();
	TArray<FPCGPoint> SelectedDataPoints;

	int32 FirstDataIndex = (bLeftToRight ? 0 : InputDatas.Num() - 1);
	int32 LastDataIndex = (bLeftToRight ? InputDatas.Num() : -1);
	int32 DataIndexIncrement = (bLeftToRight ? 1 : -1);

	// Note: this is a O(N^2) implementation. 
	// TODO: It is easy to implement a kind of divide & conquer algorithm here, but it will require some temporary storage.
	for (int32 DataIndex = FirstDataIndex; DataIndex != LastDataIndex; DataIndex += DataIndexIncrement)
	{
		// For each point, if it is not already "processed" by previous data,
		// add it & compute its final density
		const UPCGPointData* CurrentPointData = InputDatas[DataIndex]->ToPointData(Context);
		const TArray<FPCGPoint>& Points = CurrentPointData->GetPoints();

		FPCGAsync::AsyncPointProcessing(Context, Points.Num(), SelectedDataPoints, [this, PointData, OutMetadata, &Points, &InputDatas, &InputMetadatas, DataIndex, FirstDataIndex, LastDataIndex, DataIndexIncrement](int32 Index, FPCGPoint& OutPoint)
		{
			const FPCGPoint& Point = Points[Index];

			// Discard point if it is already covered by a previous data
			bool bPointToExclude = false;
			for (int32 PreviousDataIndex = FirstDataIndex; PreviousDataIndex != DataIndex; PreviousDataIndex += DataIndexIncrement)
			{
				if (InputDatas[PreviousDataIndex]->GetDensityAtPosition(Point.Transform.GetLocation()) != 0)
				{
					bPointToExclude = true;
					break;
				}
			}

			if (bPointToExclude)
			{
				return false;
			}

			check(PointData);
			check(OutMetadata);

			OutPoint = Point;
			if (OutMetadata->GetParent() != InputMetadatas[DataIndex])
			{
				UPCGMetadataAccessorHelpers::InitializeMetadata(OutPoint, OutMetadata);
				// Since we can't inherit from the parent point, we'll set the values directly here
				OutMetadata->SetPointAttributes(Point, InputMetadatas[DataIndex], OutPoint);
			}

			if (DensityFunction == EPCGUnionDensityFunction::Binary && OutPoint.Density > 0)
			{
				OutPoint.Density = 1.0f;
			}

			// Update density & metadata based on current & following data
			for (int32 FollowingDataIndex = DataIndex + DataIndexIncrement; FollowingDataIndex != LastDataIndex; FollowingDataIndex += DataIndexIncrement)
			{
				const UPCGMetadata* FollowingMetadata = InputMetadatas[FollowingDataIndex];

				// If density is saturated and there are no metadata attributes then we can skip this data as it will not contribute.
				if (OutPoint.Density >= 1.0f && (!FollowingMetadata || FollowingMetadata->GetAttributeCount() == 0))
				{
					continue;
				}

				FPCGPoint PointInData;
				if (InputDatas[FollowingDataIndex]->SamplePoint(OutPoint.Transform, OutPoint.GetLocalBounds(), PointInData, OutMetadata))
				{
					// Update density
					PCGUnionDataMaths::UpdateDensity(OutPoint.Density, PointInData.Density, DensityFunction);

					// Update color
					OutPoint.Color = FVector4(
						FMath::Max(OutPoint.Color.X, PointInData.Color.X),
						FMath::Max(OutPoint.Color.Y, PointInData.Color.Y),
						FMath::Max(OutPoint.Color.Z, PointInData.Color.Z),
						FMath::Max(OutPoint.Color.W, PointInData.Color.W));

					if (OutPoint.MetadataEntry != PCGInvalidEntryKey && PointInData.MetadataEntry != PCGInvalidEntryKey)
					{
						OutMetadata->MergePointAttributesSubset(OutPoint, OutMetadata, OutMetadata, PointInData, OutMetadata, FollowingMetadata, OutPoint, EPCGMetadataOp::Max);
					}
					else if (PointInData.MetadataEntry != PCGInvalidEntryKey)
					{
						OutPoint.MetadataEntry = PointInData.MetadataEntry;
					}
				}
			}

			return true;
		});

		// Append current iteration results to target points
		TargetPoints += SelectedDataPoints;
		SelectedDataPoints.Reset();
	}
}

UPCGSpatialData* UPCGUnionData::CopyInternal() const
{
	UPCGUnionData* NewUnionData = NewObject<UPCGUnionData>();

	NewUnionData->Data = Data;
	NewUnionData->FirstNonTrivialTransformData = FirstNonTrivialTransformData;
	NewUnionData->UnionType = UnionType;
	NewUnionData->DensityFunction = DensityFunction;
	NewUnionData->CachedBounds = CachedBounds;
	NewUnionData->CachedStrictBounds = CachedStrictBounds;
	NewUnionData->CachedDimension = CachedDimension;

	return NewUnionData;
}
