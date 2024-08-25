// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"

#include "PCGSplineStruct.generated.h"

/** Subset of the Spline Component API in a standalone struct */
USTRUCT()
struct PCG_API FPCGSplineStruct
{
	GENERATED_BODY()

	void Initialize(const USplineComponent* InSplineComponent);
	void Initialize(const TArray<FSplinePoint>& InSplinePoints, bool bIsClosedLoop, const FTransform& InTransform);
	void ApplyTo(USplineComponent* InSplineComponent);

	FTransform GetTransform() const { return Transform; }

	// Spline-related methods
	void AddPoint(const FSplinePoint& InSplinePoint, bool bUpdateSpline); 
	void AddPoints(const TArray<FSplinePoint>& InSplinePoints, bool bUpdateSpline);
	void UpdateSpline();

	int GetNumberOfSplineSegments() const;
	bool IsClosedLoop() const { return bClosedLoop; }
	FVector::FReal GetSplineLength() const;
	FBox GetBounds() const;

	const FInterpCurveVector& GetSplinePointsScale() const;
	const FInterpCurveVector& GetSplinePointsPosition() const;

	FVector GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FTransform GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale = false) const;
	FVector GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FQuat GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FVector GetScaleAtSplineInputKey(float InKey) const;

	FVector::FReal GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const;
	FVector GetLocationAtDistanceAlongSpline(FVector::FReal Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;
	FTransform GetTransformAtDistanceAlongSpline(FVector::FReal Distance, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale = false) const;
	
	float FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const;	

	UPROPERTY()
	FSplineCurves SplineCurves;

	// Replaces the component trasnform
	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	FVector DefaultUpVector = FVector::UpVector;

	UPROPERTY()
	int32 ReparamStepsPerSegment = 10;

	UPROPERTY()
	bool bClosedLoop = false;

	UPROPERTY()
	FBoxSphereBounds LocalBounds = FBoxSphereBounds(EForceInit::ForceInit);

	UPROPERTY()
	FBoxSphereBounds Bounds = FBoxSphereBounds(EForceInit::ForceInit);
};