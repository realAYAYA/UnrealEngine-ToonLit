// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGCollisionShapeData.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGVolumeSampler.h"

#include "Chaos/GeometryQueries.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Serialization/ArchiveCrc32.h"

void UPCGCollisionShapeData::Initialize(UShapeComponent* InComponent)
{
	check(InComponent && IsSupported(InComponent));
	Shape = InComponent->GetCollisionShape();
	Transform = InComponent->GetComponentTransform();

	//Note: Shape is pre-scaled
	ShapeAdapter = MakeUnique<FPhysicsShapeAdapter>(Transform.GetRotation(), Shape);

	CachedBounds = InComponent->Bounds.GetBox();
	CachedStrictBounds = CachedBounds;
}

bool UPCGCollisionShapeData::IsSupported(UShapeComponent* InComponent)
{
	if (!InComponent)
	{
		return false;
	}

	if(InComponent->IsA<UBoxComponent>() || InComponent->IsA<UCapsuleComponent>() || InComponent->IsA<USphereComponent>())
	{
		return true;
	}

	return false;
}

void UPCGCollisionShapeData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	uint32 UniqueTypeID = StaticClass()->GetDefaultObject()->GetUniqueID();
	Ar << UniqueTypeID;

	Ar << const_cast<FTransform&>(Transform);

	// Shape - only Crc data that is used.
	uint32 ShapeType = static_cast<uint32>(Shape.ShapeType);
	Ar << ShapeType;

	if (Shape.IsSphere())
	{
		Ar << const_cast<float&>(Shape.Sphere.Radius);
	}
	else if (Shape.IsCapsule())
	{
		Ar << const_cast<float&>(Shape.Capsule.Radius);
		Ar << const_cast<float&>(Shape.Capsule.HalfHeight);
	}
	else
	{
		// All other cases - just serialize all three floats
		Ar << const_cast<float&>(Shape.Box.HalfExtentX);
		Ar << const_cast<float&>(Shape.Box.HalfExtentY);
		Ar << const_cast<float&>(Shape.Box.HalfExtentZ);
	}
}

bool UPCGCollisionShapeData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	FCollisionShape CollisionShape;
	CollisionShape.SetBox(FVector3f(InBounds.GetExtent() * InTransform.GetScale3D())); // make sure to prescale
	FPhysicsShapeAdapter PointAdapter(InTransform.GetRotation(), CollisionShape);

	if (Chaos::Utilities::CastHelper(PointAdapter.GetGeometry(), PointAdapter.GetGeomPose(InTransform.GetTranslation()), [this](const auto& Downcast, const auto& FullGeomTransform) { return Chaos::OverlapQuery(ShapeAdapter->GetGeometry(), ShapeAdapter->GetGeomPose(Transform.GetTranslation()), Downcast, FullGeomTransform, /*Thickness=*/0); }))
	{
		new(&OutPoint) FPCGPoint(InTransform, /*Density=*/1.0f, /*Seed=*/0);
		OutPoint.SetLocalBounds(InBounds);
		return true;
	}
	else
	{
		return false;
	}
}

const UPCGPointData* UPCGCollisionShapeData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGShapeData::CreatePointData);

	const PCGVolumeSampler::FVolumeSamplerParams SamplerParams;

	const UPCGPointData* Data = PCGVolumeSampler::SampleVolume(Context, SamplerParams, this);

	if (Data)
	{
		UE_LOG(LogPCG, Verbose, TEXT("Shape extracted %d points"), Data->GetPoints().Num());
	}

	return Data;
}

UPCGSpatialData* UPCGCollisionShapeData::CopyInternal() const
{
	UPCGCollisionShapeData* NewShapeData = NewObject<UPCGCollisionShapeData>();

	NewShapeData->Transform = Transform;
	NewShapeData->Shape = Shape;
	NewShapeData->ShapeAdapter = MakeUnique<FPhysicsShapeAdapter>(NewShapeData->Transform.GetRotation(), NewShapeData->Shape);
	NewShapeData->CachedBounds = CachedBounds;
	NewShapeData->CachedStrictBounds = CachedStrictBounds;

	return NewShapeData;
}
