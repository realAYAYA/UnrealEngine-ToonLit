// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraStackTypes.h"
#include "Camera/CameraTypes.h"
#include "SceneView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraStackTypes)

static TAutoConsoleVariable<bool> CVarUseLegacyMaintainYFOV(
	TEXT("r.UseLegacyMaintainYFOVViewMatrix"),
	false,
	TEXT("Whether to use the old way to compute perspective view matrices when the aspect ratio constraint is vertical"),
	ECVF_Default
);

//////////////////////////////////////////////////////////////////////////
// FMinimalViewInfo

bool FMinimalViewInfo::Equals(const FMinimalViewInfo& OtherInfo) const
{
	return 
		(Location == OtherInfo.Location) &&
		(Rotation == OtherInfo.Rotation) &&
		(FOV == OtherInfo.FOV) &&
		(OrthoWidth == OtherInfo.OrthoWidth) &&
		(OrthoNearClipPlane == OtherInfo.OrthoNearClipPlane) &&
		(OrthoFarClipPlane == OtherInfo.OrthoFarClipPlane) &&
		((PerspectiveNearClipPlane == OtherInfo.PerspectiveNearClipPlane) || //either they are the same or both don't override
			(PerspectiveNearClipPlane <= 0.f && OtherInfo.PerspectiveNearClipPlane <= 0.f)) &&
		(AspectRatio == OtherInfo.AspectRatio) &&
		(bConstrainAspectRatio == OtherInfo.bConstrainAspectRatio) &&
		(bUseFieldOfViewForLOD == OtherInfo.bUseFieldOfViewForLOD) &&
		(ProjectionMode == OtherInfo.ProjectionMode) &&
		(OffCenterProjectionOffset == OtherInfo.OffCenterProjectionOffset);
}

void FMinimalViewInfo::BlendViewInfo(FMinimalViewInfo& OtherInfo, float OtherWeight)
{
	Location = FMath::Lerp(Location, OtherInfo.Location, OtherWeight);

	const FRotator DeltaAng = (OtherInfo.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + OtherWeight * DeltaAng;

	FOV = FMath::Lerp(FOV, OtherInfo.FOV, OtherWeight);
	OrthoWidth = FMath::Lerp(OrthoWidth, OtherInfo.OrthoWidth, OtherWeight);
	OrthoNearClipPlane = FMath::Lerp(OrthoNearClipPlane, OtherInfo.OrthoNearClipPlane, OtherWeight);
	OrthoFarClipPlane = FMath::Lerp(OrthoFarClipPlane, OtherInfo.OrthoFarClipPlane, OtherWeight);
	PerspectiveNearClipPlane = FMath::Lerp(PerspectiveNearClipPlane, OtherInfo.PerspectiveNearClipPlane, OtherWeight);
	OffCenterProjectionOffset = FMath::Lerp(OffCenterProjectionOffset, OtherInfo.OffCenterProjectionOffset, OtherWeight);

	AspectRatio = FMath::Lerp(AspectRatio, OtherInfo.AspectRatio, OtherWeight);
	bConstrainAspectRatio |= OtherInfo.bConstrainAspectRatio;
	bUseFieldOfViewForLOD |= OtherInfo.bUseFieldOfViewForLOD;
}

void FMinimalViewInfo::ApplyBlendWeight(const float& Weight)
{
	Location *= Weight;
	Rotation.Normalize();
	Rotation *= Weight;
	FOV *= Weight;
	OrthoWidth *= Weight;
	OrthoNearClipPlane *= Weight;
	OrthoFarClipPlane *= Weight;
	PerspectiveNearClipPlane *= Weight;
	AspectRatio *= Weight;
	OffCenterProjectionOffset *= Weight;
}

void FMinimalViewInfo::AddWeightedViewInfo(const FMinimalViewInfo& OtherView, const float& Weight)
{
	FMinimalViewInfo OtherViewWeighted = OtherView;
	OtherViewWeighted.ApplyBlendWeight(Weight);

	Location += OtherViewWeighted.Location;
	Rotation += OtherViewWeighted.Rotation;
	FOV += OtherViewWeighted.FOV;
	OrthoWidth += OtherViewWeighted.OrthoWidth;
	OrthoNearClipPlane += OtherViewWeighted.OrthoNearClipPlane;
	OrthoFarClipPlane += OtherViewWeighted.OrthoFarClipPlane;
	PerspectiveNearClipPlane += OtherViewWeighted.PerspectiveNearClipPlane;
	AspectRatio += OtherViewWeighted.AspectRatio;
	OffCenterProjectionOffset += OtherViewWeighted.OffCenterProjectionOffset;

	bConstrainAspectRatio |= OtherViewWeighted.bConstrainAspectRatio;
	bUseFieldOfViewForLOD |= OtherViewWeighted.bUseFieldOfViewForLOD;
}

FMatrix FMinimalViewInfo::CalculateProjectionMatrix() const
{
	FMatrix ProjectionMatrix;

	if (ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		const float YScale = 1.0f / AspectRatio;

		const float HalfOrthoWidth = OrthoWidth / 2.0f;
		const float ScaledOrthoHeight = OrthoWidth / 2.0f * YScale;

		const float NearPlane = OrthoNearClipPlane;
		const float FarPlane = OrthoFarClipPlane;

		const float ZScale = 1.0f / (FarPlane - NearPlane);
		const float ZOffset = -NearPlane;

		ProjectionMatrix = FReversedZOrthoMatrix(
			HalfOrthoWidth,
			ScaledOrthoHeight,
			ZScale,
			ZOffset
			);
	}
	else
	{
		const float ClippingPlane = GetFinalPerspectiveNearClipPlane();
		// Avoid divide by zero in the projection matrix calculation by clamping FOV
		ProjectionMatrix = FReversedZPerspectiveMatrix(
			FMath::Max(0.001f, FOV) * (float)UE_PI / 360.0f,
			AspectRatio,
			1.0f,
			ClippingPlane);
	}

	if (!OffCenterProjectionOffset.IsZero())
	{
		const float Left = -1.0f + OffCenterProjectionOffset.X;
		const float Right = Left + 2.0f;
		const float Bottom = -1.0f + OffCenterProjectionOffset.Y;
		const float Top = Bottom + 2.0f;
		ProjectionMatrix.M[2][0] = (Left + Right) / (Left - Right);
		ProjectionMatrix.M[2][1] = (Bottom + Top) / (Bottom - Top);
	}

	return ProjectionMatrix;
}

void FMinimalViewInfo::CalculateProjectionMatrixGivenView(const FMinimalViewInfo& ViewInfo, TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint, FViewport* Viewport, FSceneViewProjectionData& InOutProjectionData)
{
	// Create the projection matrix (and possibly constrain the view rectangle)
	if (ViewInfo.bConstrainAspectRatio)
	{
		// Enforce a particular aspect ratio for the render of the scene. 
		// Results in black bars at top/bottom etc.
		InOutProjectionData.SetConstrainedViewRectangle(Viewport->CalculateViewExtents(ViewInfo.AspectRatio, InOutProjectionData.GetViewRect()));

		InOutProjectionData.ProjectionMatrix = ViewInfo.CalculateProjectionMatrix();
	}
	else
	{
		float XAxisMultiplier;
		float YAxisMultiplier;

		const FIntRect& ViewRect = InOutProjectionData.GetViewRect();
		const int32 SizeX = ViewRect.Width();
		const int32 SizeY = ViewRect.Height();

		// If x is bigger, and we're respecting x or major axis, AND mobile isn't forcing us to be Y axis aligned
		const bool bMaintainXFOV = 
			((SizeX > SizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || 
			(AspectRatioAxisConstraint == AspectRatio_MaintainXFOV) || 
			(ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic);
		if (bMaintainXFOV)
		{
			// If the viewport is wider than it is tall
			XAxisMultiplier = 1.0f;
			YAxisMultiplier = SizeX / (float)SizeY;
		}
		else
		{
			// If the viewport is taller than it is wide
			XAxisMultiplier = SizeY / (float)SizeX;
			YAxisMultiplier = 1.0f;
		}
		
		float MatrixHalfFOV;
		if (!bMaintainXFOV && ViewInfo.AspectRatio != 0.f && !CVarUseLegacyMaintainYFOV.GetValueOnGameThread())
		{
			// The view-info FOV is horizontal. But if we have a different aspect ratio constraint, we need to
			// adjust this FOV value using the aspect ratio it was computed with, so we that we can compute the
			// complementary FOV value (with the *effective* aspect ratio) correctly.
			const float HalfXFOV = FMath::DegreesToRadians(FMath::Max(0.001f, ViewInfo.FOV) / 2.f);
			const float HalfYFOV = FMath::Atan(FMath::Tan(HalfXFOV) / ViewInfo.AspectRatio);
			MatrixHalfFOV = HalfYFOV;
		}
		else
		{
			// Avoid divide by zero in the projection matrix calculation by clamping FOV.
			// Note the division by 360 instead of 180 because we want the half-FOV.
			MatrixHalfFOV = FMath::Max(0.001f, ViewInfo.FOV) * (float)UE_PI / 360.0f;
		}
	
		if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			const float OrthoWidth = ViewInfo.OrthoWidth / 2.0f * XAxisMultiplier;
			const float OrthoHeight = (ViewInfo.OrthoWidth / 2.0f) / YAxisMultiplier;

			const float NearPlane = ViewInfo.OrthoNearClipPlane;
			const float FarPlane = ViewInfo.OrthoFarClipPlane;

			const float ZScale = 1.0f / (FarPlane - NearPlane);
			const float ZOffset = -NearPlane;

			InOutProjectionData.ProjectionMatrix = FReversedZOrthoMatrix(
				OrthoWidth, 
				OrthoHeight,
				ZScale,
				ZOffset
				);		
		}
		else
		{
			const float ClippingPlane = ViewInfo.GetFinalPerspectiveNearClipPlane();
			InOutProjectionData.ProjectionMatrix = FReversedZPerspectiveMatrix(
				MatrixHalfFOV,
				MatrixHalfFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				ClippingPlane,
				ClippingPlane
			);
		}
	}

	if (!ViewInfo.OffCenterProjectionOffset.IsZero())
	{
		const float Left = -1.0f + ViewInfo.OffCenterProjectionOffset.X;
		const float Right = Left + 2.0f;
		const float Bottom = -1.0f + ViewInfo.OffCenterProjectionOffset.Y;
		const float Top = Bottom + 2.0f;
		InOutProjectionData.ProjectionMatrix.M[2][0] = (Left + Right) / (Left - Right);
		InOutProjectionData.ProjectionMatrix.M[2][1] = (Bottom + Top) / (Bottom - Top);
	}
}

