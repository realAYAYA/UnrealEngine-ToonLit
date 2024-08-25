// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGVolumeData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGVolumeSampler.h"
#include "Helpers/PCGHelpers.h"

#include "Components/BrushComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Volume.h"

#include "Chaos/ChaosEngineInterface.h"
#include "Physics/PhysicsInterfaceDeclares.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGVolumeData)

UPCGVolumeData::~UPCGVolumeData()
{
	ReleaseInternalBodyInstance();
}

void UPCGVolumeData::ReleaseInternalBodyInstance()
{
	if (VolumeBodyInstance)
	{
		if (VolumeBodyInstance->IsValidBodyInstance())
		{
			VolumeBodyInstance->TermBody();
		}

		delete VolumeBodyInstance;
		VolumeBodyInstance = nullptr;
	}
}

void UPCGVolumeData::Initialize(AVolume* InVolume)
{
	check(InVolume);
	Volume = InVolume;
	
	if (PCGHelpers::IsRuntimeOrPIE())
	{
		const UBrushComponent* Brush = Volume->GetBrushComponent();
		if (Brush && Brush->BodyInstance.GetCollisionProfileName() == UCollisionProfile::NoCollision_ProfileName)
		{
			UE_LOG(LogPCG, Warning, TEXT("Volume Data points to a Brush Component which is set to NoCollision and may not function outside of editor."));
		}
	}

	FBoxSphereBounds BoxSphereBounds = Volume->GetBounds();
	// TODO: Compute the strict bounds, we must find a FBox inscribed into the oriented box.
	// Currently, we'll leave the strict bounds empty and fall back to checking against the local box
	Bounds = FBox::BuildAABB(BoxSphereBounds.Origin, BoxSphereBounds.BoxExtent);
	
	// Keep a "sceneless" equivalent body so we can do queries against it without locking constraints
	if (UBrushComponent* BrushComponent = Volume->GetBrushComponent())
	{
		FBodyInstance* BodyInstance = BrushComponent->GetBodyInstance();
		UBodySetup* BodySetup = BrushComponent->GetBodySetup();

		// In some instances, non-collidable bodies will not be initialized, but it's not an issue for PCG so we can continue regardless.
		// Otherwise, require that the body is not dynamic.
		if (BodyInstance && BodySetup && (!FPhysicsInterface::IsValid(BodyInstance->ActorHandle) || !BodyInstance->IsDynamic()))
		{
			ReleaseInternalBodyInstance();

			VolumeBodyInstance = new FBodyInstance();
			VolumeBodyInstance->bAutoWeld = false;
			VolumeBodyInstance->bSimulatePhysics = false;
			VolumeBodyInstance->InitBody(BodySetup, BrushComponent->GetComponentTransform(), nullptr, nullptr);
		}
	}
}

void UPCGVolumeData::Initialize(const FBox& InBounds)
{
	Bounds = InBounds;
	StrictBounds = InBounds;
}

void UPCGVolumeData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use a global unique data CRC.
	AddUIDToCrc(Ar);
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

	PCGVolumeSampler::FVolumeSamplerParams SamplerParams;
	SamplerParams.VoxelSize = VoxelSize;
	SamplerParams.Bounds = GetBounds();

	const UPCGPointData* Data = PCGVolumeSampler::SampleVolume(Context, SamplerParams, this);

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
		else if (VolumeBodyInstance)
		{
			float OutDistanceSquared = -1.0f;
			if (FPhysicsInterface::GetSquaredDistanceToBody(VolumeBodyInstance, InPosition, OutDistanceSquared))
			{
				PointDensity = (OutDistanceSquared == 0.0f ? 1.0f : 0.0f);
			}
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
