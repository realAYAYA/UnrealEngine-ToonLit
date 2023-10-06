// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "Components/SplineComponent.h"

#include "CineSplineMetadata.generated.h"

USTRUCT()
struct FCineSplineCurveDefaults
{
	GENERATED_BODY()

		FCineSplineCurveDefaults()
		: DefaultAbsolutePosition(-1.0f)
		, DefaultFocalLength(35.0f)
		, DefaultAperture(2.8f)
		, DefaultFocusDistance(100000.f)
		, DefaultPointRotation(FQuat::Identity)
	{};

	UPROPERTY(EditDefaultsOnly, Category = CineSplineCurveDefaults)
	float DefaultAbsolutePosition;

	UPROPERTY(EditDefaultsOnly, Category = CineSplineCurveDefaults)
	float DefaultFocalLength;

	UPROPERTY(EditDefaultsOnly, Category = CineSplineCurveDefaults)
	float DefaultAperture;

	UPROPERTY(EditDefaultsOnly, Category = CineSplineCurveDefaults)
	float DefaultFocusDistance;

	UPROPERTY(EditDefaultsOnly, Category = CineSplineCurveDefaults)
	FQuat DefaultPointRotation;
};

UCLASS()
class CINECAMERARIGS_API UCineSplineMetadata : public USplineMetadata
{
	GENERATED_BODY()
	
public:
	/** Insert point before index, lerping metadata between previous and next key values */
	virtual void InsertPoint(int32 Index, float Alpha, bool bClosedLoop) override;
	/** Update point at index by lerping metadata between previous and next key values */
	virtual void UpdatePoint(int32 Index, float Alpha, bool bClosedLoop) override;
	virtual void AddPoint(float InputKey) override;
	virtual void RemovePoint(int32 Index) override;
	virtual void DuplicatePoint(int32 Index) override;
	virtual void CopyPoint(const USplineMetadata* FromSplineMetadata, int32 FromIndex, int32 ToIndex) override;
	virtual void Reset(int32 NumPoints) override;
	virtual void Fixup(int32 NumPoints, USplineComponent* SplineComp) override;

	UPROPERTY(EditAnywhere, Category = "Point")
	FInterpCurveFloat AbsolutePosition;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FInterpCurveFloat FocalLength;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FInterpCurveFloat Aperture;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FInterpCurveFloat FocusDistance;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FInterpCurveQuat PointRotation;
};
