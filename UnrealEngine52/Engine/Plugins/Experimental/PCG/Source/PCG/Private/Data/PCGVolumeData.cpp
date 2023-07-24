// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGVolumeData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGVolumeSampler.h"
#include "Helpers/PCGHelpers.h"

#include "GameFramework/Volume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGVolumeData)

void UPCGVolumeData::Initialize(AVolume* InVolume, AActor* InTargetActor)
{
	check(InVolume);
	Volume = InVolume;
	TargetActor = InTargetActor ? InTargetActor : InVolume;
	
	FBoxSphereBounds BoxSphereBounds = Volume->GetBounds();
	Bounds = FBox::BuildAABB(BoxSphereBounds.Origin, BoxSphereBounds.BoxExtent);

	// TODO: Compute the strict bounds, we must find a FBox inscribed into the oriented box.
	// Currently, we'll leave the strict bounds empty and fall back to checking against the local box
}

void UPCGVolumeData::Initialize(const FBox& InBounds, AActor* InTargetActor)
{
	Bounds = InBounds;
	StrictBounds = InBounds;
	TargetActor = InTargetActor;
}

FBox UPCGVolumeData::GetBounds() const
{
	return Bounds;
}

FBox UPCGVolumeData::GetStrictBounds() const
{
	return StrictBounds;
}

const UPCGPointData* UPCGVolumeData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGVolumeData::CreatePointData);

	PCGVolumeSampler::FVolumeSamplerSettings SamplerSettings;
	SamplerSettings.VoxelSize = VoxelSize;

	UPCGPointData* Data = PCGVolumeSampler::SampleVolume(Context, this, SamplerSettings);

	if (Data)
	{
		UE_LOG(LogPCG, Verbose, TEXT("Volume extracted %d points"), Data->GetPoints().Num());
	}

	return Data;
}

bool UPCGVolumeData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGVolumeData::SamplePoint);
	// TODO: add metadata
	// TODO: consider bounds

	// This is a pure implementation

	const FVector InPosition = InTransform.GetLocation();
	if (PCGHelpers::IsInsideBounds(GetBounds(), InPosition))
	{
		float PointDensity = 0.0f;

		if (!Volume.IsValid() || PCGHelpers::IsInsideBounds(GetStrictBounds(), InPosition))
		{
			PointDensity = 1.0f;
		}
		else
		{
			PointDensity = Volume->EncompassesPoint(InPosition) ? 1.0f : 0.0f;
		}

		OutPoint.Transform = InTransform;
		OutPoint.SetLocalBounds(InBounds);
		OutPoint.Density = PointDensity;

		return OutPoint.Density > 0;
	}
	else
	{
		return false;
	}
}

void UPCGVolumeData::CopyBaseVolumeData(UPCGVolumeData* NewVolumeData) const
{
	NewVolumeData->VoxelSize = VoxelSize;
	NewVolumeData->Volume = Volume;
	NewVolumeData->Bounds = Bounds;
	NewVolumeData->StrictBounds = StrictBounds;
}

UPCGSpatialData* UPCGVolumeData::CopyInternal() const
{
	UPCGVolumeData* NewVolumeData = NewObject<UPCGVolumeData>();

	CopyBaseVolumeData(NewVolumeData);

	return NewVolumeData;
}
