// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGSpatialData.h"
#include "Data/PCGDifferenceData.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGProjectionData.h"
#include "Data/PCGUnionData.h"

#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSpatialData)

UPCGSpatialData::UPCGSpatialData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Metadata = ObjectInitializer.CreateDefaultSubobject<UPCGMetadata>(this, TEXT("Metadata"));
}

void UPCGSpatialDataWithPointCache::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (CachedPointData)
	{
		const_cast<UPCGPointData*>(CachedPointData.Get())->GetResourceSizeEx(CumulativeResourceSize);
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(CachedBoundedPointDataBoxes.GetAllocatedSize() + CachedBoundedPointData.GetAllocatedSize());

	for (const UPCGPointData* Data : CachedBoundedPointData)
	{
		if (Data)
		{
			const_cast<UPCGPointData*>(Data)->GetResourceSizeEx(CumulativeResourceSize);
		}
	}
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

void UPCGSpatialData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (Metadata)
	{
		Metadata->GetResourceSizeEx(CumulativeResourceSize);
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

bool UPCGSpatialData::K2_SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	return SamplePoint(InTransform, InBounds, OutPoint, OutMetadata);
}

bool UPCGSpatialData::K2_ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	return ProjectPoint(InTransform, InBounds, InParams, OutPoint, OutMetadata);
}

void UPCGSpatialData::SamplePoints(const TArrayView<const TPair<FTransform, FBox>>& InSamples, const TArrayView<FPCGPoint>& OutPoints, UPCGMetadata* OutMetadata) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSpatialData::SamplePoints);
	check(InSamples.Num() == OutPoints.Num());
	for(int Index = 0; Index < InSamples.Num(); ++Index)
	{
		const TPair<FTransform, FBox>& Sample = InSamples[Index];
		FPCGPoint& OutPoint = OutPoints[Index];

		if (!SamplePoint(Sample.Key, Sample.Value, OutPoint, OutMetadata))
		{
			OutPoint.Density = 0;
		}
	}
}

bool UPCGSpatialData::ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// Fallback implementation - calls SamplePoint because SamplePoint was being used for projection previously.
	
	// TODO This is a crutch until we implement ProjectPoint everywhere

	const bool bResult = SamplePoint(InTransform, InBounds, OutPoint, OutMetadata);

	// Respect the projection params that we can at this point given our available data (InTransform)

	if (!InParams.bProjectPositions)
	{
		OutPoint.Transform.SetLocation(InTransform.GetLocation());
	}

	if (!InParams.bProjectRotations)
	{
		OutPoint.Transform.SetRotation(InTransform.GetRotation());
	}

	if (!InParams.bProjectScales)
	{
		OutPoint.Transform.SetScale3D(InTransform.GetScale3D());
	}

	return bResult;
}

void UPCGSpatialData::ProjectPoints(const TArrayView<const TPair<FTransform, FBox>>& InSamples, const FPCGProjectionParams& InParams, const TArrayView<FPCGPoint>& OutPoints, UPCGMetadata* OutMetadata) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSpatialData::ProjectPoints);
	check(InSamples.Num() == OutPoints.Num());
	for (int Index = 0; Index < InSamples.Num(); ++Index)
	{
		const TPair<FTransform, FBox>& Sample = InSamples[Index];
		FPCGPoint& OutPoint = OutPoints[Index];

		if (!ProjectPoint(Sample.Key, Sample.Value, InParams, OutPoint, OutMetadata))
		{
			OutPoint.Density = 0;
		}
	}
}

UPCGIntersectionData* UPCGSpatialData::IntersectWith(const UPCGSpatialData* InOther) const
{
	UPCGIntersectionData* IntersectionData = NewObject<UPCGIntersectionData>();
	IntersectionData->Initialize(this, InOther);

	return IntersectionData;
}

UPCGSpatialData* UPCGSpatialData::ProjectOn(const UPCGSpatialData* InOther, const FPCGProjectionParams& InParams) const
{
	// Check necessary conditions. Fail to project -> return copy of projection source, i.e. projection not performed.
	if (!InOther)
	{
		UE_LOG(LogPCG, Warning, TEXT("No projection target specified, no projection will occur"));
		return DuplicateData();
	}

	if (GetDimension() > InOther->GetDimension())
	{
		UE_LOG(LogPCG, Error, TEXT("Dimension of projection source (%d) must be less than or equal to that of the projection target (%d)"), GetDimension(), InOther->GetDimension());
		return DuplicateData();
	}

	const UPCGSpatialData* ConcreteTarget = InOther->FindFirstConcreteShapeFromNetwork();
	if (!ConcreteTarget)
	{
		UE_LOG(LogPCG, Error, TEXT("Could not find a concrete shape in the target data to project onto."));
		return DuplicateData();
	}

	UPCGProjectionData* ProjectionData = NewObject<UPCGProjectionData>();
	ProjectionData->Initialize(this, ConcreteTarget, InParams);

	return ProjectionData;
}

UPCGUnionData* UPCGSpatialData::UnionWith(const UPCGSpatialData* InOther) const
{
	UPCGUnionData* UnionData = NewObject<UPCGUnionData>();
	UnionData->Initialize(this, InOther);

	return UnionData;
}

UPCGDifferenceData* UPCGSpatialData::Subtract(const UPCGSpatialData* InOther) const
{
	UPCGDifferenceData* DifferenceData = NewObject<UPCGDifferenceData>();
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

void UPCGSpatialData::InitializeFromData(const UPCGSpatialData* InSource, const UPCGMetadata* InMetadataParentOverride, bool bInheritMetadata, bool bInheritAttributes)
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
		const UPCGMetadata* ParentMetadata = bInheritMetadata ? (InMetadataParentOverride ? InMetadataParentOverride : (InSource ? InSource->Metadata : nullptr)) : nullptr;
		Metadata->Initialize(ParentMetadata, bInheritAttributes);
	}
	else
	{
		UE_LOG(LogPCG, Warning, TEXT("InitializeFromData has both no source and no metadata override"));
	}
}

UPCGSpatialData* UPCGSpatialData::DuplicateData(bool bInitializeMetadata) const
{
	UPCGSpatialData* NewSpatialData = CopyInternal();
	check(NewSpatialData);

	if (bInitializeMetadata)
	{
		NewSpatialData->InitializeFromData(this);
	}

	if (bHasCachedLastSelector)
	{
		NewSpatialData->SetLastSelector(CachedLastSelector);
	}

	return NewSpatialData;
}

void UPCGSpatialData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	if (Metadata)
	{
		// Can impact results of downstream node execution.
		FName LatestAttribute = Metadata->GetLatestAttributeNameOrNone();
		Ar << LatestAttribute;
	}
}

bool UPCGSpatialData::HasCachedLastSelector() const
{
	return bHasCachedLastSelector || (Metadata && Metadata->GetAttributeCount() > 0);
}

FPCGAttributePropertyInputSelector UPCGSpatialData::GetCachedLastSelector() const
{
	if (bHasCachedLastSelector)
	{
		return CachedLastSelector;
	}

	FPCGAttributePropertyInputSelector TempSelector{};

	// If we have attribute and no last selector, create a cached last selector on the latest attribute, to catch "CreateAttribute" calls that didn't use accessors.
	if (Metadata && Metadata->GetAttributeCount() > 0)
	{
		TempSelector.SetAttributeName(Metadata->GetLatestAttributeNameOrNone());
	}

	return TempSelector;
}

void UPCGSpatialData::SetLastSelector(const FPCGAttributePropertySelector& InSelector)
{
	// Check that it is not a Last or Source selector
	if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute &&
		(InSelector.GetAttributeName() == PCGMetadataAttributeConstants::LastAttributeName
			|| InSelector.GetAttributeName() == PCGMetadataAttributeConstants::LastCreatedAttributeName
			|| InSelector.GetAttributeName() == PCGMetadataAttributeConstants::SourceAttributeName
			|| InSelector.GetAttributeName() == PCGMetadataAttributeConstants::SourceNameAttributeName))
	{
		return;
	}

	bHasCachedLastSelector = true;
	CachedLastSelector.ImportFromOtherSelector(InSelector);
}
