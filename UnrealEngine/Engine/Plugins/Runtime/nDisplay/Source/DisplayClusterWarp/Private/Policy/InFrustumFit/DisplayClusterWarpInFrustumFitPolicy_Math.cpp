// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/InFrustumFit/DisplayClusterWarpInFrustumFitPolicy.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Components/DisplayClusterInFrustumFitCameraComponent.h"
#include "Camera/CameraTypes.h"
#include "HAL/IConsoleManager.h"
#include "IDisplayClusterWarpBlend.h"

// Debug: enable camera position fittig
int32 GDisplayClusterWarpInFrustumFitPolicyEnableCameraPositionFit = 1;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyEnableCameraPositionFit(
	TEXT("nDisplay.warp.InFrustumFit.EnableCameraPositionFit"),
	GDisplayClusterWarpInFrustumFitPolicyEnableCameraPositionFit,
	TEXT("(debug) Enable projection angles fitting (0 - disable)\n"),
	ECVF_Default
);

// Debug: enable projection angle fitting
int32 GDisplayClusterWarpInFrustumFitPolicyEnableProjectionAnglesFit = 1;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyEnableProjectionAnglesFit(
	TEXT("nDisplay.warp.InFrustumFit.EnableProjectionAnglesFit"),
	GDisplayClusterWarpInFrustumFitPolicyEnableProjectionAnglesFit,
	TEXT("(debug) Enable projection angles fitting (0 - disable)\n"),
	ECVF_Default
);

void FDisplayClusterWarpInFrustumFitPolicy::MakeGroupFrustumSymmetrical(bool bFixedViewTarget)
{
	if (bValidGroupFrustum)
	{
		if (bFixedViewTarget)
		{
			// If the view target is set to a fixed value instead of being computed by the group AABB, we do not want to alter the view direction,
			// but make the frustum symmetric around that fixed direction. This involves expanding the asymmetric frustum so that its left and right,
			// top and bottom angles are equal

			const float MaxHorizontal = FMath::Max(FMath::Abs(GroupGeometryWarpProjection.Left), FMath::Abs(GroupGeometryWarpProjection.Right));
			const float MaxVertical = FMath::Max(FMath::Abs(GroupGeometryWarpProjection.Top), FMath::Abs(GroupGeometryWarpProjection.Bottom));

			GroupGeometryWarpProjection.Left = -MaxHorizontal;
			GroupGeometryWarpProjection.Right = MaxHorizontal;

			GroupGeometryWarpProjection.Bottom = -MaxVertical;
			GroupGeometryWarpProjection.Top = MaxVertical;
		}
		else
		{
			// Otherwise, symmetrize the view frustum by computing a new view direction within the existing frustum
			// First, compute the change in the view forward vector needed to make the frustum symmetric. This is done my storing a rotation on the original view forward vector
			// computed from the group AABB that can then be applied to the each viewport's view direction when the render pass is performed for that viewport. The rotation
			// angles can be computed by computing the difference between the current frustum's minimum edges and the half angle of the frustum's FOV
			FDisplayClusterWarpProjection GroupWarpProjectionInDegrees = GroupGeometryWarpProjection;
			GroupWarpProjectionInDegrees.ConvertProjectionAngles(EDisplayClusterWarpAngleUnit::Degrees);

			const FVector2D GroupHalfFOVDegrees = 0.5 * FVector2D(GroupWarpProjectionInDegrees.Right - GroupWarpProjectionInDegrees.Left, GroupWarpProjectionInDegrees.Top - GroupWarpProjectionInDegrees.Bottom);
			const FVector2D CorrectionAngles = GroupHalfFOVDegrees - FVector2D(FMath::Abs(GroupWarpProjectionInDegrees.Left), FMath::Abs(GroupWarpProjectionInDegrees.Bottom));

			SymmetricForwardCorrection = FRotator(CorrectionAngles.Y, CorrectionAngles.X, 0);

			// Finally, make the frustum angles themselves symmetric
			GroupWarpProjectionInDegrees.Left = -0.5 * (GroupWarpProjectionInDegrees.Right - GroupWarpProjectionInDegrees.Left);
			GroupWarpProjectionInDegrees.Right = -GroupWarpProjectionInDegrees.Left;

			GroupWarpProjectionInDegrees.Bottom = -0.5 * (GroupWarpProjectionInDegrees.Top - GroupWarpProjectionInDegrees.Bottom);
			GroupWarpProjectionInDegrees.Top = -GroupWarpProjectionInDegrees.Bottom;

			GroupWarpProjectionInDegrees.ConvertProjectionAngles(EDisplayClusterWarpAngleUnit::Default);
			GroupGeometryWarpProjection = GroupWarpProjectionInDegrees;
		}
	}
}

FDisplayClusterWarpProjection FDisplayClusterWarpInFrustumFitPolicy::ApplyInFrustumFit(IDisplayClusterViewport* InViewport, const FTransform& World2OriginTransform, const FDisplayClusterWarpProjection& InWarpProjection)
{
	UDisplayClusterInFrustumFitCameraComponent* SceneCameraComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(InViewport->GetViewPointCameraComponent(EDisplayClusterRootActorType::Scene));
	if (!SceneCameraComponent)
	{
		// By default, this structure has invalid values.
		return FDisplayClusterWarpProjection();
	}

	// Get the configuration in use
	const UDisplayClusterInFrustumFitCameraComponent& ConfigurationCameraComponent = SceneCameraComponent->GetConfigurationInFrustumFitCameraComponent(InViewport->GetConfiguration());

	FDisplayClusterWarpProjection OutWarpProjection(InWarpProjection);

	// Get location from scene component
	FMinimalViewInfo CameraViewInfo;
	SceneCameraComponent->GetDesiredView(InViewport->GetConfiguration(), CameraViewInfo);

	// Use camera position to render:
	if (GDisplayClusterWarpInFrustumFitPolicyEnableCameraPositionFit)
	{
		OutWarpProjection.CameraRotation = World2OriginTransform.InverseTransformRotation(CameraViewInfo.Rotation.Quaternion()).Rotator();
		OutWarpProjection.CameraLocation = World2OriginTransform.InverseTransformPosition(CameraViewInfo.Location);
	}

	// Fit the frustum to the rules:

	FDisplayClusterWarpProjection ViewportAngles = InWarpProjection;
	FDisplayClusterWarpProjection GroupAngles = GroupGeometryWarpProjection;

	const FVector2D GeometryFOV(FMath::Abs(GroupAngles.Right - GroupAngles.Left), FMath::Abs(GroupAngles.Top - GroupAngles.Bottom));

	// Convert frustum angles to group FOV space, normalized to 0..1
	const FVector2D ViewportMin((ViewportAngles.Left - GroupAngles.Left) / GeometryFOV.X, (ViewportAngles.Bottom - GroupAngles.Bottom) / GeometryFOV.Y);
	const FVector2D ViewportMax((ViewportAngles.Right - GroupAngles.Left) / GeometryFOV.X, (ViewportAngles.Top - GroupAngles.Bottom) / GeometryFOV.Y);

	// And convert back to camera space:
	const float CameraHalfFOVDegrees = CameraViewInfo.FOV * 0.5f;
	const float CameraHalfFOVProjection = GroupGeometryWarpProjection.ConvertDegreesToProjection(CameraHalfFOVDegrees);
	const FVector2D CameraHalfFOV(CameraHalfFOVProjection, CameraHalfFOVProjection / CameraViewInfo.AspectRatio);
	const FVector2D GeometryHalfFOV = GeometryFOV * 0.5;

	// Receive configuration from InCfgComponent
	const FVector2D FinalHalfFOV = FindFrustumFit(ConfigurationCameraComponent.CameraProjectionMode, CameraHalfFOV, GeometryHalfFOV);

	// Sample code: convert back to projection angles
	if (GDisplayClusterWarpInFrustumFitPolicyEnableProjectionAnglesFit)
	{
		OutWarpProjection.Left = -FinalHalfFOV.X + ViewportMin.X * FinalHalfFOV.X * 2;
		OutWarpProjection.Right = -FinalHalfFOV.X + ViewportMax.X * FinalHalfFOV.X * 2;
		OutWarpProjection.Top = -FinalHalfFOV.Y + ViewportMax.Y * FinalHalfFOV.Y * 2;
		OutWarpProjection.Bottom = -FinalHalfFOV.Y + ViewportMin.Y * FinalHalfFOV.Y * 2;
	}

	return OutWarpProjection;
}

FVector2D FDisplayClusterWarpInFrustumFitPolicy::FindFrustumFit(const EDisplayClusterWarpCameraProjectionMode InProjectionMode, const FVector2D& InCameraFOV, const FVector2D& InGeometryFOV)
{
	FVector2D OutFOV(InCameraFOV);

	double DestAspectRatio = InGeometryFOV.X / InGeometryFOV.Y;

	switch (InProjectionMode)
	{
	case EDisplayClusterWarpCameraProjectionMode::Fit:
		if (InCameraFOV.Y * DestAspectRatio < InCameraFOV.X)
		{
			OutFOV.Y = InCameraFOV.Y;
			OutFOV.X = OutFOV.Y * DestAspectRatio;
		}
		else
		{

			OutFOV.X = InCameraFOV.X;
			OutFOV.Y = OutFOV.X / DestAspectRatio;
		}
		break;

	case EDisplayClusterWarpCameraProjectionMode::Fill:
		if (InCameraFOV.Y * DestAspectRatio > InCameraFOV.X)
		{
			OutFOV.Y = InCameraFOV.Y;
			OutFOV.X = OutFOV.Y * DestAspectRatio;

			// todo : add check ve 180 degree fov
		}
		else
		{

			OutFOV.X = InCameraFOV.X;
			OutFOV.Y = OutFOV.X / DestAspectRatio;
		}
		break;
	}

	return OutFOV;
}
