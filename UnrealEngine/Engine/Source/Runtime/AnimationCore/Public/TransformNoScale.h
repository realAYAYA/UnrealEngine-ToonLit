// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "UObject/ObjectMacros.h"

#include "TransformNoScale.generated.h"

USTRUCT(BlueprintType)
struct FTransformNoScale
{
	GENERATED_BODY()

	/**
	 * The identity transformation (Rotation = FRotator::ZeroRotator, Translation = FVector::ZeroVector, Scale = (1,1,1)).
	 */
	static ANIMATIONCORE_API const FTransformNoScale Identity;

	FORCEINLINE_DEBUGGABLE FTransformNoScale()
		: Location(ForceInitToZero)
		, Rotation(ForceInitToZero)
	{
	}

	FORCEINLINE_DEBUGGABLE FTransformNoScale(const FVector& InLocation, const FQuat& InRotation)
		: Location(InLocation)
		, Rotation(InRotation)
	{
	}

	FORCEINLINE_DEBUGGABLE FTransformNoScale(const FTransform& InTransform)
		: Location(InTransform.GetLocation())
		, Rotation(InTransform.GetRotation())
	{
	}

	FORCEINLINE_DEBUGGABLE FTransformNoScale& operator =(const FTransform& InTransform)
	{
		FromFTransform(InTransform);
		return *this;
	}

	FORCEINLINE_DEBUGGABLE operator FTransform() const
	{
		return ToFTransform();
	}

	/** The translation of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FVector Location;

	/** The rotation of this transform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FQuat Rotation;

	/** Convert to an FTransform */
	FORCEINLINE_DEBUGGABLE FTransform ToFTransform() const
	{
		return FTransform(Rotation, Location, FVector::OneVector);
	}

	/** Convert from an FTransform */
	FORCEINLINE_DEBUGGABLE void FromFTransform(const FTransform& InTransform)
	{
		Location = InTransform.GetLocation();
		Rotation = InTransform.GetRotation();
	}
};
