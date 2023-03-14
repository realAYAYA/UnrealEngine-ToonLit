// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementArrow.h"
#include "BaseGizmos/GizmoElementBox.h"
#include "BaseGizmos/GizmoElementCone.h"
#include "BaseGizmos/GizmoElementCylinder.h"
#include "BaseGizmos/GizmoMath.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "InputState.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementArrow)

UGizmoElementArrow::UGizmoElementArrow()
{
	HeadType = EGizmoElementArrowHeadType::Cone;
	CylinderElement = NewObject<UGizmoElementCylinder>();
	ConeElement = NewObject<UGizmoElementCone>();
	BoxElement = nullptr;
}

void UGizmoElementArrow::Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState)
{
	check(RenderAPI);

	if (bUpdateArrowBody)
	{
		UpdateArrowBody();
	}

	if (bUpdateArrowHead)
	{
		UpdateArrowHead();
	}

	FRenderTraversalState CurrentRenderState(RenderState);
	bool bVisibleViewDependent = UpdateRenderState(RenderAPI, Base, CurrentRenderState);

	if (bVisibleViewDependent)
	{
		check(CylinderElement);
		CylinderElement->Render(RenderAPI, CurrentRenderState);

		if (HeadType == EGizmoElementArrowHeadType::Cone)
		{
			check(ConeElement);
			ConeElement->Render(RenderAPI, CurrentRenderState);
		}
		else // (HeadType == EGizmoElementArrowHeadType::Cube)
		{
			check(BoxElement);
			BoxElement->Render(RenderAPI, CurrentRenderState);
		}
	}
}

FInputRayHit UGizmoElementArrow::LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection)
{
	FLineTraceTraversalState CurrentLineTraceState(LineTraceState);
	bool bHittableViewDependent = UpdateLineTraceState(ViewContext, Base, CurrentLineTraceState);

	if (!bHittableViewDependent)
	{
		return FInputRayHit();
	}

	check(CylinderElement);
	FInputRayHit Hit = CylinderElement->LineTrace(ViewContext, CurrentLineTraceState, RayOrigin, RayDirection);

	if (!Hit.bHit)
	{
		if (HeadType == EGizmoElementArrowHeadType::Cone)
		{
			check(ConeElement);
			Hit = ConeElement->LineTrace(ViewContext, CurrentLineTraceState, RayOrigin, RayDirection);
		}
		else // (HeadType == EGizmoElementArrowHeadType::Cube)
		{
			check(BoxElement);
			Hit = BoxElement->LineTrace(ViewContext, CurrentLineTraceState, RayOrigin, RayDirection);
		}
	}

	if (Hit.bHit)
	{
		Hit.SetHitObject(this);
		Hit.HitIdentifier = PartIdentifier;
	}

	return Hit;
}

void UGizmoElementArrow::SetBase(const FVector& InBase)
{
	if (Base != InBase)
	{
		Base = InBase;
		bUpdateArrowBody = true;
		bUpdateArrowHead = true;
	}
}

FVector UGizmoElementArrow::GetBase() const
{
	return Base;
}

void UGizmoElementArrow::SetDirection(const FVector& InDirection)
{
	Direction = InDirection;
	Direction.Normalize();
	bUpdateArrowBody = true;
	bUpdateArrowHead = true;
}

FVector UGizmoElementArrow::GetDirection() const
{
	return Direction;
}

void UGizmoElementArrow::SetSideDirection(const FVector& InSideDirection)
{
	SideDirection = InSideDirection;
	SideDirection.Normalize();
	bUpdateArrowHead = true;
}

FVector UGizmoElementArrow::GetSideDirection() const
{
	return SideDirection;
}

void UGizmoElementArrow::SetBodyLength(float InBodyLength)
{
	if (BodyLength != InBodyLength)
	{
		BodyLength = InBodyLength;
		bUpdateArrowBody = true;
		bUpdateArrowHead = true;
	}
}

float UGizmoElementArrow::GetBodyLength() const
{
	return BodyLength;
}

void UGizmoElementArrow::SetBodyRadius(float InBodyRadius)
{
	if (BodyRadius != InBodyRadius)
	{
		BodyRadius = InBodyRadius;
		bUpdateArrowBody = true;
		bUpdateArrowHead = true;
	}
}

float UGizmoElementArrow::GetBodyRadius() const
{
	return BodyRadius;
}

void UGizmoElementArrow::SetHeadLength(float InHeadLength)
{
	if (HeadLength != InHeadLength)
	{
		HeadLength = InHeadLength;
		bUpdateArrowHead = true;
	}
}

float UGizmoElementArrow::GetHeadLength() const
{
	return HeadLength;
}

void UGizmoElementArrow::SetHeadRadius(float InHeadRadius)
{
	if (HeadRadius != InHeadRadius)
	{
		HeadRadius = InHeadRadius;
		bUpdateArrowHead = true;
	}
}

float UGizmoElementArrow::GetHeadRadius() const
{
	return HeadRadius;
}

void UGizmoElementArrow::SetNumSides(int32 InNumSides)
{
	if (NumSides != InNumSides)
	{
		NumSides = InNumSides;
		bUpdateArrowBody = true;
		bUpdateArrowHead = true;
	}
}

int32 UGizmoElementArrow::GetNumSides() const
{
	return NumSides;
}

void UGizmoElementArrow::SetPixelHitDistanceThreshold(float InPixelHitDistanceThreshold)
{
	if (PixelHitDistanceThreshold != InPixelHitDistanceThreshold)
	{
		PixelHitDistanceThreshold = InPixelHitDistanceThreshold;
		bUpdateArrowBody = true;
		bUpdateArrowHead = true;
	}
}

void UGizmoElementArrow::SetHeadType(EGizmoElementArrowHeadType InHeadType)
{
	if (InHeadType != HeadType)
	{
		HeadType = InHeadType;

		if (HeadType == EGizmoElementArrowHeadType::Cone)
		{
			ConeElement = NewObject<UGizmoElementCone>();
			BoxElement = nullptr;
		}
		else // (HeadType == EGizmoElementArrowHeadType::Cube)
		{
			BoxElement = NewObject<UGizmoElementBox>();
			ConeElement = nullptr;
		}
		UpdateArrowHead();
	}
}

EGizmoElementArrowHeadType UGizmoElementArrow::GetHeadType() const
{
	return HeadType;
}

void UGizmoElementArrow::UpdateArrowBody()
{
	CylinderElement->SetBase(FVector::ZeroVector);
	CylinderElement->SetDirection(Direction);
	CylinderElement->SetHeight(BodyLength);
	CylinderElement->SetNumSides(NumSides);
	CylinderElement->SetRadius(BodyRadius);
	CylinderElement->SetPixelHitDistanceThreshold(PixelHitDistanceThreshold);

	bUpdateArrowBody = false;
}

void UGizmoElementArrow::UpdateArrowHead()
{
	if (HeadType == EGizmoElementArrowHeadType::Cone)
	{
		check(ConeElement);
		// head length is multiplied by 0.9f to prevent gap between body cylinder and head cone
		ConeElement->SetOrigin(Direction * (BodyLength + HeadLength * 0.9f)); 
		ConeElement->SetDirection(-Direction);
		ConeElement->SetHeight(HeadLength);
		ConeElement->SetRadius(HeadRadius);
		ConeElement->SetNumSides(NumSides);
		ConeElement->SetElementInteractionState(ElementInteractionState);
		ConeElement->SetPixelHitDistanceThreshold(PixelHitDistanceThreshold);
	}
	else // (HeadType == EGizmoElementArrowHeadType::Cube)
	{
		check(BoxElement);
		BoxElement->SetCenter(Direction * (BodyLength + HeadLength * 0.5f));
		BoxElement->SetUpDirection(Direction);
		BoxElement->SetSideDirection(SideDirection);
		BoxElement->SetDimensions(FVector(HeadLength, HeadLength, HeadLength));
		BoxElement->SetElementInteractionState(ElementInteractionState);
		BoxElement->SetPixelHitDistanceThreshold(PixelHitDistanceThreshold);
	}

	bUpdateArrowHead = false;
}

