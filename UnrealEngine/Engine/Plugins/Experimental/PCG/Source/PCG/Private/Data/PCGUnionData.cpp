// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGUnionData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "PCGHelpers.h"

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

	const bool bSkipLoop = (bHasSetPoint && !OutMetadata && OutPoint.Density >= 1.0f);
	const int32 DataCount = Data.Num();
	for (int32 DataIndex = 0; DataIndex < DataCount && !bSkipLoop; ++DataIndex)
	{
		if (Data[DataIndex] == FirstNonTrivialTransformData)
		{
			continue;
		}

		FPCGPoint PointInData;
		if(Data[DataIndex]->SamplePoint(PointTransform, InBounds, PointInData, OutMetadata))
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
						OutMetadata->MergePointAttributesSubset(OutPoint, OutMetadata, OutMetadata, PointInData, OutMetadata, Data[DataIndex]->Metadata, OutPoint, EPCGMetadataOp::Max);
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

	UPCGPointData* PointData = NewObject<UPCGPointData>();
	PointData->InitializeFromData(this, Data[0]->Metadata);

	// Initialize metadata
	for (TObjectPtr<const UPCGSpatialData> Datum : Data)
	{
		PointData->Metadata->AddAttributes(Datum->Metadata);
	}

	switch (UnionType)
	{
	case EPCGUnionType::LeftToRightPriority:
	default:
		CreateSequentialPointData(Context, PointData, /*bLeftToRight=*/true);
		break;

	case EPCGUnionType::RightToLeftPriority:
		CreateSequentialPointData(Context, PointData, /*bLeftToRight=*/false);
		break;

	case EPCGUnionType::KeepAll:
		{
			TArray<FPCGPoint>& TargetPoints = PointData->GetMutablePoints();
			for (TObjectPtr<const UPCGSpatialData> Datum : Data)
			{
				const UPCGPointData* DatumPointData = Datum->ToPointData(Context);
				int32 TargetPointIndex = TargetPoints.Num();
				TargetPoints.Append(DatumPointData->GetPoints());

				if (PointData->Metadata && DatumPointData->GetPoints().Num() > 0)
				{
					PointData->Metadata->SetPointAttributes(MakeArrayView(DatumPointData->GetPoints()), DatumPointData->Metadata, MakeArrayView(&TargetPoints[TargetPointIndex], DatumPointData->GetPoints().Num()));
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

void UPCGUnionData::CreateSequentialPointData(FPCGContext* Context, UPCGPointData* PointData, bool bLeftToRight) const
{
	check(PointData);

	TArray<FPCGPoint>& TargetPoints = PointData->GetMutablePoints();
	TArray<FPCGPoint> SelectedDataPoints;

	int32 FirstDataIndex = (bLeftToRight ? 0 : Data.Num() - 1);
	int32 LastDataIndex = (bLeftToRight ? Data.Num() : -1);
	int32 DataIndexIncrement = (bLeftToRight ? 1 : -1);

	// Note: this is a O(N^2) implementation. 
	// TODO: It is easy to implement a kind of divide & conquer algorithm here, but it will require some temporary storage.
	for (int32 DataIndex = FirstDataIndex; DataIndex != LastDataIndex; DataIndex += DataIndexIncrement)
	{
		// For each point, if it is not already "processed" by previous data,
		// add it & compute its final density
		const TArray<FPCGPoint>& Points = Data[DataIndex]->ToPointData(Context)->GetPoints();

		FPCGAsync::AsyncPointProcessing(Context, Points.Num(), SelectedDataPoints, [this, PointData, &Points, DataIndex, FirstDataIndex, LastDataIndex, DataIndexIncrement](int32 Index, FPCGPoint& OutPoint)
		{
			const FPCGPoint& Point = Points[Index];

			// Discard point if it is already covered by a previous data
			bool bPointToExclude = false;
			for (int32 PreviousDataIndex = FirstDataIndex; PreviousDataIndex != DataIndex; PreviousDataIndex += DataIndexIncrement)
			{
				if (Data[PreviousDataIndex]->GetDensityAtPosition(Point.Transform.GetLocation()) != 0)
				{
					bPointToExclude = true;
					break;
				}
			}

			if (bPointToExclude)
			{
				return false;
			}

			check(PointData && PointData->Metadata);

			OutPoint = Point;
			if (PointData->Metadata->GetParent() == Data[DataIndex]->Metadata)
			{
				UPCGMetadataAccessorHelpers::InitializeMetadata(OutPoint, PointData->Metadata, Point);
			}
			else
			{
				UPCGMetadataAccessorHelpers::InitializeMetadata(OutPoint, PointData->Metadata);
				// Since we can't inherit from the parent point, we'll set the values directly here
				PointData->Metadata->SetPointAttributes(Point, Data[DataIndex]->Metadata, OutPoint);
			}

			if (DensityFunction == EPCGUnionDensityFunction::Binary && OutPoint.Density > 0)
			{
				OutPoint.Density = 1.0f;
			}

			// Update density & metadata based on current & following data
			const bool bSkipLoop = (OutPoint.Density >= 1.0f && !PointData->Metadata);
			for (int32 FollowingDataIndex = DataIndex + DataIndexIncrement; FollowingDataIndex != LastDataIndex && !bSkipLoop; FollowingDataIndex += DataIndexIncrement)
			{
				FPCGPoint PointInData;
				if(Data[FollowingDataIndex]->SamplePoint(OutPoint.Transform, OutPoint.GetLocalBounds(), PointInData, PointData->Metadata))
				{
					// Update density
					PCGUnionDataMaths::UpdateDensity(OutPoint.Density, PointInData.Density, DensityFunction);

					// Update color
					OutPoint.Color = FVector4(
						FMath::Max(OutPoint.Color.X, PointInData.Color.X),
						FMath::Max(OutPoint.Color.Y, PointInData.Color.Y),
						FMath::Max(OutPoint.Color.Z, PointInData.Color.Z),
						FMath::Max(OutPoint.Color.W, PointInData.Color.W));

					if (PointData->Metadata)
					{
						if (OutPoint.MetadataEntry != PCGInvalidEntryKey && PointInData.MetadataEntry != PCGInvalidEntryKey)
						{
							PointData->Metadata->MergePointAttributesSubset(OutPoint, PointData->Metadata, PointData->Metadata, PointInData, PointData->Metadata, Data[FollowingDataIndex]->Metadata, OutPoint, EPCGMetadataOp::Max);
						}
						else if (PointInData.MetadataEntry != PCGInvalidEntryKey)
						{
							OutPoint.MetadataEntry = PointInData.MetadataEntry;
						}
					}
					else if (OutPoint.Density >= 1.0f)
					{
						break;
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