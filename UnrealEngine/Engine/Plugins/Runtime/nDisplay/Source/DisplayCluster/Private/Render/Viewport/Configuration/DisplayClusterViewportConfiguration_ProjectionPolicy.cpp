// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfiguration_ProjectionPolicy.h"
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

namespace UE::DisplayCluster::Configuration::ProjectionPolicyHelpers
{
	template<class T>
	T* FindComponentByName(ADisplayClusterRootActor& InRootActor, const FString& InComponentName)
	{
		TArray<T*> ActorComps;
		InRootActor.GetComponents(ActorComps);
		for (T* CompIt : ActorComps)
		{
			if (CompIt->GetName() == InComponentName)
			{
				return CompIt;
			}
		}

		return nullptr;
	}
};
using namespace UE::DisplayCluster::Configuration;

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration_ProjectionPolicy
///////////////////////////////////////////////////////////////////
void FDisplayClusterViewportConfiguration_ProjectionPolicy::Update()
{
	if (FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			// ignore internal viewport
			if (ViewportIt.IsValid() && !ViewportIt->IsInternalViewport())
			{
				// Support advanced logic for 'camera' projection policy
				if (ViewportIt->GetProjectionPolicy().IsValid())
				{
					if (ViewportIt->GetProjectionPolicy()->GetType().Compare(DisplayClusterProjectionStrings::projection::Camera) == 0)
					{
						UpdateCameraPolicy(*ViewportIt);
					}

					// Projection policies can override postprocess settings
					// The camera PP override code has been moved from FDisplayClusterViewportConfiguration_ProjectionPolicy::UpdateCameraPolicy_Base() to the projection policy new function:
					ViewportIt->GetProjectionPolicy()->UpdatePostProcessSettings(ViewportIt.Get());
				}
			}
		}
	}
}

bool FDisplayClusterViewportConfiguration_ProjectionPolicy::UpdateCameraPolicy(FDisplayClusterViewport& DstViewport)
{
	const TMap<FString, FString>& CameraPolicyParameters = DstViewport.GetProjectionPolicy()->GetParameters();

	FString CameraComponentId;
	// Get assigned camera ID
	if (!DisplayClusterHelpers::map::template ExtractValue(CameraPolicyParameters, DisplayClusterProjectionStrings::cfg::camera::Component, CameraComponentId))
	{
		// use default cameras
		DstViewport.ResetShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::UpdateCameraPolicy);

		return true;
	}

	if (CameraComponentId.IsEmpty())
	{
		if (DstViewport.CanShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::UpdateCameraPolicy_ReferencedCameraNameIsEmpty))
		{
			UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Viewport '%s': referenced camera '' (empty name)."), *DstViewport.GetId());
		}

		return false;
	}

	// Get ICVFX camera component
	if (UpdateCameraPolicy_ICVFX(DstViewport, CameraComponentId))
	{
		DstViewport.ResetShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::UpdateCameraPolicy);

		return true;
	}

	// Get camera component
	if (UpdateCameraPolicy_Base(DstViewport, CameraComponentId))
	{
		DstViewport.ResetShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::UpdateCameraPolicy);

		return true;
	}

	if (DstViewport.CanShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::UpdateCameraPolicy_ReferencedCameraNotFound))
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("Viewport '%s': referenced camera '%s' not found."), *DstViewport.GetId(), *CameraComponentId);
	}

	return false;
}

bool FDisplayClusterViewportConfiguration_ProjectionPolicy::UpdateCameraPolicy_Base(FDisplayClusterViewport& DstViewport, const FString& CameraComponentId)
{
	ADisplayClusterRootActor* ConfigurationRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Configuration);
	ADisplayClusterRootActor*         SceneRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Scene);
	if (ConfigurationRootActor && SceneRootActor)
	{
		UCameraComponent*         SceneCameraComp = ProjectionPolicyHelpers::FindComponentByName<UCameraComponent>(*SceneRootActor, CameraComponentId);
		UCameraComponent* ConfigurationCameraComp = ProjectionPolicyHelpers::FindComponentByName<UCameraComponent>(*ConfigurationRootActor, CameraComponentId);
		if (SceneCameraComp && ConfigurationCameraComp)
		{
			// Now this parameter is always 1 for the standard camera component.
			const float FOVMultiplier = 1.f;

			// add camera's post processing materials
			// Moved to IDisplayClusterProjectionPolicy::UpdatePostProcessSettings()

			FDisplayClusterProjectionCameraPolicySettings PolicyCameraSettings;
			PolicyCameraSettings.FOVMultiplier = FOVMultiplier;

			// Initialize camera policy with camera component and settings
			return IDisplayClusterProjection::Get().CameraPolicySetCamera(DstViewport.GetProjectionPolicy(), SceneCameraComp, PolicyCameraSettings);
		}
	}

	return false;
}

bool FDisplayClusterViewportConfiguration_ProjectionPolicy::UpdateCameraPolicy_ICVFX(FDisplayClusterViewport& DstViewport, const FString& CameraComponentId)
{
	FDisplayClusterViewportManager*  ViewportManager = Configuration.GetViewportManagerImpl();
	ADisplayClusterRootActor* ConfigurationRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Configuration);
	ADisplayClusterRootActor*         SceneRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Scene);
	if (ConfigurationRootActor && SceneRootActor && ViewportManager)
	{
		UDisplayClusterICVFXCameraComponent*         SceneCameraComp = ProjectionPolicyHelpers::FindComponentByName<UDisplayClusterICVFXCameraComponent>(*SceneRootActor, CameraComponentId);
		UDisplayClusterICVFXCameraComponent* ConfigurationCameraComp = ProjectionPolicyHelpers::FindComponentByName<UDisplayClusterICVFXCameraComponent>(*ConfigurationRootActor, CameraComponentId);
		if (SceneCameraComp && ConfigurationCameraComp)
		{
			// If icvfx viewport already rendered, just copy to visible
			if (FDisplayClusterViewport* ExistCameraViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::FindCameraViewport(Configuration, *SceneCameraComp))
			{
				// Re-use camera RTT, and support OCIO:
				const EDisplayClusterViewportOverrideMode ViewportOverrideMode = DstViewport.IsOpenColorIOEquals(*ExistCameraViewport) ? EDisplayClusterViewportOverrideMode::All : EDisplayClusterViewportOverrideMode::InernalRTT;

				// Gain direct access to internal settings of the viewport:
				FDisplayClusterViewport_RenderSettings& InOutRenderSettings = DstViewport.GetRenderSettingsImpl();

				InOutRenderSettings.SetViewportOverride(ExistCameraViewport->GetId(), ViewportOverrideMode);
			}
			else
			{
				// Request render:
				// Save base settings from camera viewport
				const FDisplayClusterViewport_RenderSettings           SavedRenderSettings = DstViewport.GetRenderSettings();
				const FDisplayClusterViewport_RenderSettingsICVFX SavedRenderSettingsICVFX = DstViewport.GetRenderSettingsICVFX();

				// Update render settings for icvfx camera:
				FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportSettings(DstViewport, *SceneCameraComp, ConfigurationCameraComp->GetCameraSettingsICVFX());

				// Restore some base settings:
				DstViewport.GetRenderSettingsImpl() = SavedRenderSettings;
				DstViewport.GetRenderSettingsICVFXImpl() = SavedRenderSettingsICVFX;
			}

			return true;
		}
	}

	return false;
}
