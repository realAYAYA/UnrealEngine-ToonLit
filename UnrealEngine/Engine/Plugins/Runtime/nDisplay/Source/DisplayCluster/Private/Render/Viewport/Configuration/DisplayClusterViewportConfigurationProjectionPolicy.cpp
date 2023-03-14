// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationProjectionPolicy.h"
#include "DisplayClusterViewportConfigurationHelpers_ICVFX.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "DisplayClusterViewportConfigurationHelpers.h"
#include "DisplayClusterViewportConfigurationHelpers_Postprocess.h"
#include "DisplayClusterConfigurationTypes.h"

#include "IDisplayClusterProjection.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "DisplayClusterProjectionStrings.h"

#include "DisplayClusterRootActor.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationBase
///////////////////////////////////////////////////////////////////

void FDisplayClusterViewportConfigurationProjectionPolicy::Update()
{
	for (FDisplayClusterViewport* ViewportIt : ViewportManager.ImplGetViewports())
	{
		// ignore ICVFX internal resources
		if (ViewportIt && (ViewportIt->GetRenderSettingsICVFX().RuntimeFlags & ViewportRuntime_InternalResource) == 0)
		{
			// Support advanced logic for 'camera' projection policy
			if (ViewportIt->ProjectionPolicy.IsValid() && ViewportIt->ProjectionPolicy->GetType().Compare(DisplayClusterProjectionStrings::projection::Camera) == 0)
			{
				UpdateCameraPolicy(*ViewportIt);
			}
		}
	}
}

bool FDisplayClusterViewportConfigurationProjectionPolicy::UpdateCameraPolicy(FDisplayClusterViewport& DstViewport)
{
	const TMap<FString, FString>& CameraPolicyParameters = DstViewport.ProjectionPolicy->GetParameters();

	FString CameraComponentId;
	// Get assigned camera ID
	if (!DisplayClusterHelpers::map::template ExtractValue(CameraPolicyParameters, DisplayClusterProjectionStrings::cfg::camera::Component, CameraComponentId))
	{
		// use default cameras
		return true;
	}

	if (CameraComponentId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Viewport '%s': referenced camera '' (empty name)."), *DstViewport.GetId());
		return false;
	}

	// Get ICVFX camera component
	TArray<UDisplayClusterICVFXCameraComponent*> ICVFXCameraComps;
	RootActor.GetComponents(ICVFXCameraComps);
	for (UDisplayClusterICVFXCameraComponent* CompICVFX : ICVFXCameraComps)
	{
		if (CompICVFX->GetName() == CameraComponentId)
		{
			return UpdateCameraPolicy_ICVFX(DstViewport, CompICVFX);
		}
	}

	// Get camera component
	TArray<UCameraComponent*> CameraComps;
	RootActor.GetComponents(CameraComps);
	for (UCameraComponent* Comp : CameraComps)
	{
		if (Comp->GetName() == CameraComponentId)
		{
			// Now this parameter is always 1 for the standard camera component.
			const float FOVMultiplier = 1.f;

			return UpdateCameraPolicy_Base(DstViewport, Comp, FOVMultiplier);
		}
	}

	UE_LOG(LogDisplayClusterViewport, Error, TEXT("Viewport '%s': referenced camera '%s' not found."), *DstViewport.GetId(), *CameraComponentId);
	return false;
}

bool FDisplayClusterViewportConfigurationProjectionPolicy::UpdateCameraPolicy_Base(FDisplayClusterViewport& DstViewport, UCameraComponent* const InCameraComponent, float FOVMultiplier)
{
	check(InCameraComponent);

	// add camera's post processing materials
	FMinimalViewInfo DesiredView;
	const float DeltaTime = RootActor.GetWorldDeltaSeconds();
	InCameraComponent->GetCameraView(DeltaTime, DesiredView);

	DstViewport.CustomPostProcessSettings.AddCustomPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override, DesiredView.PostProcessSettings, 1.0, true);

	FDisplayClusterProjectionCameraPolicySettings PolicyCameraSettings;
	PolicyCameraSettings.FOVMultiplier = FOVMultiplier;

	// Initialize camera policy with camera component and settings
	return IDisplayClusterProjection::Get().CameraPolicySetCamera(DstViewport.ProjectionPolicy, InCameraComponent, PolicyCameraSettings);
}

bool FDisplayClusterViewportConfigurationProjectionPolicy::UpdateCameraPolicy_ICVFX(FDisplayClusterViewport& DstViewport, UDisplayClusterICVFXCameraComponent* const InICVFXCameraComponent)
{
	check(InICVFXCameraComponent);

	// If icvfx viewport already rendered, just copy to visible
	FDisplayClusterViewport* ExistCameraViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::FindCameraViewport(RootActor, *InICVFXCameraComponent);
	if (ExistCameraViewport)
	{
		DstViewport.RenderSettings.OverrideViewportId = ExistCameraViewport->GetId();
	}
	else
	{
		// Request render:
		// Save base settings from camera viewport
		const FDisplayClusterViewport_RenderSettings SavedRenderSettings = DstViewport.RenderSettings;
		const FDisplayClusterViewport_RenderSettingsICVFX SavedRenderSettingsICVFX = DstViewport.RenderSettingsICVFX;

		// Update render settings for icvfx camera:
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportSettings(DstViewport, RootActor, *InICVFXCameraComponent);

		// Restore some base settings:
		DstViewport.RenderSettings = SavedRenderSettings;
		DstViewport.RenderSettingsICVFX = SavedRenderSettingsICVFX;
	}

	return true;
}
