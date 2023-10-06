// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementCone.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoMath.h"
#include "InputState.h"
#include "Materials/MaterialInterface.h"
#include "SceneManagement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementCone)

void UGizmoElementCone::Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState)
{
	FRenderTraversalState CurrentRenderState(RenderState);
	bool bVisibleViewDependent = UpdateRenderState(RenderAPI, Origin, CurrentRenderState);

	if (bVisibleViewDependent)
	{
		if (const UMaterialInterface* UseMaterial = CurrentRenderState.GetCurrentMaterial())
		{
			FQuat Rotation = FRotationMatrix::MakeFromX(Direction).ToQuat();
			const FVector Scale(Height);

			FTransform RenderLocalToWorldTransform = FTransform(Rotation, FVector::ZeroVector, Scale) * CurrentRenderState.LocalToWorldTransform;
			const float ConeSide = FMath::Sqrt(Height * Height + Radius * Radius);
			const float Angle = FMath::Acos(Height / ConeSide);
			FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
			DrawCone(PDI, RenderLocalToWorldTransform.ToMatrixWithScale(), Angle, Angle, NumSides, false, FColor::White, UseMaterial->GetRenderProxy(), SDPG_Foreground);
		}
	}
}

FInputRayHit UGizmoElementCone::LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection)
{
	FLineTraceTraversalState CurrentLineTraceState(LineTraceState);
	bool bHittableViewDependent = UpdateLineTraceState(ViewContext, Origin, CurrentLineTraceState);

	if (bHittableViewDependent)
	{
		bool bIntersects = false;
		double RayParam = 0.0;

		const double PixelHitThresholdAdjust = CurrentLineTraceState.PixelToWorldScale * PixelHitDistanceThreshold;
		const double ConeSide = FMath::Sqrt(Height * Height + Radius * Radius);
		const double CosAngle = Height / ConeSide;
		const double WorldHeight = Height * CurrentLineTraceState.LocalToWorldTransform.GetScale3D().X + PixelHitThresholdAdjust * 2.0;
		const FVector WorldDirection = CurrentLineTraceState.LocalToWorldTransform.TransformVectorNoScale(Direction);
		const FVector WorldOrigin = CurrentLineTraceState.LocalToWorldTransform.TransformPosition(FVector::ZeroVector) - WorldDirection * PixelHitThresholdAdjust;

		GizmoMath::RayConeIntersection(
			WorldOrigin,
			WorldDirection,
			CosAngle,
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

void UGizmoElementCone::SetOrigin(const FVector& InOrigin)
{
	Origin = InOrigin;
}

FVector UGizmoElementCone::GetOrigin() const
{
	return Origin;
}

void UGizmoElementCone::SetDirection(const FVector& InDirection)
{
	Direction = InDirection;
	Direction.Normalize();
}

FVector UGizmoElementCone::GetDirection() const
{
	return Direction;
}

void UGizmoElementCone::SetHeight(float InHeight)
{
	Height = InHeight;
}

float UGizmoElementCone::GetHeight() const
{
	return Height;
}

void UGizmoElementCone::SetRadius(float InRadius)
{
	Radius = InRadius;
}

float UGizmoElementCone::GetRadius() const
{
	return Radius;
}

void UGizmoElementCone::SetNumSides(int32 InNumSides)
{
	NumSides = InNumSides;
}

int32 UGizmoElementCone::GetNumSides() const
{
	return NumSides;
}


