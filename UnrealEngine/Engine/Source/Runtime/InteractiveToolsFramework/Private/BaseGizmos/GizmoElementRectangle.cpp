// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementRectangle.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoMath.h"
#include "InputState.h"
#include "Materials/MaterialInterface.h"
#include "SceneManagement.h"
#include "Math/UnrealMathUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementRectangle)

void UGizmoElementRectangle::Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState)
{
	FRenderTraversalState CurrentRenderState(RenderState);
	bool bVisibleViewDependent = UpdateRenderState(RenderAPI, Center, CurrentRenderState);

	if (bVisibleViewDependent)
	{
		const FVector WorldUpAxis = CurrentRenderState.LocalToWorldTransform.TransformVectorNoScale(UpDirection);
		const FVector WorldSideAxis = CurrentRenderState.LocalToWorldTransform.TransformVectorNoScale(SideDirection);
		const FVector WorldCenter = CurrentRenderState.LocalToWorldTransform.TransformPosition(FVector::ZeroVector);
		const float WorldWidth = static_cast<float>(Width * CurrentRenderState.LocalToWorldTransform.GetScale3D().X);
		const float WorldHeight = static_cast<float>(Height * CurrentRenderState.LocalToWorldTransform.GetScale3D().X);

		FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

		if (bDrawMesh)
		{
			if (const UMaterialInterface* UseMaterial = CurrentRenderState.GetCurrentMaterial())
			{
				FColor VertexColor = CurrentRenderState.GetCurrentVertexColor().ToFColor(false);
				DrawRectangleMesh(PDI, WorldCenter, WorldUpAxis, WorldSideAxis, VertexColor, WorldWidth, WorldHeight, UseMaterial->GetRenderProxy(), SDPG_Foreground);
			}
		}
		if (bDrawLine)
		{
			check(RenderAPI);
			const FSceneView* View = RenderAPI->GetSceneView();
			check(View);
			float CurrentLineThickness = GetCurrentLineThickness(View->IsPerspectiveProjection(), View->FOV);

			FColor LineColor = CurrentRenderState.GetCurrentLineColor().ToFColor(false);
			DrawRectangle(PDI, WorldCenter, WorldUpAxis, WorldSideAxis, LineColor, WorldWidth, WorldHeight, SDPG_Foreground, CurrentLineThickness, 0.0, bScreenSpaceLine);
		}
	}
}

FInputRayHit UGizmoElementRectangle::LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection)
{
	FLineTraceTraversalState CurrentLineTraceState(LineTraceState);
	bool bHittableViewDependent = UpdateLineTraceState(ViewContext, Center, CurrentLineTraceState);

	if (bHittableViewDependent)
	{
		FTransform LocalToWorldTransform = CurrentLineTraceState.LocalToWorldTransform;
		const FVector WorldUpAxis = LocalToWorldTransform.TransformVectorNoScale(UpDirection);
		const FVector WorldSideAxis = LocalToWorldTransform.TransformVectorNoScale(SideDirection);
		const FVector WorldNormal = FVector::CrossProduct(WorldUpAxis, WorldSideAxis);
		const FVector WorldCenter = LocalToWorldTransform.TransformPosition(FVector::ZeroVector);
		const double Scale = LocalToWorldTransform.GetScale3D().X;
		const double PixelHitThresholdAdjust = CurrentLineTraceState.PixelToWorldScale * PixelHitDistanceThreshold;
		const double WorldHeight = Scale * Height + PixelHitThresholdAdjust * 2.0;
		const double WorldWidth = Scale * Width + PixelHitThresholdAdjust * 2.0;
		const FVector Base = WorldCenter - WorldUpAxis * WorldHeight * 0.5 - WorldSideAxis * WorldWidth * 0.5;

		// if ray is parallel to rectangle, no hit
		if (FMath::IsNearlyZero(FVector::DotProduct(WorldNormal, RayDirection)))
		{
			return FInputRayHit();
		}

		if (bHitMesh)
		{
			FPlane Plane(Base, WorldNormal);
			double HitDepth = FMath::RayPlaneIntersectionParam(RayOrigin, RayDirection, Plane);
			if (HitDepth < 0)
			{
				return FInputRayHit();
			}

			FVector HitPoint = RayOrigin + RayDirection * HitDepth;
			FVector HitOffset = HitPoint - Base;
			double HdU = FVector::DotProduct(HitOffset, WorldUpAxis);
			double HdS = FVector::DotProduct(HitOffset, WorldSideAxis);

			// clip to rectangle dimensions
			if (HdU >= 0.0 && HdU <= WorldHeight && HdS >= 0.0 && HdS <= WorldWidth)
			{
				FInputRayHit RayHit(static_cast<float>(HitDepth));
				RayHit.SetHitObject(this);
				RayHit.HitIdentifier = PartIdentifier;
				return RayHit;
			}
		}
		else if (bHitLine)
		{
			FPlane Plane(Base, WorldNormal);
			double HitDepth = FMath::RayPlaneIntersectionParam(RayOrigin, RayDirection, Plane);
			if (HitDepth < 0)
			{
				return FInputRayHit();
			}

			FVector HitPoint = RayOrigin + RayDirection * HitDepth;
			FVector HitOffset = HitPoint - Base;
			double HdU = FVector::DotProduct(HitOffset, WorldUpAxis);
			double HdS = FVector::DotProduct(HitOffset, WorldSideAxis);

			const double HitBuffer = PixelHitThresholdAdjust + LineThickness;

			// determine if the hit is within pixel tolerance of the edges of rectangle
			if (HdU >= 0.0 && HdU <= WorldHeight && HdS >= 0.0 && HdS <= WorldWidth &&
				(HdS <= HitBuffer || HdS >= WorldWidth - HitBuffer ||
				 HdU <= HitBuffer || HdU >= WorldHeight - HitBuffer))
			{
				FInputRayHit RayHit(static_cast<float>(HitDepth));
				RayHit.SetHitObject(this);
				RayHit.HitIdentifier = PartIdentifier;
				return RayHit;
			}
		}
	}
	return FInputRayHit();
}

void UGizmoElementRectangle::SetCenter(FVector InCenter)
{
	Center = InCenter;
}

FVector UGizmoElementRectangle::GetCenter() const
{
	return Center;
}

void UGizmoElementRectangle::SetWidth(float InWidth)
{
	Width = InWidth;
}

float UGizmoElementRectangle::GetWidth() const
{
	return Width;
}

void UGizmoElementRectangle::SetHeight(float InHeight)
{
	Height = InHeight;
}

float UGizmoElementRectangle::GetHeight() const
{
	return Height;
}

void UGizmoElementRectangle::SetUpDirection(const FVector& InUpDirection)
{
	UpDirection = InUpDirection.GetSafeNormal();
}

FVector UGizmoElementRectangle::GetUpDirection() const
{
	return UpDirection;
}

void UGizmoElementRectangle::SetSideDirection(const FVector& InSideDirection)
{
	SideDirection = InSideDirection.GetSafeNormal();
}

FVector UGizmoElementRectangle::GetSideDirection() const
{
	return SideDirection;
}

void UGizmoElementRectangle::SetDrawMesh(bool InDrawMesh)
{
	bDrawMesh = InDrawMesh;
}
bool UGizmoElementRectangle::GetDrawMesh() const
{
	return bDrawMesh;
}

void UGizmoElementRectangle::SetDrawLine(bool InDrawLine)
{
	bDrawLine = InDrawLine;
}

bool UGizmoElementRectangle::GetDrawLine() const
{
	return bDrawLine;
}

void UGizmoElementRectangle::SetHitMesh(bool InHitMesh)
{
	bHitMesh = InHitMesh;
}

bool UGizmoElementRectangle::GetHitMesh() const
{
	return bHitMesh;
}

void UGizmoElementRectangle::SetHitLine(bool InHitLine)
{
	bHitLine = InHitLine;
}

bool UGizmoElementRectangle::GetHitLine() const
{
	return bHitLine;
}

