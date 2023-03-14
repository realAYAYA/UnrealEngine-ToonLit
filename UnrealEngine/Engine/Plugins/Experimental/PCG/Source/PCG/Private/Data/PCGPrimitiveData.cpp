// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPrimitiveData.h"

#include "Data/PCGPointData.h"
#include "Elements/PCGVolumeSampler.h"
#include "Components/PrimitiveComponent.h"

void UPCGPrimitiveData::Initialize(UPrimitiveComponent* InPrimitive)
{
	check(InPrimitive);
	Primitive = InPrimitive;
	TargetActor = InPrimitive->GetOwner();
	CachedBounds = Primitive->Bounds.GetBox();
	// Not obvious to find strict bounds, leave at the default value
}

bool UPCGPrimitiveData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	FCollisionShape CollisionShape;
	CollisionShape.SetBox(FVector3f(InBounds.GetExtent() * InTransform.GetScale3D()));

	const FVector BoxCenter = InTransform.TransformPosition(InBounds.GetCenter());

	if (Primitive->OverlapComponent(BoxCenter, InTransform.GetRotation(), CollisionShape))
	{
		OutPoint.Transform = InTransform;
		OutPoint.SetLocalBounds(InBounds);
		OutPoint.Density = 1.0f;
		return true;
	}
	else
	{
		return false;
	}
}

const UPCGPointData* UPCGPrimitiveData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGPrimitiveData::CreatePointData);

	PCGVolumeSampler::FVolumeSamplerSettings SamplerSettings;
	SamplerSettings.VoxelSize = VoxelSize;

	UPCGPointData* Data = PCGVolumeSampler::SampleVolume(Context, this, SamplerSettings);

	if (Data)
	{
		UE_LOG(LogPCG, Verbose, TEXT("Primitive %s extracted %d points"), *Primitive->GetFName().ToString(), Data->GetPoints().Num());
	}

	return Data;
}