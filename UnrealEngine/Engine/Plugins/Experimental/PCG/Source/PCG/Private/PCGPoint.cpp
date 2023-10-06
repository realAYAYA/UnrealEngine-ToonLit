// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPoint.h"

#include "Math/BoxSphereBounds.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"
#include "Metadata/Accessors/IPCGAttributeAccessorTpl.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPoint)

namespace PCGPointCustomPropertyNames
{
	const FName ExtentsName = TEXT("Extents");
	const FName LocalCenterName = TEXT("LocalCenter");
	const FName PositionName = TEXT("Position");
	const FName RotationName = TEXT("Rotation");
	const FName ScaleName = TEXT("Scale");
}

FPCGPoint::FPCGPoint(const FTransform& InTransform, float InDensity, int32 InSeed)
	: Transform(InTransform)
	, Density(InDensity)
	, Seed(InSeed)
{
}

FBox FPCGPoint::GetLocalBounds() const
{
	return FBox(BoundsMin, BoundsMax);
}

FBox FPCGPoint::GetLocalDensityBounds() const
{
	return FBox((2 - Steepness) * BoundsMin, (2 - Steepness) * BoundsMax);
}

void FPCGPoint::SetLocalBounds(const FBox& InBounds)
{
	BoundsMin = InBounds.Min;
	BoundsMax = InBounds.Max;
}

FBoxSphereBounds FPCGPoint::GetDensityBounds() const
{
	return FBoxSphereBounds(GetLocalDensityBounds().TransformBy(Transform));
}

bool FPCGPoint::HasCustomPropertyGetterSetter(FName Name)
{
	return Name == PCGPointCustomPropertyNames::ExtentsName ||
		Name == PCGPointCustomPropertyNames::LocalCenterName ||
		Name == PCGPointCustomPropertyNames::PositionName ||
		Name == PCGPointCustomPropertyNames::RotationName ||
		Name == PCGPointCustomPropertyNames::ScaleName;
}

TUniquePtr<IPCGAttributeAccessor> FPCGPoint::CreateCustomPropertyAccessor(FName Name)
{
	if (Name == PCGPointCustomPropertyNames::ExtentsName)
	{
		return MakeUnique<FPCGCustomPointAccessor<FVector>>(
			[](const FPCGPoint& Point, void* OutValue) { *reinterpret_cast<FVector*>(OutValue) = Point.GetExtents(); return true; },
			[](FPCGPoint& Point, const void* InValue) { Point.SetExtents(*reinterpret_cast<const FVector*>(InValue)); return true; }
		);
	}
	else if (Name == PCGPointCustomPropertyNames::LocalCenterName)
	{
		return MakeUnique<FPCGCustomPointAccessor<FVector>>(
			[](const FPCGPoint& Point, void* OutValue) { *reinterpret_cast<FVector*>(OutValue) = Point.GetLocalCenter(); return true; },
			[](FPCGPoint& Point, const void* InValue) { Point.SetLocalCenter(*reinterpret_cast<const FVector*>(InValue)); return true; }
		);
	}
	else if (Name == PCGPointCustomPropertyNames::PositionName)
	{
		return MakeUnique<FPCGCustomPointAccessor<FVector>>(
			[](const FPCGPoint& Point, void* OutValue) { *reinterpret_cast<FVector*>(OutValue) = Point.Transform.GetLocation(); return true; },
			[](FPCGPoint& Point, const void* InValue) { Point.Transform.SetLocation(*reinterpret_cast<const FVector*>(InValue)); return true; }
		);
	}
	else if (Name == PCGPointCustomPropertyNames::RotationName)
	{
		return MakeUnique<FPCGCustomPointAccessor<FQuat>>(
			[](const FPCGPoint& Point, void* OutValue) { *reinterpret_cast<FQuat*>(OutValue) = Point.Transform.GetRotation(); return true; },
			[](FPCGPoint& Point, const void* InValue) { Point.Transform.SetRotation(*reinterpret_cast<const FQuat*>(InValue)); return true; }
		);
	}
	else if (Name == PCGPointCustomPropertyNames::ScaleName)
	{
		return MakeUnique<FPCGCustomPointAccessor<FVector>>(
			[](const FPCGPoint& Point, void* OutValue) { *reinterpret_cast<FVector*>(OutValue) = Point.Transform.GetScale3D(); return true; },
			[](FPCGPoint& Point, const void* InValue) { Point.Transform.SetScale3D(*reinterpret_cast<const FVector*>(InValue)); return true; }
		);
	}

	return TUniquePtr<IPCGAttributeAccessor>();
}
