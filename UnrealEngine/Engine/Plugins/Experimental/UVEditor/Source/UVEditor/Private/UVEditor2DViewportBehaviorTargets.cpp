// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditor2DViewportBehaviorTargets.h"

#include "UVEditor2DViewportClient.h"

// FUVEditor2DScrollBehaviorTarget

FUVEditor2DScrollBehaviorTarget::FUVEditor2DScrollBehaviorTarget(FUVEditor2DViewportClient* ViewportClientIn)
	: ViewportClient(ViewportClientIn)
{
}

FInputRayHit FUVEditor2DScrollBehaviorTarget::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	// Verify that ray is facing the proper direction
	if (PressPos.WorldRay.Direction.Z * PressPos.WorldRay.Origin.Z < 0)
	{
		return FInputRayHit(static_cast<float>(-PressPos.WorldRay.Origin.Z / PressPos.WorldRay.Direction.Z));
	}
	return FInputRayHit();
}

void FUVEditor2DScrollBehaviorTarget::OnClickPress(const FInputDeviceRay& PressPos)
{
	if (ensure(PressPos.WorldRay.Direction.Z * PressPos.WorldRay.Origin.Z < 0))
	{
		// Intersect with XY plane
		double T = -PressPos.WorldRay.Origin.Z / PressPos.WorldRay.Direction.Z;
		DragStart = FVector3d(
			PressPos.WorldRay.Origin.X + T * PressPos.WorldRay.Direction.X,
			PressPos.WorldRay.Origin.Y + T * PressPos.WorldRay.Direction.Y,
			0);

		OriginalCameraLocation = (FVector3d)ViewportClient->GetViewLocation();
	}
}

void FUVEditor2DScrollBehaviorTarget::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (ensure(DragPos.WorldRay.Direction.Z * DragPos.WorldRay.Origin.Z < 0))
	{
		// Intersect a ray starting from the original position and using the new
		// ray direction. I.e., pretend the camera is not moving.

		double T = -OriginalCameraLocation.Z / DragPos.WorldRay.Direction.Z;
		FVector3d DragEnd = FVector3d(
			OriginalCameraLocation.X + T * DragPos.WorldRay.Direction.X,
			OriginalCameraLocation.Y + T * DragPos.WorldRay.Direction.Y,
			0);

		// We want to make it look like we are sliding the plane such that DragStart
		// ends up on DragEnd. For that, our camera will be moving the opposite direction.
		FVector3d CameraDisplacement = DragStart - DragEnd;
		check(CameraDisplacement.Z == 0);
		ViewportClient->SetViewLocation((FVector)(OriginalCameraLocation + CameraDisplacement));
	}
}

void FUVEditor2DScrollBehaviorTarget::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
}

void FUVEditor2DScrollBehaviorTarget::OnTerminateDragSequence()
{
}



// FUVEditor2DMouseWheelZoomBehaviorTarget

FUVEditor2DMouseWheelZoomBehaviorTarget::FUVEditor2DMouseWheelZoomBehaviorTarget(FUVEditor2DViewportClient* ViewportClientIn)
	: ViewportClient(ViewportClientIn)
{
	SetZoomAmount(DEFAULT_ZOOM_AMOUNT);
}

FInputRayHit FUVEditor2DMouseWheelZoomBehaviorTarget::ShouldRespondToMouseWheel(const FInputDeviceRay& CurrentPos)
{
	// Always allowed to zoom with mouse wheel
	FInputRayHit ToReturn;
	ToReturn.bHit = true;
	return ToReturn;
}

void FUVEditor2DMouseWheelZoomBehaviorTarget::OnMouseWheelScrollUp(const FInputDeviceRay& CurrentPos)
{
	FVector OriginalLocation = ViewportClient->GetViewLocation();
	double TToPlane = -OriginalLocation.Z / CurrentPos.WorldRay.Direction.Z;

	FVector NewLocation = OriginalLocation + (ZoomInProportion * TToPlane * CurrentPos.WorldRay.Direction);

	ViewportClient->OverrideFarClipPlane(static_cast<float>(NewLocation.Z - CameraFarPlaneWorldZ));
	ViewportClient->OverrideNearClipPlane(static_cast<float>(NewLocation.Z * (1.0-CameraNearPlaneProportionZ)));

	// Don't zoom in so far that the XY plane lies in front of our near clipping plane, or else everything
	// will suddenly disappear.
	if (NewLocation.Z > ViewportClient->GetNearClipPlane() && NewLocation.Z > ZoomInLimit)
	{
		ViewportClient->SetViewLocation(NewLocation);
	}
}

void FUVEditor2DMouseWheelZoomBehaviorTarget::OnMouseWheelScrollDown(const FInputDeviceRay& CurrentPos)
{
	FVector OriginalLocation = ViewportClient->GetViewLocation();
	double TToPlane = -OriginalLocation.Z / CurrentPos.WorldRay.Direction.Z;
	FVector NewLocation = OriginalLocation - (ZoomOutProportion * TToPlane * CurrentPos.WorldRay.Direction);

	ViewportClient->OverrideFarClipPlane(static_cast<float>(NewLocation.Z - CameraFarPlaneWorldZ));
	ViewportClient->OverrideNearClipPlane(static_cast<float>(NewLocation.Z * (1.0 - CameraNearPlaneProportionZ)));

	if (NewLocation.Z < ZoomOutLimit)
	{
		ViewportClient->SetViewLocation(NewLocation);
	}
}

void FUVEditor2DMouseWheelZoomBehaviorTarget::SetZoomAmount(double PercentZoomIn)
{
	check(PercentZoomIn < 100 && PercentZoomIn >= 0);

	ZoomInProportion = PercentZoomIn / 100;

	// Set the zoom out proportion such that (1 + ZoomOutProportion)(1 - ZoomInProportion) = 1
	// so that zooming in and then zooming out will return to the same zoom level.
	ZoomOutProportion = ZoomInProportion / (1 - ZoomInProportion);
}

void FUVEditor2DMouseWheelZoomBehaviorTarget::SetZoomLimits(double ZoomInLimitIn, double ZoomOutLimitIn)
{
	ZoomInLimit = ZoomInLimitIn;
	ZoomOutLimit = ZoomOutLimitIn;
}

void FUVEditor2DMouseWheelZoomBehaviorTarget::SetCameraFarPlaneWorldZ(double CameraFarPlaneWorldZIn)
{
	CameraFarPlaneWorldZ = CameraFarPlaneWorldZIn;
}

void FUVEditor2DMouseWheelZoomBehaviorTarget::SetCameraNearPlaneProportionZ(double CameraNearPlaneProportionZIn)
{
	CameraNearPlaneProportionZ = CameraNearPlaneProportionZIn;
}