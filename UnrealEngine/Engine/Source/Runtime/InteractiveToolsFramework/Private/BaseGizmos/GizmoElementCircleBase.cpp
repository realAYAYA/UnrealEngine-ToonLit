// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementCircleBase.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoMath.h"
#include "InputState.h"
#include "Materials/MaterialInterface.h"
#include "SceneManagement.h"
#include "VectorUtil.h"

bool UGizmoElementCircleBase::IsPartial(const FSceneView* View, const FVector& InWorldCenter, const FVector& InWorldNormal)
{
	return IsPartial(InWorldCenter, InWorldNormal, View->ViewLocation, View->GetViewDirection(),
		View->IsPerspectiveProjection());
}

bool UGizmoElementCircleBase::IsPartial(const UGizmoViewContext* View, const FVector& InWorldCenter, const FVector& InWorldNormal)
{
	return IsPartial(InWorldCenter, InWorldNormal, View->ViewLocation, View->GetViewDirection(),
		View->IsPerspectiveProjection());
}

bool UGizmoElementCircleBase::IsPartial(const FVector& InWorldCenter, const FVector& InWorldNormal,
	const FVector& InViewLocation, const FVector& InViewDirection, bool bIsPerspectiveProjection)
{
	FVector ViewToCircleBaseCenter;
	if (bIsPerspectiveProjection)
	{
		ViewToCircleBaseCenter = (InWorldCenter - InViewLocation).GetSafeNormal();
	}
	else
	{
		ViewToCircleBaseCenter = InViewDirection;
	}

	double DotP = FMath::Abs(FVector::DotProduct(InWorldNormal, ViewToCircleBaseCenter));
	return (PartialType == EGizmoElementPartialType::Partial ||
		(PartialType == EGizmoElementPartialType::PartialViewDependent && DotP <= PartialViewDependentMaxCosTol));
}

void UGizmoElementCircleBase::SetCenter(const FVector& InCenter)
{
	Center = InCenter;
}

FVector UGizmoElementCircleBase::GetCenter() const
{
	return Center;
}

void UGizmoElementCircleBase::SetAxis0(const FVector& InAxis0)
{
	Axis0 = InAxis0;
	Axis0.Normalize();
}

FVector UGizmoElementCircleBase::GetAxis0() const
{
	return Axis1;
}

void UGizmoElementCircleBase::SetAxis1(const FVector& InAxis1)
{
	Axis1 = InAxis1;
	Axis1.Normalize();
}

FVector UGizmoElementCircleBase::GetAxis1() const
{
	return Axis0;
}

void UGizmoElementCircleBase::SetRadius(double InRadius)
{
	Radius = InRadius;
}

double UGizmoElementCircleBase::GetRadius() const
{
	return Radius;
}

void UGizmoElementCircleBase::SetNumSegments(int32 InNumSegments)
{
	NumSegments = InNumSegments;
}

int32 UGizmoElementCircleBase::GetNumSegments() const
{
	return NumSegments;
}

void UGizmoElementCircleBase::SetPartialType(EGizmoElementPartialType InPartialType)
{
	PartialType = InPartialType;
}

EGizmoElementPartialType UGizmoElementCircleBase::GetPartialType() const
{
	return PartialType;
}

void UGizmoElementCircleBase::SetPartialStartAngle(double InPartialStartAngle)
{
	PartialStartAngle = InPartialStartAngle;
}

double UGizmoElementCircleBase::GetPartialStartAngle() const
{
	return PartialStartAngle;
}

void UGizmoElementCircleBase::SetPartialEndAngle(double InPartialEndAngle)
{
	PartialEndAngle = InPartialEndAngle;
}

double UGizmoElementCircleBase::GetPartialEndAngle() const
{
	return PartialEndAngle;
}

void UGizmoElementCircleBase::SetPartialViewDependentMaxCosTol(double InPartialViewDependentMaxCosTol)
{
	PartialViewDependentMaxCosTol = InPartialViewDependentMaxCosTol;
}

double UGizmoElementCircleBase::GetPartialViewDependentMaxCosTol() const
{
	return PartialViewDependentMaxCosTol;
}
