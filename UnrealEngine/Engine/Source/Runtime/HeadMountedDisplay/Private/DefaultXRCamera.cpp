// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultXRCamera.h"
#include "GameFramework/PlayerController.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "Slate/SceneViewport.h"
#include "StereoRendering.h"
#include "StereoRenderTargetManager.h"
#include "IHeadMountedDisplay.h"

FDefaultXRCamera::FDefaultXRCamera(const FAutoRegister& AutoRegister, IXRTrackingSystem* InTrackingSystem, int32 InDeviceId)
	: FHMDSceneViewExtension(AutoRegister)
	, TrackingSystem(InTrackingSystem)
	, DeviceId(InDeviceId)
	, DeltaControlRotation(0, 0, 0)
	, DeltaControlOrientation(FQuat::Identity)
	, bUseImplicitHMDPosition(false)
{
}

void FDefaultXRCamera::ApplyHMDRotation(APlayerController* PC, FRotator& ViewRotation)
{
	ViewRotation.Normalize();
	FQuat DeviceOrientation;
	FVector DevicePosition;
	if ( TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition) )
	{
		const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
		DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

		// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
		// Same with roll.
		DeltaControlRotation.Pitch = 0;
		DeltaControlRotation.Roll = 0;
		DeltaControlOrientation = DeltaControlRotation.Quaternion();

		ViewRotation = FRotator(DeltaControlOrientation * DeviceOrientation);
	}
}

bool FDefaultXRCamera::UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	FQuat DeviceOrientation;
	FVector DevicePosition;
	if (!TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition))
	{
		return false;
	}

	if (GEnableVREditorHacks && !bUseImplicitHMDPosition)
	{
		DeltaControlOrientation = CurrentOrientation;
		DeltaControlRotation = DeltaControlOrientation.Rotator();
	}

	CurrentPosition = DevicePosition;
	CurrentOrientation = DeviceOrientation;

	return true;
}

void FDefaultXRCamera::OverrideFOV(float& InOutFOV)
{
	// The default camera does not override the FOV
}

void FDefaultXRCamera::SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component, bool bSkipLateUpdate)
{
	LateUpdate.Setup(ParentToWorld, Component, bSkipLateUpdate);
}

void FDefaultXRCamera::CalculateStereoCameraOffset(const int32 ViewIndex, FRotator& ViewRotation, FVector& ViewLocation)
{
	FQuat EyeOrientation;
	FVector EyeOffset;
	if (TrackingSystem->GetRelativeEyePose(DeviceId, ViewIndex, EyeOrientation, EyeOffset))
	{
		ViewLocation += ViewRotation.Quaternion().RotateVector(EyeOffset);
		ViewRotation = FRotator(ViewRotation.Quaternion() * EyeOrientation);
	}
	else if (ViewIndex == EStereoscopicEye::eSSE_MONOSCOPIC && TrackingSystem->GetHMDDevice())
	{
		float HFov, VFov;
		TrackingSystem->GetHMDDevice()->GetFieldOfView(HFov, VFov);
		if (HFov > 0.0f)
		{
			const float CenterOffset = GNearClippingPlane + (TrackingSystem->GetHMDDevice()->GetInterpupillaryDistance() / 2.0f) * (1.0f / FMath::Tan(FMath::DegreesToRadians(HFov)));
			ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(CenterOffset, 0, 0));
		}
	}
	else
	{
		return;
	}

	if (!bUseImplicitHMDPosition)
	{
		FQuat DeviceOrientation; // Unused
		FVector DevicePosition;
		TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition);
		ViewLocation += DeltaControlOrientation.RotateVector(DevicePosition);
	}
}

void FDefaultXRCamera::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View)
{
	check(IsInRenderingThread());

	// Disable late update for day dream, their compositor doesn't support it.
	// Also disable it if we are set to skip it.
	const bool bDoLateUpdate = !LateUpdate.GetSkipLateUpdate_RenderThread();
	if (bDoLateUpdate)
	{
		FQuat DeviceOrientation;
		FVector DevicePosition;

		if (TrackingSystem->DoesSupportLateProjectionUpdate() && TrackingSystem->GetStereoRenderingDevice())
		{
			View.UpdateProjectionMatrix(TrackingSystem->GetStereoRenderingDevice()->GetStereoProjectionMatrix(View.StereoViewIndex));
		}

		if (TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition))
		{
			const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * DeviceOrientation;
			View.ViewRotation = FRotator(View.ViewRotation.Quaternion() * DeltaOrient);

			if (bUseImplicitHMDPosition)
			{
				const FQuat LocalDeltaControlOrientation = View.ViewRotation.Quaternion() * DeviceOrientation.Inverse();
				const FVector DeltaPosition = DevicePosition - View.BaseHmdLocation;
				View.ViewLocation += LocalDeltaControlOrientation.RotateVector(DeltaPosition);
			}
		
			View.UpdateViewMatrix();
		}
	}
}

void FDefaultXRCamera::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());
	{
		// Backwards compatibility during deprecation phase. Remove once IHeadMountedDisplay::BeginRendering_GameThread has been removed.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		auto HMD = TrackingSystem->GetHMDDevice();
		if (HMD)
		{
			HMD->BeginRendering_GameThread();
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	TrackingSystem->OnBeginRendering_GameThread();
}

void FDefaultXRCamera::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	{
		// Backwards compatibility during deprecation phase. Remove once IXRTrackingSystem::RefreshPoses has been removed.
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TrackingSystem->RefreshPoses();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	TrackingSystem->OnBeginRendering_RenderThread(GraphBuilder.RHICmdList, ViewFamily);

	{
		FQuat CurrentOrientation;
		FVector CurrentPosition;
		if (TrackingSystem->DoesSupportLateUpdate() && TrackingSystem->GetCurrentPose(DeviceId, CurrentOrientation, CurrentPosition))
		{
			const FSceneView* MainView = ViewFamily.Views[0];
			check(MainView);

			// TODO: Should we (and do we have enough information to) double-check that the plugin actually has updated the pose here?
			// ensure((CurrentPosition != MainView->BaseHmdLocation && CurrentOrientation != MainView->BaseHmdOrientation) || CurrentPosition.IsZero() || CurrentOrientation.IsIdentity() );

			const FTransform OldRelativeTransform(MainView->BaseHmdOrientation, MainView->BaseHmdLocation);
			const FTransform CurrentRelativeTransform(CurrentOrientation, CurrentPosition);

			LateUpdate.Apply_RenderThread(ViewFamily.Scene, OldRelativeTransform, CurrentRelativeTransform);
			TrackingSystem->OnLateUpdateApplied_RenderThread(GraphBuilder.RHICmdList, CurrentRelativeTransform);

			{
				// Backwards compatibility during deprecation phase. Remove once IHeadMountedDisplay::BeginRendering_RenderThread has been removed.
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
					auto HMD = TrackingSystem->GetHMDDevice();
				if (HMD)
				{
					HMD->BeginRendering_RenderThread(CurrentRelativeTransform, GraphBuilder.RHICmdList, ViewFamily);
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}
	}
}

void FDefaultXRCamera::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	static const auto CVarAllowMotionBlurInVR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.AllowMotionBlurInVR"));
	const bool AllowMotionBlur = (CVarAllowMotionBlurInVR && CVarAllowMotionBlurInVR->GetValueOnAnyThread() != 0);
	const IHeadMountedDisplay* const HMD = TrackingSystem->GetHMDDevice();
	InViewFamily.EngineShowFlags.MotionBlur = AllowMotionBlur;
	InViewFamily.EngineShowFlags.HMDDistortion = HMD != nullptr ? HMD->GetHMDDistortionEnabled(InViewFamily.Scene->GetShadingPath()) : false;
	InViewFamily.EngineShowFlags.StereoRendering = bCurrentFrameIsStereoRendering;
	InViewFamily.EngineShowFlags.Rendering = HMD != nullptr ? !HMD->IsRenderingPaused() : true;
}

void FDefaultXRCamera::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	FQuat DeviceOrientation;
	FVector DevicePosition;

	if ( TrackingSystem->GetCurrentPose(DeviceId, DeviceOrientation, DevicePosition) )
	{
		InView.BaseHmdOrientation = DeviceOrientation;
		InView.BaseHmdLocation = DevicePosition;
	}
}

bool FDefaultXRCamera::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	bCurrentFrameIsStereoRendering = FHMDSceneViewExtension::IsActiveThisFrame_Internal(Context); // The current context/viewport might disallow stereo rendering. Save it so we'll use the correct value in SetupViewFamily.
	return bCurrentFrameIsStereoRendering && TrackingSystem->IsHeadTrackingAllowed();
}
