// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfiguration_ICVFX.h"

#include "DisplayClusterViewportConfiguration.h"
#include "DisplayClusterViewportConfigurationHelpers.h"
#include "DisplayClusterViewportConfigurationHelpers_ICVFX.h"
#include "DisplayClusterViewportConfigurationHelpers_Visibility.h"

#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

#include "IDisplayClusterProjection.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"
#include "DisplayClusterProjectionStrings.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/Parse.h"


////////////////////////////////////////////////////////////////////////////////
int32 GDisplayClusterEnableAlphaChannelRendering = 0;
static FAutoConsoleVariableRef CVarDisplayClusterEnableAlphaChannelRendering(
	TEXT("DC.EnableAlphaChannelRendering"),
	GDisplayClusterEnableAlphaChannelRendering,
	TEXT("Enable alpha channel rendering to backbuffer (0 == disabled, 1 == enabled)"),
	ECVF_RenderThreadSafe
);


///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration_ICVFX
///////////////////////////////////////////////////////////////////
bool FDisplayClusterViewportConfiguration_ICVFX::CreateLightcardViewport(FDisplayClusterViewport& BaseViewport)
{
	if (FDisplayClusterViewport* LightcardViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateLightcardViewport(BaseViewport))
	{
		// Update lightcard viewport settings
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(*LightcardViewport, BaseViewport);

		// Support projection policy update
		LightcardViewport->UpdateConfiguration_ProjectionPolicy();

		return true;
	}

	return false;
}

bool FDisplayClusterViewportConfiguration_ICVFX::CreateUVLightcardViewport(FDisplayClusterViewport& BaseViewport)
{
	if (FDisplayClusterViewport* UVLightCardViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateUVLightcardViewport(BaseViewport))
	{
		// Update UV LightCard viewport settings
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(*UVLightCardViewport, BaseViewport);

		// Support projection policy update
		UVLightCardViewport->UpdateConfiguration_ProjectionPolicy();

		// Optimize: re-use UVLightCard viewports with equals OCIO
		FDisplayClusterViewportConfigurationHelpers_ICVFX::ReuseUVLightCardViewportWithinClusterNode(*UVLightCardViewport);

		return true;
	}

	return false;
}

void FDisplayClusterViewportConfiguration_ICVFX::Update()
{
	ImplBeginReallocateViewports();

	const FDisplayClusterRenderFrameSettings& RenderFrameSettings = Configuration.GetRenderFrameSettings();
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = Configuration.GetStageSettings();
	FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl();

	// Disable ICVFX if cluster node rendering is not used
	const FString& ClusterNodeId = RenderFrameSettings.ClusterNodeId;
	if (!(ViewportManager && StageSettings && !ClusterNodeId.IsEmpty()))
	{
		// The nDisplay viewport should now always have the name of the cluster node.
		// When rendering MRQ viewports, the list of viewports is used without cluster node names.
		ImplFinishReallocateViewports();

		return;
	}

	// Find ICVFX target viewports
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> TargetViewports;
	EDisplayClusterViewportICVFXFlags TargetViewportsFlags;
	if (!ImplGetTargetViewports(TargetViewports, TargetViewportsFlags))
	{
		// We need to be able to render inner views (ICVFX camera view) without any outers. Current pipeline has
		// a bunch of optimizations that block inner rendering if no outers available. To avoid the limitation
		// we force inner data initialization if no outers found.
		static const bool bIsRenderingOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));

		// Cherry-pick ICVFX cameras initialization from IF-block above
		if (bIsRenderingOffscreen)
		{
			GetAndUpdateStageCameras();
		}

		ImplFinishReallocateViewports();

		return;
	}

	// If not all of the viewports disable camera:
	// Collect all ICVFX cameras from stage
	if (!EnumHasAnyFlags(TargetViewportsFlags, EDisplayClusterViewportICVFXFlags::DisableCamera))
	{
		GetAndUpdateStageCameras(&TargetViewports);
	}

	// If not all viewports disable lightcard
	if (!EnumHasAnyFlags(TargetViewportsFlags, EDisplayClusterViewportICVFXFlags::DisableLightcard))
	{
		// UVLightCard must be enabled in LC manager
		const bool bUVLightCardEnabled = ViewportManager->LightCardManager->IsUVLightCardEnabled();

		// Allocate and assign lightcard resources
		const bool bUseLightCard = StageSettings->Lightcard.ShouldUseLightCard(*StageSettings);
		const bool bUseUVLightCard = bUVLightCardEnabled && StageSettings->Lightcard.ShouldUseUVLightCard(*StageSettings);

		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : TargetViewports)
		{
			// only for support targets
			if (TargetIt.IsValid() && !EnumHasAnyFlags(TargetIt->GetRenderSettingsICVFX().Flags, EDisplayClusterViewportICVFXFlags::DisableLightcard))
			{
				if (bUseLightCard)
				{
					CreateLightcardViewport(*TargetIt);
				}

				if (bUseUVLightCard)
				{
					CreateUVLightcardViewport(*TargetIt);
				}
			}
		}
	}

	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : TargetViewports)
	{
		if (TargetIt.IsValid())
		{
			// Gain direct access to internal settings of the viewport:
			FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = TargetIt->GetRenderSettingsICVFXImpl();

			// Sort cameras by render order for each target
			InOutRenderSettingsICVFX.ICVFX.SortCamerasRenderOrder();

			// Setup incamera overlap mode
			InOutRenderSettingsICVFX.ICVFX.CameraOverlappingRenderMode = StageSettings->GetCameraOverlappingRenderMode();
		}
	}

	if (StageSettings->bFreezeRenderOuterViewports || RenderFrameSettings.IsPreviewFreezeRender())
	{
		// Freeze rendering for outer viewports
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : TargetViewports)
		{
			if (TargetIt.IsValid())
			{
				// Gain direct access to internal settings of the viewport:
				FDisplayClusterViewport_RenderSettings& InOutRenderSettings = TargetIt->GetRenderSettingsImpl();

				InOutRenderSettings.bFreezeRendering = true;
			}
		}

		// Freeze render for lightcards when outer viewports freezed
		if (StageSettings->Lightcard.bIgnoreOuterViewportsFreezingForLightcards == false)
		{
			for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
			{
				if (ViewportIt.IsValid() && EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
				{
					// Gain direct access to internal settings of the viewport:
					FDisplayClusterViewport_RenderSettings& InOutRenderSettings = ViewportIt->GetRenderSettingsImpl();

					InOutRenderSettings.bFreezeRendering = true;
				}
			}
		}
	}

	ImplFinishReallocateViewports();
}

void FDisplayClusterViewportConfiguration_ICVFX::PostUpdate()
{
	if(FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl())
	{
		// Update visibility for icvfx viewports and cameras
		ImplUpdateVisibility();

		// Support alpha channel rendering for WarpBlend
		if (GDisplayClusterEnableAlphaChannelRendering != 0)
		{
			for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
			{
				if (ViewportIt.IsValid())
				{
					// Gain direct access to internal settings of the viewport:
					FDisplayClusterViewport_RenderSettings& InOutRenderSettings = ViewportIt->GetRenderSettingsImpl();

					InOutRenderSettings.bWarpBlendRenderAlphaChannel = true;
				}
			}
		}
	}
}

void FDisplayClusterViewportConfiguration_ICVFX::ImplUpdateVisibility()
{
	// Collect viewports, that use ICVFX hide list
	if (FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl())
	{
		TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> ICVFXViewports;

		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid())
			{
				const bool bInternalResource = EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource);
				const bool bIsInCamera = EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera);
				const bool bICVFX_Enable = EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().Flags, EDisplayClusterViewportICVFXFlags::Enable);

				if ((bICVFX_Enable && !bInternalResource) || (bInternalResource && bIsInCamera))
				{
					ICVFXViewports.Add(ViewportIt);
				}
			}
		}

		// Update hide list for all icvfx viewports
		FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateHideList_ICVFX(Configuration, ICVFXViewports);
	}

	// Support additional hide list for icvfx cameras:
	for (const FDisplayClusterViewportConfiguration_ICVFXCamera& CameraIt : StageCameras)
	{
		if (CameraIt.CameraViewport.IsValid())
		{
			FDisplayClusterViewportConfigurationHelpers_Visibility::AppendHideList_ICVFX(*CameraIt.CameraViewport, CameraIt.GetCameraSettings().CameraHideList);
		}
	}
}

void FDisplayClusterViewportConfiguration_ICVFX::ImplBeginReallocateViewports()
{
	if (FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			// Runtime icvfx viewport support reallocate feature:
			if (ViewportIt.IsValid() && EnumHasAllFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
			{
				// Gain direct access to internal settings of the viewport:
				FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = ViewportIt->GetRenderSettingsICVFXImpl();

				// Mark all dynamic ICVFX viewports for delete
				EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);
			}
		}
	}
}

void FDisplayClusterViewportConfiguration_ICVFX::ImplFinishReallocateViewports()
{
	if (FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl())
	{
		TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> UnusedViewports;

		// Collect all unused viewports for remove
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid() && EnumHasAllFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused))
			{
				UnusedViewports.Add(ViewportIt);
			}
		}

		// Delete unused viewports:
		for (TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& RemoveViewportIt : UnusedViewports)
		{
			if (RemoveViewportIt.IsValid())
			{
				ViewportManager->DeleteViewport(RemoveViewportIt->GetId());
			}
		}

		UnusedViewports.Empty();
	}
}

void FDisplayClusterViewportConfiguration_ICVFX::GetAndUpdateStageCameras(const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>* InTargetViewports)
{
	// Allocate and assign camera resources
	if (ImplGetStageCameras())
	{
		// Collect visible targets for cameras:
		if (InTargetViewports)
		{
			for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : *InTargetViewports)
			{
				// Target viewpot must support camera render:
				if (TargetIt.IsValid() && !EnumHasAnyFlags(TargetIt->GetRenderSettingsICVFX().Flags, EDisplayClusterViewportICVFXFlags::DisableCamera))
				{
					// Add this target to all cameras visible on it
					for (FDisplayClusterViewportConfiguration_ICVFXCamera& CameraIt : StageCameras)
					{
						if (CameraIt.IsCameraProjectionVisibleOnViewport(TargetIt.Get())
							&& !CameraIt.GetCameraSettings().HiddenICVFXViewports.ItemNames.Contains(TargetIt->GetId()))
						{
							CameraIt.VisibleTargets.Add(TargetIt);
						}
					}
				}
			}
		}

		// Create camera resources and initialize target ICVFX viewports
		for (FDisplayClusterViewportConfiguration_ICVFXCamera& CameraIt : StageCameras)
		{
			if (InTargetViewports == nullptr || CameraIt.VisibleTargets.Num() > 0)
			{
				CameraIt.Update();
			}
		}
	}
}

bool FDisplayClusterViewportConfiguration_ICVFX::ImplGetStageCameras()
{
	ADisplayClusterRootActor* SceneRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Scene);
	ADisplayClusterRootActor* ConfigurationRootActor = Configuration.GetRootActor(EDisplayClusterRootActorType::Configuration);
	const UDisplayClusterConfigurationData* ConfigurationData = Configuration.GetConfigurationData();
	if (!(SceneRootActor && ConfigurationRootActor && ConfigurationData))
	{
		return false;
	}

	// Get all ICVFX camera components
	TArray<UDisplayClusterICVFXCameraComponent*> SceneCameraComponents;
	SceneRootActor->GetComponents(SceneCameraComponents);

	TArray<UDisplayClusterICVFXCameraComponent*> ConfigurationCameraComponents;
	ConfigurationRootActor->GetComponents(ConfigurationCameraComponents);

	// Filter active cameras only
	for (UDisplayClusterICVFXCameraComponent* SceneCameraComponentIt : SceneCameraComponents)
	{
		if (SceneCameraComponentIt)
		{
			UDisplayClusterICVFXCameraComponent** ConfigurationCameraPtr = ConfigurationCameraComponents.FindByPredicate([CameraId = SceneCameraComponentIt->GetCameraUniqueId()](const UDisplayClusterICVFXCameraComponent* CameraComponent)
				{
					return CameraComponent && (CameraComponent->GetCameraUniqueId() == CameraId);
				});

			if (ConfigurationCameraPtr && *ConfigurationCameraPtr)
			{
				const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = (*ConfigurationCameraPtr)->GetCameraSettingsICVFX();
				if (CameraSettings.IsICVFXEnabled(*ConfigurationData, Configuration.GetClusterNodeId()))
				{
					const FString InnerFrustumID = (*ConfigurationCameraPtr)->GetCameraUniqueId();
					if (ConfigurationRootActor->IsInnerFrustumEnabled(InnerFrustumID))
					{
						FDisplayClusterViewportConfiguration_ICVFXCamera NewCamera(Configuration, *SceneCameraComponentIt , *(*ConfigurationCameraPtr));
						if (NewCamera.Initialize())
						{
							StageCameras.Add(NewCamera);
						}
					}
				}
			}
		}
	}

	return StageCameras.Num() > 0;
}

bool FDisplayClusterViewportConfiguration_ICVFX::ImplGetTargetViewports(TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& OutTargets, EDisplayClusterViewportICVFXFlags& OutMergedICVFXFlags)
{
	EDisplayClusterViewportICVFXFlags InvFlags = EDisplayClusterViewportICVFXFlags::None;

	if (FDisplayClusterViewportManager* ViewportManager = Configuration.GetViewportManagerImpl())
	{
		// Collect invertet disable flags from all target viewports
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			// Process only external viewports:
			if (ViewportIt.IsValid() && !ViewportIt->IsInternalViewport())
			{
				//Raise new projection target if possible
				if (ViewportIt->GetRenderSettings().bEnable && EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().Flags, EDisplayClusterViewportICVFXFlags::Enable))
				{
					if (ViewportIt->GetProjectionPolicy().IsValid() && ViewportIt->GetProjectionPolicy()->ShouldSupportICVFX(ViewportIt.Get()))
					{
						// Collect this viewport ICVFX target
						OutTargets.Add(ViewportIt);

						// Gain direct access to internal settings of the viewport:
						FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = ViewportIt->GetRenderSettingsICVFXImpl();

						// proj policy support ICVFX, Use this viewport as icvfx target
						EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target);

						// Update targets use flags:
						InvFlags |= ~(InOutRenderSettingsICVFX.Flags);
					}
				}
			}
		}
	}

	// Collect all targets disable flags
	OutMergedICVFXFlags = ~(InvFlags);

	return OutTargets.Num() > 0;
}
