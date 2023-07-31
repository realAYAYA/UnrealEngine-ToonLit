// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementLineBase.h"
#include "InputState.h"
#include "UObject/ObjectMacros.h"
#include "GizmoElementCircleBase.generated.h"

class FPrimitiveDrawInterface;
class FMaterialRenderProxy;

/**
 * Abstract base object for circle, torus and arc.
 */
UCLASS(Transient, Abstract)
class INTERACTIVETOOLSFRAMEWORK_API UGizmoElementCircleBase : public UGizmoElementLineBase
{
	GENERATED_BODY()

public:

	// CircleBase center.
	virtual void SetCenter(const FVector& InCenter);
	virtual FVector GetCenter() const;

	// Axis0 of plane in which circle lies, must perpendicular to normal. 
	// Start and end angles for partial circles are relative to this axis.
	virtual void SetAxis0(const FVector& InAxis0);
	virtual FVector GetAxis0() const;

	// Axis1 of plane in which circle lies, must perpendicular to Axis0.
	virtual void SetAxis1(const FVector& InAxis0);
	virtual FVector GetAxis1() const;

	// Circle radius.
	virtual void SetRadius(double InRadius);
	virtual double GetRadius() const;

	// Number of segments for rendering circle.
	virtual void SetNumSegments(int32 InNumSegments);
	virtual int32 GetNumSegments() const;

	// When Partial, renders partial arc based on angle.
	// When PartialViewDependent, same as Partial but renders full arc when arc normal aligns with view direction
	virtual void SetPartialType(EGizmoElementPartialType InPartial);
	virtual EGizmoElementPartialType GetPartialType() const;

	// Start of arc angle of partial torus in radians, relative to Axis0.
	virtual void SetPartialStartAngle(double InPartialAngle);
	virtual double GetPartialStartAngle() const;

	// Start of arc angle of partial torus in radians, relative to Axis1.
	virtual void SetPartialEndAngle(double InPartialAngle);
	virtual double GetPartialEndAngle() const;

	// If partial type is PartialViewDependent, when the cosine of angle between the normal and view direction 
	// is within this tolerance, the arc will be rendered as full rather than partial
	virtual void SetPartialViewDependentMaxCosTol(double InPartialViewDependentMaxCosTol);
	virtual double GetPartialViewDependentMaxCosTol() const;

protected:

	// CircleBase center.
	UPROPERTY()
	FVector Center = FVector::ZeroVector;

	// Axis0 of plane in which circle lies, must perpendicular to normal. 
	// Start and end angles for partial circles are relative to this axis.
	UPROPERTY()
	FVector Axis0 = FVector::RightVector;

	// Axis1 of plane in which circle lies, must perpendicular to Axis0.
	UPROPERTY()
	FVector Axis1 = FVector::ForwardVector;

	// Radius of main circle, some derived elements have inner radius (e.g. torus and arc)
	UPROPERTY()
	double Radius = 100.0;

	// Number of segments for rendering arc.
	UPROPERTY()
	int32 NumSegments = 64;

	// True when the arc is not full.
	UPROPERTY()
	EGizmoElementPartialType PartialType = EGizmoElementPartialType::None;

	// Start angle to render for partial torus
	UPROPERTY()
	double PartialStartAngle = 0.0;

	// End angle to render for partial torus
	UPROPERTY()
	double PartialEndAngle = UE_DOUBLE_TWO_PI;

	// For PartialViewDependent, max cosine of angle between the normal and view direction 
	// Within this tolerance, the arc will be rendered as full rather than partial
	UPROPERTY()
	double PartialViewDependentMaxCosTol = 0.96f;

protected:

	// Returns whether element should be partial based on current view
	bool IsPartial(const FSceneView* View, const FVector& InWorldCenter, const FVector& InWorldNormal);

	// Returns whether element should be partial based on current view
	bool IsPartial(const UGizmoViewContext* ViewContext, const FVector& InWorldCenter, const FVector& InWorldNormal);

	// Returns whether element should be partial based on current view
	bool IsPartial(const FVector& InWorldCenter, const FVector& InWorldNormal,
		const FVector& InViewLocation, const FVector& InViewDirection,
		bool bIsPerspectiveProjection);
};