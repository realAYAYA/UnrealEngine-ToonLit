// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStereoscopicPass.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "DisplayClusterSceneViewExtensions.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "Misc/DisplayClusterLog.h"

#include "EngineUtils.h"
#include "SceneView.h"

namespace UE::DisplayCluster::Viewport::Math
{
	/**
	 * Calculates the ViewOffset for the eye from the view location and changes the value of the view location to the eye position.
	 * 
	 * @param PassOffsetSwap - Distance to the eye from the midpoint between the eyes.
	 * @param InOutViewLocation - (in, out) view location
	 * @param InViewRotation (in) - rotation of the view (from this rotator we get the direction to the eye)
	 * 
	 * @return - the distance to the eye from the original ViewLocation
	 */
	static inline FVector ImplGetViewOffset(const double PassOffsetSwap, FVector& InOutViewLocation, const FRotator& InViewRotation)
	{
		// Apply computed offset to the view location
		const FQuat EyeQuat = InViewRotation.Quaternion();
		FVector ViewOffset = EyeQuat.RotateVector(FVector(0.0f, PassOffsetSwap, 0.0f));
		
		InOutViewLocation += ViewOffset;

		return ViewOffset;
	}

	/** check frustum. */
	static inline void GetNonZeroFrustumRange(double& InOutValue0, double& InOutValue1, double n)
	{
		static const double MinHalfFOVRangeRad = FMath::DegreesToRadians(0.5f);
		static const double MinRangeBase = FMath::Tan(MinHalfFOVRangeRad * 2);;

		const double MinRangeValue = n * MinRangeBase;
		if ((InOutValue1 - InOutValue0) < MinRangeValue)
		{
			// Get minimal values from center of range
			const double CenterRad = (FMath::Atan(InOutValue0 / n) + (FMath::Atan(InOutValue1 / n))) * 0.5f;
			InOutValue0 = double(n * FMath::Tan(CenterRad - MinHalfFOVRangeRad));
			InOutValue1 = double(n * FMath::Tan(CenterRad + MinHalfFOVRangeRad));
		}
	}
};
///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
FVector2D FDisplayClusterViewport::GetClippingPlanes() const
{
	const float NCP = GNearClippingPlane;
	const float FCP = NCP; // nDisplay does not use the far plane of the clipping

	// Supports custom near clipping plane
	float ZNear = NCP;
	float ZFar = FCP;
	if (CustomNearClippingPlane >= 0)
	{
		ZNear = CustomNearClippingPlane;
		ZFar = (NCP == FCP) ? ZNear : ZFar;
	}

	return FVector2D(ZNear, ZFar);
}

bool FDisplayClusterViewport::GetViewPointCameraEye(const uint32 InContextNum, FVector& OutViewLocation, FRotator& OutViewRotation, FVector& OutViewOffset)
{
	using namespace UE::DisplayCluster::Viewport::Math;

	// Here we use the ViewPoint component as the eye position
	if (UDisplayClusterCameraComponent* SceneCameraComponent = GetViewPointCameraComponent(EDisplayClusterRootActorType::Scene))
	{
		OutViewLocation = SceneCameraComponent->GetComponentLocation();
		OutViewRotation = SceneCameraComponent->GetComponentRotation();

		// Calculate stereo ViewOffset:
		OutViewOffset = ImplGetViewOffset(GetStereoEyeOffsetDistance(InContextNum), OutViewLocation, OutViewRotation);

		return true;
	}

	return false;
}

bool FDisplayClusterViewport::CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const float WorldToMeters)
{
	using namespace UE::DisplayCluster::Viewport::Math;

	if (Contexts.IsValidIndex(InContextNum))
	{
		if (!EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidViewPoint))
		{
			// The function can be called several times per frame.
			// Each time it must return the same values. For optimization purposes, after the first call this function
			// stores the result in the context variables 'ViewLocation' and 'ViewRotation'.
			// Finally, raises this flag for subsequent calls in the current frame.

			if (EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedViewPoint))
			{
				// Use calculated values
				// Since this function can be called several times from LocalPlayer.cpp, the cached values are used on repeated calls.
				// This should give a performance boost for 'mesh', 'mpcdi' projections with a large number of vertices in the geometry or large warp texture size.
				InOutViewLocation = Contexts[InContextNum].ViewLocation;
				InOutViewRotation = Contexts[InContextNum].ViewRotation;

				return true;
			}

			// Calculate stereo ViewOffset:
			const FVector ViewOffset = ImplGetViewOffset(GetStereoEyeOffsetDistance(InContextNum), InOutViewLocation, InOutViewRotation);
			const FVector2D ClipingPlanes = GetClippingPlanes();
			if (ProjectionPolicy.IsValid() && ProjectionPolicy->CalculateView(this, InContextNum, InOutViewLocation, InOutViewRotation, ViewOffset, WorldToMeters, ClipingPlanes.X, ClipingPlanes.Y))
			{
				Contexts[InContextNum].WorldToMeters = WorldToMeters;

				// Save the calculated values and update the state of the context
				Contexts[InContextNum].ViewLocation = InOutViewLocation;
				Contexts[InContextNum].ViewRotation = InOutViewRotation;
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedViewPoint);

				return true;
			}

			// ProjectionPolicy->CalculateView() returns false, this view is invalid
			EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidViewPoint);
		}
	}

	return false;
}

bool FDisplayClusterViewport::GetProjectionMatrix(const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	if (Contexts.IsValidIndex(InContextNum))
	{
		if (!EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidProjectionMatrix))
		{
			// The function can also be called several times per frame.
			// stores the result in the context variables 'ProjectionMatrix' and 'OverscanProjectionMatrix'.
			// Finally, raises this flag for subsequent calls in the current frame.

			if (EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedProjectionMatrix))
			{
				// use already calculated values
				if (EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedOverscanProjectionMatrix))
				{
					// use overscan proj matrix for rendering
					OutPrjMatrix = Contexts[InContextNum].OverscanProjectionMatrix;
				}
				else
				{
					OutPrjMatrix = Contexts[InContextNum].ProjectionMatrix;
				}

				return true;
			}
			else if (ProjectionPolicy.IsValid() && ProjectionPolicy->GetProjectionMatrix(this, InContextNum, OutPrjMatrix))
			{
				// Save the calculated values and update the state of the context
				Contexts[InContextNum].ProjectionMatrix = OutPrjMatrix;
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedProjectionMatrix);

				if (OverscanRuntimeSettings.bIsEnabled)
				{
					// use overscan proj matrix for rendering
					OutPrjMatrix = Contexts[InContextNum].OverscanProjectionMatrix;
					EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedOverscanProjectionMatrix);
				}

				return true;
			}
			else
			{
				// ProjectionPolicy->GetProjectionMatrix() returns false, this projection matrix is invalid
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidProjectionMatrix);
			}
		}
	}

	return false;
}

void FDisplayClusterViewport::CalculateProjectionMatrix(const uint32 InContextNum, float Left, float Right, float Top, float Bottom, float ZNear, float ZFar, bool bIsAnglesInput)
{
	using namespace UE::DisplayCluster::Viewport::Math;

	// limit max frustum to 89
	static const double MaxFrustumAngle = FMath::Tan(FMath::DegreesToRadians(89));
	const double MaxValue = ZNear * MaxFrustumAngle;

	const double n = ZNear;
	const double f = ZFar;

	double t = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Top)))    : Top;
	double b = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Bottom))) : Bottom;
	double l = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Left)))   : Left;
	double r = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Right)))  : Right;

	// Protect PrjMatrix from bad input values, and fix\clamp FOV to limits
	{
		// Protect from broken input data, return valid matrix
		if (isnan(l) || isnan(r) || isnan(t) || isnan(b) || isnan(n) || isnan(f) || n <= 0)
		{
			return;
		}

		// Ignore inverted frustum
		if (l > r || b > t)
		{
			return;
		}

		// Clamp frustum values in range -89..89 degree
		l = FMath::Clamp(l, -MaxValue, MaxValue);
		r = FMath::Clamp(r, -MaxValue, MaxValue);
		t = FMath::Clamp(t, -MaxValue, MaxValue);
		b = FMath::Clamp(b, -MaxValue, MaxValue);
	}

	// Support custom frustum rendering
	const double OrigValues[] = {l, r, t, b};
	if (FDisplayClusterViewport_CustomFrustumRuntimeSettings::UpdateProjectionAngles(CustomFrustumRuntimeSettings, Contexts[InContextNum].RenderTargetRect.Size(), l, r, t, b))
	{
		const bool bIsValidLimits =  FMath::IsWithin(l, -MaxValue, MaxValue)
							&& FMath::IsWithin(r, -MaxValue, MaxValue)
							&& FMath::IsWithin(t, -MaxValue, MaxValue)
							&& FMath::IsWithin(b, -MaxValue, MaxValue);

		if (!bIsValidLimits)
		{
			// overscan out of frustum : disable
			CustomFrustumRuntimeSettings.bIsEnabled = false;

			// restore orig values
			l = OrigValues[0];
			r = OrigValues[1];
			t = OrigValues[2];
			b = OrigValues[3];
		}
	}

	GetNonZeroFrustumRange(l, r, n);
	GetNonZeroFrustumRange(b, t, n);

	Contexts[InContextNum].ProjectionMatrix = IDisplayClusterViewport::MakeProjectionMatrix(l, r, t, b, n, f);

	// Update cached projection data:
	FDisplayClusterViewport_Context::FCachedProjectionData& CachedProjectionData = Contexts[InContextNum].ProjectionData;
	CachedProjectionData.ProjectionAngles = FVector4(l, r, t, b);
	CachedProjectionData.ZNear = n;
	CachedProjectionData.ZFar = f;
	CachedProjectionData.bValid = true;

	if (FDisplayClusterViewport_OverscanRuntimeSettings::UpdateProjectionAngles(OverscanRuntimeSettings, Contexts[InContextNum].RenderTargetRect.Size(), l, r, t, b))
	{
		if (FMath::IsWithin(l, -MaxValue, MaxValue) &&
			FMath::IsWithin(r, -MaxValue, MaxValue) &&
			FMath::IsWithin(t, -MaxValue, MaxValue) &&
			FMath::IsWithin(b, -MaxValue, MaxValue)
			)
		{
			// Use overscan projection matrix
			Contexts[InContextNum].OverscanProjectionMatrix = IDisplayClusterViewport::MakeProjectionMatrix(l, r, t, b, n, f);

			// Cache projection data for overscan
			CachedProjectionData.bUseOverscan = true;
			CachedProjectionData.OverscanProjectionAngles = FVector4(l, r, t, b);

			return;
		}
	}

	// overscan out of frustum: disable
	OverscanRuntimeSettings.bIsEnabled = false;
}

///////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
FMatrix IDisplayClusterViewport::MakeProjectionMatrix(float l, float r, float t, float b, float n, float f)
{
	const float mx = 2.f * n / (r - l);
	const float my = 2.f * n / (t - b);
	const float ma = -(r + l) / (r - l);
	const float mb = -(t + b) / (t - b);

	// Support unlimited far plane (f==n)
	const float mc = (f == n) ? (1.0f - Z_PRECISION) : (f / (f - n));
	const float md = (f == n) ? (-n * (1.0f - Z_PRECISION)) : (-(f * n) / (f - n));

	const float me = 1.f;

	// Normal LHS
	const FMatrix ProjectionMatrix = FMatrix(
		FPlane(mx, 0, 0, 0),
		FPlane(0, my, 0, 0),
		FPlane(ma, mb, mc, me),
		FPlane(0, 0, md, 0));

	// Invert Z-axis (UE uses Z-inverted LHS)
	static const FMatrix flipZ = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0, 1, 1));

	const FMatrix ResultMatrix(ProjectionMatrix * flipZ);

	return ResultMatrix;
}
