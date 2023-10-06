// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementArc.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoMath.h"
#include "DynamicMeshBuilder.h"
#include "InputState.h"
#include "Materials/MaterialInterface.h"
#include "SceneManagement.h"
#include "VectorUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementArc)


static void DrawThickArc(FPrimitiveDrawInterface* PDI, const FVector& InCenter, const FVector& InAxis0, const FVector& InAxis1,
	double InOuterRadius, double InInnerRadius, const double InStartAngle, const double InEndAngle, const int32 InNumSegments,
	const FMaterialRenderProxy* MaterialRenderProxy, const FColor& InColor)
{
	// Implementation copied from FWidget::DrawThickArc. This should eventually be moved to PrimDrawingUtils.h/cpp.

	if (InColor.A == 0)
	{
		return;
	}

	const int32 NumPoints = FMath::TruncToInt32(InNumSegments * (InEndAngle - InStartAngle) / (UE_DOUBLE_PI / 2.0)) + 1;

	FColor TriangleColor = InColor;
	FColor RingColor = InColor;
	RingColor.A = MAX_uint8;

	FVector ZAxis = InAxis0 ^ InAxis1;
	FVector LastWorldVertex;

	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

	for (int32 RadiusIndex = 0; RadiusIndex < 2; ++RadiusIndex)
	{
		double Radius = (RadiusIndex == 0) ? InOuterRadius : InInnerRadius;
		double TCRadius = Radius / (double)InOuterRadius;
		//Compute vertices for base circle.
		for (int32 VertexIndex = 0; VertexIndex <= NumPoints; VertexIndex++)
		{
			double Percent = VertexIndex / (double)NumPoints;
			double Angle = FMath::Lerp(InStartAngle, InEndAngle, Percent);
			double AngleDeg = FRotator::ClampAxis(Angle * 180.f / PI);

			FVector VertexDir = InAxis0.RotateAngleAxis(AngleDeg, ZAxis);
			VertexDir.Normalize();

			double TCAngle = Percent * (PI / 2);
			FVector2f TC(static_cast<float>(TCRadius * FMath::Cos(Angle)), static_cast<float>(TCRadius * FMath::Sin(Angle)));

			// Keep the vertices in local space so that we don't lose precision when dealing with LWC
			// The local-to-world transform is handled in the MeshBuilder.Draw() call at the end of this function
			const FVector VertexPosition = VertexDir * Radius;
			FVector Normal = VertexPosition;
			Normal.Normalize();

			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = (FVector3f)VertexPosition;
			MeshVertex.Color = TriangleColor;
			MeshVertex.TextureCoordinate[0] = TC;

			MeshVertex.SetTangents(
				(FVector3f)-ZAxis,
				FVector3f((-ZAxis) ^ Normal),
				(FVector3f)Normal
			);

			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex

			// Push out the arc line borders so they dont z-fight with the mesh arcs
			// DrawLine needs vertices in world space, but this is fine because it takes FVectors and works with LWC well
			FVector StartLinePos = LastWorldVertex;
			FVector EndLinePos = VertexPosition + InCenter;
			if (VertexIndex != 0)
			{
				PDI->DrawLine(StartLinePos, EndLinePos, RingColor, SDPG_Foreground);
			}
			LastWorldVertex = EndLinePos;
		}
	}

	//Add top/bottom triangles, in the style of a fan.
	int32 InnerVertexStartIndex = NumPoints + 1;
	for (int32 VertexIndex = 0; VertexIndex < NumPoints; VertexIndex++)
	{
		MeshBuilder.AddTriangle(VertexIndex, VertexIndex + 1, InnerVertexStartIndex + VertexIndex);
		MeshBuilder.AddTriangle(VertexIndex + 1, InnerVertexStartIndex + VertexIndex + 1, InnerVertexStartIndex + VertexIndex);
	}

	MeshBuilder.Draw(PDI, FTranslationMatrix(InCenter), MaterialRenderProxy, SDPG_Foreground, 0.f);
}


void UGizmoElementArc::Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState)
{
	FRenderTraversalState CurrentRenderState(RenderState);
	bool bVisibleViewDependent = UpdateRenderState(RenderAPI, Center, CurrentRenderState);

	if (!bVisibleViewDependent)
	{
		return;
	}

	const UMaterialInterface* UseMaterial = CurrentRenderState.GetCurrentMaterial();
	if (!UseMaterial)
	{
		return;
	}
	const FVector WorldCenter = CurrentRenderState.LocalToWorldTransform.TransformPosition(FVector::ZeroVector);
	const FVector WorldAxis0 = CurrentRenderState.LocalToWorldTransform.TransformVectorNoScale(Axis0);
	const FVector WorldAxis1 = CurrentRenderState.LocalToWorldTransform.TransformVectorNoScale(Axis1);
	const FVector WorldNormal = WorldAxis0 ^ WorldAxis1;

	bool bPartial = IsPartial(RenderAPI->GetSceneView(), WorldCenter, WorldNormal);
	double Start, End;
	if (bPartial)
	{
		if (PartialEndAngle - PartialStartAngle <= 0)
		{
			return;
		}

		Start = PartialStartAngle;
		End = PartialEndAngle;
	}
	else
	{
		Start = 0.0;
		End = UE_DOUBLE_TWO_PI;
	}

	const double WorldOuterRadius = Radius * CurrentRenderState.LocalToWorldTransform.GetScale3D().X;
	const double WorldInnerRadius = InnerRadius * CurrentRenderState.LocalToWorldTransform.GetScale3D().X;
	const FColor VertexColor = CurrentRenderState.GetCurrentVertexColor().ToFColor(false);

	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

	DrawThickArc(PDI, WorldCenter, WorldAxis0, WorldAxis1, WorldOuterRadius, WorldInnerRadius,
		Start, End, NumSegments, UseMaterial->GetRenderProxy(), VertexColor);
}


FInputRayHit UGizmoElementArc::LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection)
{
	FLineTraceTraversalState CurrentLineTraceState(LineTraceState);
	bool bHittableViewDependent = UpdateLineTraceState(ViewContext, Center, CurrentLineTraceState);

	if (bHittableViewDependent)
	{
		const double PixelHitThresholdAdjust = CurrentLineTraceState.PixelToWorldScale * PixelHitDistanceThreshold;
		const FVector WorldCenter = CurrentLineTraceState.LocalToWorldTransform.TransformPosition(FVector::ZeroVector);
		const FVector WorldAxis0 = CurrentLineTraceState.LocalToWorldTransform.TransformVectorNoScale(Axis0);
		const FVector WorldAxis1 = CurrentLineTraceState.LocalToWorldTransform.TransformVectorNoScale(Axis1);
		const FVector WorldNormal = WorldAxis0 ^ WorldAxis1;
		double HitDepth = -1.0;

		bool bPartial = IsPartial(ViewContext, WorldCenter, WorldNormal);
		const double PartialAngle = PartialEndAngle - PartialStartAngle;

		if (bPartial && PartialAngle <= 0.0)
		{
			return FInputRayHit();
		}

		// Intersect ray with plane in which arc lies.
		FPlane Plane(WorldCenter, WorldNormal);
		HitDepth = FMath::RayPlaneIntersectionParam(RayOrigin, RayDirection, Plane);
		if (HitDepth < 0.0)
		{
			return FInputRayHit();
		}

		FVector HitPoint = RayOrigin + RayDirection * HitDepth;
		FVector HitVec = HitPoint - WorldCenter;

		// Determine whether hit point lies within the arc
		const double WorldOuterRadius = Radius * CurrentLineTraceState.LocalToWorldTransform.GetScale3D().X;
		const double WorldInnerRadius = InnerRadius * CurrentLineTraceState.LocalToWorldTransform.GetScale3D().X;
		const double Distance = HitVec.Length();
		const double HitBufferMax = WorldOuterRadius + PixelHitThresholdAdjust;
		const double HitBufferMin = WorldInnerRadius - PixelHitThresholdAdjust;

		if (Distance > HitBufferMax || Distance < HitBufferMin)
		{
			return FInputRayHit();
		}

		// Handle partial arc
		if (bPartial)
		{
			// Compute projected angle
			FVector WorldBeginAxis = WorldAxis0.RotateAngleAxis(PartialStartAngle, WorldNormal).GetSafeNormal();
			FVector HitVecNormal = HitVec.GetSafeNormal();
			double HitAngle = UE::Geometry::VectorUtil::PlaneAngleSignedR(WorldBeginAxis, HitVec, WorldNormal); 

			if (HitAngle < 0)
			{
				HitAngle = UE_DOUBLE_TWO_PI + HitAngle;
			}

			if (HitAngle > PartialAngle)
			{
				return FInputRayHit();
			}
		}

		if (HitDepth >= 0.0)
		{
			FInputRayHit RayHit(static_cast<float>(HitDepth));
			RayHit.SetHitObject(this);
			RayHit.HitIdentifier = PartIdentifier;
			return RayHit;
		}
	}
	
	return FInputRayHit();
}

void UGizmoElementArc::SetInnerRadius(double InInnerRadius)
{
	InnerRadius = InInnerRadius;
}

double UGizmoElementArc::GetInnerRadius() const
{
	return InnerRadius;
}

