// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSpatialData.h"
#include "Data/PCGDifferenceData.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGProjectionData.h"
#include "Data/PCGUnionData.h"

UPCGSpatialData::UPCGSpatialData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Metadata = ObjectInitializer.CreateDefaultSubobject<UPCGMetadata>(this, TEXT("Metadata"));
}

const UPCGPointData* UPCGSpatialDataWithPointCache::ToPointData(FPCGContext* Context, const FBox& InBounds) const
{
	if (InBounds.IsValid && SupportsBoundedPointData())
	{
		const UPCGPointData* BoundedPointData = nullptr;
		CacheLock.Lock();
		check(CachedBoundedPointDataBoxes.Num() == CachedBoundedPointData.Num());
		for (int CachedDataIndex = 0; CachedDataIndex < CachedBoundedPointDataBoxes.Num(); ++CachedDataIndex)
		{
			if (InBounds.Equals(CachedBoundedPointDataBoxes[CachedDataIndex]))
			{
				BoundedPointData = CachedBoundedPointData[CachedDataIndex];
				break;
			}
		}

		if (!BoundedPointData)
		{
			BoundedPointData = CreatePointData(Context, InBounds);
			CachedBoundedPointDataBoxes.Add(InBounds);
			CachedBoundedPointData.Add(BoundedPointData);
		}
		CacheLock.Unlock();

		return BoundedPointData;
	}
	else
	{
		if (!CachedPointData)
		{
			CacheLock.Lock();

			if (!CachedPointData)
			{
				CachedPointData = CreatePointData(Context, InBounds);
			}

			CacheLock.Unlock();
		}

		return CachedPointData;
	}
}

float UPCGSpatialData::GetDensityAtPosition(const FVector& InPosition) const
{
	FPCGPoint TemporaryPoint;
	if (SamplePoint(FTransform(InPosition), FBox::BuildAABB(FVector::ZeroVector, FVector::ZeroVector), TemporaryPoint, nullptr))
	{
		return TemporaryPoint.Density;
	}
	else
	{
		return 0;
	}
}

FVector UPCGSpatialData::TransformPosition(const FVector& InPosition) const
{
	FPCGPoint TemporaryPoint;
	if (SamplePoint(FTransform(InPosition), FBox::BuildAABB(FVector::ZeroVector, FVector::ZeroVector), TemporaryPoint, nullptr))
	{
		return TemporaryPoint.Transform.GetLocation();
	}
	else
	{
		return InPosition;
	}
}

UPCGIntersectionData* UPCGSpatialData::IntersectWith(const UPCGSpatialData* InOther) const
{
	UPCGIntersectionData* IntersectionData = NewObject<UPCGIntersectionData>(const_cast<UPCGSpatialData*>(this));
	IntersectionData->Initialize(this, InOther);

	return IntersectionData;
}

UPCGProjectionData* UPCGSpatialData::ProjectOn(const UPCGSpatialData* InOther) const
{
	UPCGProjectionData* ProjectionData = NewObject<UPCGProjectionData>();
	ProjectionData->Initialize(this, InOther);

	return ProjectionData;
}

UPCGUnionData* UPCGSpatialData::UnionWith(const UPCGSpatialData* InOther) const
{
	UPCGUnionData* UnionData = NewObject<UPCGUnionData>(const_cast<UPCGSpatialData*>(this));
	UnionData->Initialize(this, InOther);

	return UnionData;
}

UPCGDifferenceData* UPCGSpatialData::Subtract(const UPCGSpatialData* InOther) const
{
	UPCGDifferenceData* DifferenceData = NewObject<UPCGDifferenceData>(const_cast<UPCGSpatialData*>(this));
	DifferenceData->Initialize(this);
	DifferenceData->AddDifference(InOther);

	return DifferenceData;
}

UPCGMetadata* UPCGSpatialData::CreateEmptyMetadata()
{
	if (Metadata)
	{
		UE_LOG(LogPCG, Warning, TEXT("Spatial data already had metadata"));
	}

	Metadata = NewObject<UPCGMetadata>(this);
	return Metadata;
}

void UPCGSpatialData::InitializeFromData(const UPCGSpatialData* InSource, const UPCGMetadata* InMetadataParentOverride, bool bInheritMetadata)
{
	if (InSource && TargetActor.IsExplicitlyNull())
	{
		TargetActor = InSource->TargetActor;
	}

	if (!Metadata)
	{
		Metadata = NewObject<UPCGMetadata>(this);
	}

	if (!bInheritMetadata || InMetadataParentOverride || InSource)
	{
		const UPCGMetadata* ParentMetadata = bInheritMetadata ? (InMetadataParentOverride ? InMetadataParentOverride : InSource->Metadata) : nullptr;

		Metadata->Initialize(ParentMetadata);
	}
	else
	{
		UE_LOG(LogPCG, Warning, TEXT("InitializeFromData has both no source and no metadata override"));
	}
}