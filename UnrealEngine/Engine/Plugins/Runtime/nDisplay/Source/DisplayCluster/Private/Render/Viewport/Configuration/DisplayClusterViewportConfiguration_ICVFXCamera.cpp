// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfiguration_ICVFXCamera.h"

#include "DisplayClusterViewportConfiguration.h"
#include "DisplayClusterViewportConfigurationHelpers.h"
#include "DisplayClusterViewportConfigurationHelpers_ICVFX.h"
#include "DisplayClusterViewportConfigurationHelpers_Visibility.h"

#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"

#include "IDisplayClusterProjection.h"
#include "DisplayClusterProjectionStrings.h"

#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/Parse.h"

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration_ICVFXCamera
////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterViewportConfiguration_ICVFXCamera::CreateAndSetupInnerCameraViewport()
{
	if (FDisplayClusterViewport* NewCameraViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateCameraViewport(Configuration, CameraComponent, GetCameraSettings()))
	{
		CameraViewport = NewCameraViewport->AsShared();

		// overlay rendered only for enabled incamera
		check(GetCameraSettings().bEnable);

		// Update camera viewport settings
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportSettings(*CameraViewport, CameraComponent, GetCameraSettings());

		// Support projection policy update
		CameraViewport->UpdateConfiguration_ProjectionPolicy();

		// Reuse for EditorPreview
		FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseInnerFrustumViewportWithinClusterNodes(*CameraViewport, CameraComponent, GetCameraSettings());

		return true;
	}

	return false;
}

bool FDisplayClusterViewportConfiguration_ICVFXCamera::IsCameraProjectionVisibleOnViewport(FDisplayClusterViewport* TargetViewport)
{
	if (TargetViewport && TargetViewport->GetProjectionPolicy().IsValid())
	{
		// Currently, only mono context is supported to check the visibility of the inner camera.
		if (TargetViewport->GetProjectionPolicy()->IsCameraProjectionVisible(CameraContext.ViewRotation, CameraContext.ViewLocation, CameraContext.PrjMatrix))
		{
			return true;
		}
	}

	// do not use camera for this viewport
	return false;
}

void FDisplayClusterViewportConfiguration_ICVFXCamera::Update()
{
	if (CreateAndSetupInnerCameraViewport())
	{
		if (CameraViewport.IsValid())
		{
			FDisplayClusterShaderParameters_ICVFX::FCameraSettings ShaderParametersCameraSettings = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetShaderParametersCameraSettings(*CameraViewport, CameraComponent, GetCameraSettings());

			// Add this camera data to all visible targets:
			for (TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : VisibleTargets)
			{
				if (ViewportIt.IsValid())
				{
					// Gain direct access to internal settings of the viewport:
					FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = ViewportIt->GetRenderSettingsICVFXImpl();

					InOutRenderSettingsICVFX.ICVFX.Cameras.Add(ShaderParametersCameraSettings);
				}
			}
		}

		// Create and assign chromakey for all targets for this camera
		CreateAndSetupInnerCameraChromakey();
	}
}

bool FDisplayClusterViewportConfiguration_ICVFXCamera::Initialize()
{
	// Create new camera projection policy for camera viewport
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> CameraProjectionPolicy;
	if (!FDisplayClusterViewportConfigurationHelpers_ICVFX::CreateProjectionPolicyCameraICVFX(Configuration, CameraComponent, GetCameraSettings(), CameraProjectionPolicy))
	{
		return false;
	}

	ADisplayClusterRootActor* SceneRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Scene);
	if (!SceneRootActor)
	{
		return false;
	}

	// Applying the correct sequence of steps to use the projection policy math:
	// SetupProjectionViewPoint()->CalculateView()->GetProjectionMatrix()
	FMinimalViewInfo CameraViewInfo;
	float CustomNearClippingPlane = -1; // a value less than zero means ignoring.
	CameraProjectionPolicy->SetupProjectionViewPoint(nullptr, SceneRootActor->GetWorldDeltaSeconds(), CameraViewInfo, &CustomNearClippingPlane);

	CameraContext.ViewLocation = CameraViewInfo.Location;
	CameraContext.ViewRotation = CameraViewInfo.Rotation;

	// Todo: Here we need to calculate the correct ViewOffset so that ICVFX can support stereo rendering.
	const FVector ViewOffset = FVector::ZeroVector;

	// Get world scale
	const float WorldToMeters = Configuration.GetWorldToMeters();
	// Supports custom near clipping plane
	const float NCP = (CustomNearClippingPlane >= 0) ? CustomNearClippingPlane : GNearClippingPlane;

	if(CameraProjectionPolicy->CalculateView(nullptr, 0, CameraContext.ViewLocation, CameraContext.ViewRotation, ViewOffset, WorldToMeters, NCP, NCP)
	&& CameraProjectionPolicy->GetProjectionMatrix(nullptr, 0, CameraContext.PrjMatrix))
	{
		return true;
	}

	return false;
}

const FDisplayClusterConfigurationICVFX_CameraSettings& FDisplayClusterViewportConfiguration_ICVFXCamera::GetCameraSettings() const
{
	return ConfigurationCameraComponent.GetCameraSettingsICVFX();
}

bool FDisplayClusterViewportConfiguration_ICVFXCamera::ImplCreateChromakeyViewport()
{
	check(CameraViewport.IsValid());

	const FString ICVFXCameraId = CameraComponent.GetCameraUniqueId();

	// Create new chromakey viewport
	if (FDisplayClusterViewport* NewChromakeyViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateChromakeyViewport(Configuration, CameraComponent, GetCameraSettings()))
	{
		ChromakeyViewport = NewChromakeyViewport->AsShared();

		// Update chromakey viewport settings
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateChromakeyViewportSettings(*ChromakeyViewport, *CameraViewport, GetCameraSettings());

		// Support projection policy update
		ChromakeyViewport->UpdateConfiguration_ProjectionPolicy();

		// reuse for EditorPreview
		FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseChromakeyViewportWithinClusterNodes(*ChromakeyViewport, ICVFXCameraId);

		return true;
	}

	return false;
}

bool FDisplayClusterViewportConfiguration_ICVFXCamera::CreateAndSetupInnerCameraChromakey()
{
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = Configuration.GetStageSettings();
	if (!StageSettings)
	{
		return false;
	}

	// Try create chromakey render on demand
	if (const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* ChromakeyRenderSettings = GetCameraSettings().Chromakey.GetChromakeyRenderSettings(*StageSettings))
	{
		if (ChromakeyRenderSettings->ShouldUseChromakeyViewport(*StageSettings))
		{
			ImplCreateChromakeyViewport();
		}
	}

	// Chromakey viewport name with alpha channel
	const FString ChromakeyViewportId(ChromakeyViewport.IsValid() ? ChromakeyViewport->GetId() : TEXT(""));

	// Assign this chromakey to all supported targets
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& OuterViewportIt : VisibleTargets)
	{
		if (OuterViewportIt.IsValid())
		{
			const bool bEnableChromakey = !EnumHasAnyFlags(OuterViewportIt->GetRenderSettingsICVFX().Flags, EDisplayClusterViewportICVFXFlags::DisableChromakey);
			const bool bEnableChromakeyMarkers = !EnumHasAnyFlags(OuterViewportIt->GetRenderSettingsICVFX().Flags, EDisplayClusterViewportICVFXFlags::DisableChromakeyMarkers);

			// Gain direct access to internal settings of the viewport:
			FDisplayClusterViewport_RenderSettingsICVFX& InOutOuterViewportRenderSettingsICVFX = OuterViewportIt->GetRenderSettingsICVFXImpl();
			FDisplayClusterShaderParameters_ICVFX::FCameraSettings& DstCameraData = InOutOuterViewportRenderSettingsICVFX.ICVFX.Cameras.Last();

			if (bEnableChromakey)
			{
				// Setup chromakey with markers
				FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_Chromakey(DstCameraData, *StageSettings, GetCameraSettings(), bEnableChromakeyMarkers, ChromakeyViewportId);
			}

			// Setup overlap chromakey with markers
			FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_OverlapChromakey(DstCameraData, *StageSettings, GetCameraSettings(), bEnableChromakeyMarkers);
		}
	}

	return true;
}
