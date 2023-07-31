// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementCylinder.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoMath.h"
#include "InputState.h"
#include "Materials/MaterialInterface.h"
#include "SceneManagement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementCylinder)

void UGizmoElementCylinder::Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState)
{
	FRenderTraversalState CurrentRenderState(RenderState);
	bool bVisibleViewDependent = UpdateRenderState(RenderAPI, Base, CurrentRenderState);

	if (bVisibleViewDependent)
	{
		if (const UMaterialInterface* UseMaterial = CurrentRenderState.GetCurrentMaterial())
		{
			const FQuat Rotation = FRotationMatrix::MakeFromZ(Direction).ToQuat();
			const double HalfHeight = Height * 0.5;
			const FVector OriginOffset = Direction * HalfHeight;

			FTransform RenderLocalToWorldTransform = FTransform(Rotation, OriginOffset) * CurrentRenderState.LocalToWorldTransform;
			FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
			DrawCylinder(PDI, RenderLocalToWorldTransform.ToMatrixWithScale(), FVector::ZeroVector, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), Radius, HalfHeight, NumSides, UseMaterial->GetRenderProxy(), SDPG_Foreground);
		}
	}
}


FInputRayHit UGizmoElementCylinder::LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection)
{
	FLineTraceTraversalState CurrentLineTraceState(LineTraceState);
	bool bHittableViewDependent = UpdateLineTraceState(ViewContext, Base, CurrentLineTraceState);

	if (bHittableViewDependent)
	{
		bool bIntersects = false;
		double RayParam = 0.0;
		
		const double PixelHitThresholdAdjust = CurrentLineTraceState.PixelToWorldScale * PixelHitDistanceThreshold;
		const double WorldHeight = Height * CurrentLineTraceState.LocalToWorldTransform.GetScale3D().X + PixelHitThresholdAdjust * 2.0;
		const double WorldRadius = Radius * CurrentLineTraceState.LocalToWorldTransform.GetScale3D().X + PixelHitThresholdAdjust;
		const FVector WorldDirection = CurrentLineTraceState.LocalToWorldTransform.TransformVectorNoScale(Direction);
		const FVector LocalCenter = Direction * Height * 0.5;
		const FVector WorldCenter = CurrentLineTraceState.LocalToWorldTransform.TransformPosition(LocalCenter);

		GizmoMath::RayCylinderIntersection(
			WorldCenter,
			WorldDirection,
			WorldRadius,
			WorldHeight,
			RayOrigin, RayDirection,
			bIntersects, RayParam);

		if (bIntersects)
		{
			FInputRayHit RayHit(static_cast<float>(RayParam));
			RayHit.SetHitObject(this);
			RayHit.HitIdentifier = PartIdentifier;
			return RayHit;
		}
	}

	return FInputRayHit();
}

void UGizmoElementCylinder::SetBase(const FVector& InBase)
{
	Base = InBase;
}

FVector UGizmoElementCylinder::GetBase() const
{
	return Base;
}

void UGizmoElementCylinder::SetDirection(const FVector& InDirection)
{
	Direction = InDirection;
	Direction.Normalize();
}

FVector UGizmoElementCylinder::GetDirection() const
{
	return Direction;
}

void UGizmoElementCylinder::SetHeight(float InHeight)
{
	Height = InHeight;
}

float UGizmoElementCylinder::GetHeight() const
{
	return Height;
}

void UGizmoElementCylinder::SetRadius(float InRadius)
{
	Radius = InRadius;
}

float UGizmoElementCylinder::GetRadius() const
{
	return Radius;
}

void UGizmoElementCylinder::SetNumSides(int32 InNumSides)
{
	NumSides = InNumSides;
}

int32 UGizmoElementCylinder::GetNumSides() const
{
	return NumSides;
}

