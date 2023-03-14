// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGPoint.generated.h"

UENUM()
enum class EPCGPointProperties : uint8
{
	Density,
	BoundsMin,
	BoundsMax,
	Extents,
	Color,
	Position,
	Rotation,
	Scale,
	Transform,
	Steepness
};

USTRUCT(BlueprintType)
struct PCG_API FPCGPoint
{
	GENERATED_BODY()
public:
	FPCGPoint() = default;
	FPCGPoint(const FTransform& InTransform, float InDensity, int32 InSeed);

	FBox GetLocalBounds() const;
	void SetLocalBounds(const FBox& InBounds);
	FBoxSphereBounds GetDensityBounds() const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	FTransform Transform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	float Density = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	FVector BoundsMin = -FVector::One();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	FVector BoundsMax = FVector::One();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	FVector4 Color = FVector4::One();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties, meta = (ClampMin = "0", ClampMax = "1"))
	float Steepness = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Properties|Metadata")
	int64 MetadataEntry = -1;

	FVector GetExtents() const { return (BoundsMax - BoundsMin) / 2.0; }
	void SetExtents(const FVector& InExtents)
	{
		const FVector Center = GetLocalCenter();
		BoundsMin = Center - InExtents;
		BoundsMax = Center + InExtents;
	}

	FVector GetLocalCenter() const { return (BoundsMax + BoundsMin) / 2.0; }
	void SetLocalCenter(const FVector& InCenter)
	{
		const FVector Delta = InCenter - GetLocalCenter();
		BoundsMin += Delta;
		BoundsMax += Delta;
	}
};
