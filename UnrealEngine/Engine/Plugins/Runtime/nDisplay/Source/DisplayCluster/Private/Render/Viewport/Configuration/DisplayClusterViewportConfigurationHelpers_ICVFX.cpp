// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_ICVFX.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_OpenColorIO.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Postprocess.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Visibility.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Tile.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "DisplayClusterRootActor.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes_PostRender.h"

#include "IDisplayCluster.h"
#include "Cluster/IDisplayClusterClusterManager.h"

#include "ShaderParameters/DisplayClusterShaderParameters_PostprocessBlur.h"
#include "ShaderParameters/DisplayClusterShaderParameters_GenerateMips.h"
#include "ShaderParameters/DisplayClusterShaderParameters_Override.h"
#include "ShaderParameters/DisplayClusterShaderParameters_ICVFX.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettingsICVFX.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"
#include "DisplayClusterProjectionStrings.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Misc/DisplayClusterLog.h"
#include "TextureResource.h"
#include "HAL/IConsoleManager.h"

////////////////////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::Viewport::ConfigurationHelpers_ICVFX
{
	// Return unique ICVFX name
	static FString ImplGetNameICVFX(const FString& InClusterNodeId, const FString& InViewportId, const FString& InResourceId)
	{
		check(!InClusterNodeId.IsEmpty());
		check(!InViewportId.IsEmpty());
		check(!InResourceId.IsEmpty());

		return FString::Printf(TEXT("%s_%s_%s_%s"), *InClusterNodeId, DisplayClusterViewportStrings::icvfx::prefix, *InViewportId, *InResourceId);
	}
};
using namespace UE::DisplayCluster::Viewport::ConfigurationHelpers_ICVFX;

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationHelpers_ICVFX
////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterViewportConfigurationHelpers_ICVFX::CreateProjectionPolicyICVFX(FDisplayClusterViewportConfiguration& InConfiguration, const FString& InViewportId, const FString& InResourceId, bool bIsCameraProjection, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& OutProjPolicy)
{
	const FString& ClusterNodeId = InConfiguration.GetClusterNodeId();
	if (ClusterNodeId.IsEmpty())
	{
		return false;
	}

	FDisplayClusterConfigurationProjection CameraProjectionPolicyConfig;
	CameraProjectionPolicyConfig.Type = bIsCameraProjection ? DisplayClusterProjectionStrings::projection::Camera : DisplayClusterProjectionStrings::projection::Link;

	// Create projection policy for viewport
	OutProjPolicy = FDisplayClusterViewportManager::CreateProjectionPolicy(ImplGetNameICVFX(ClusterNodeId, InViewportId, InResourceId), &CameraProjectionPolicyConfig);

	if (!OutProjPolicy.IsValid())
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("ICVFX Viewport '%s': projection policy for resource '%s' not created for node '%s'."), *InViewportId, *InResourceId, *ClusterNodeId);
		return false;
	}

	return true;
}

bool FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraProjectionSettingsICVFX(FDisplayClusterViewportConfiguration& InConfiguration, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
{
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = InConfiguration.GetStageSettings();
	if (!StageSettings)
	{
		return false;
	}

	// Initialize camera policy with camera component and settings
	FDisplayClusterProjectionCameraPolicySettings PolicyCameraSettings;
	PolicyCameraSettings.FOVMultiplier = InCameraSettings.CustomFrustum.GetCameraFieldOfViewMultiplier(*StageSettings);

	// Lens correction
	PolicyCameraSettings.FrustumRotation = InCameraSettings.FrustumRotation;
	PolicyCameraSettings.FrustumOffset = InCameraSettings.FrustumOffset;
	PolicyCameraSettings.OffCenterProjectionOffset = InCameraSettings.OffCenterProjectionOffset;

	static IDisplayClusterProjection& DisplayClusterProjectionAPI = IDisplayClusterProjection::Get();

	// Initialize camera policy with camera component and settings
	return DisplayClusterProjectionAPI.CameraPolicySetCamera(InProjectionPolicy, &InCameraComponent, PolicyCameraSettings);
}

bool FDisplayClusterViewportConfigurationHelpers_ICVFX::CreateProjectionPolicyCameraICVFX(FDisplayClusterViewportConfiguration& InConfiguration, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& OutProjPolicy)
{
	return CreateProjectionPolicyICVFX(InConfiguration, InCameraComponent.GetCameraUniqueId(), DisplayClusterViewportStrings::icvfx::camera, true, OutProjPolicy)
		&& UpdateCameraProjectionSettingsICVFX(InConfiguration, InCameraComponent, InCameraSettings, OutProjPolicy);
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::FindViewportICVFX(FDisplayClusterViewportConfiguration& InConfiguration, const FString& InViewportId, const FString& InResourceId)
{
	if (FDisplayClusterViewportManager* ViewportManager = InConfiguration.GetViewportManagerImpl())
	{
		TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> Viewport = ViewportManager->ImplFindViewport(ImplGetNameICVFX(InConfiguration.GetClusterNodeId(), InViewportId, InResourceId));

		return Viewport.Get();
	}

	return nullptr;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::CreateViewportICVFX(FDisplayClusterViewportConfiguration& InConfiguration, const FString& InViewportId, const FString& InResourceId, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
{
	check(InProjectionPolicy.IsValid());

	if (FDisplayClusterViewportManager* ViewportManager = InConfiguration.GetViewportManagerImpl())
	{
		// Create viewport for new projection policy
		TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> NewViewport = ViewportManager->ImplCreateViewport(ImplGetNameICVFX(InConfiguration.GetClusterNodeId(), InViewportId, InResourceId), InProjectionPolicy);
		if (NewViewport.IsValid())
		{
			// Gain direct access to internal resources of the NewViewport:
			FDisplayClusterViewport_RenderSettings&           InOutRenderSettings = NewViewport->GetRenderSettingsImpl();
			FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = NewViewport->GetRenderSettingsICVFXImpl();

			// Mark as internal resource
			EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource);

			// Dont show ICVFX composing viewports on frame target
			InOutRenderSettings.bVisible = false;

			return NewViewport.Get();
		}
	}

	return nullptr;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::FindCameraViewport(FDisplayClusterViewportConfiguration& InConfiguration, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	return FindViewportICVFX(InConfiguration, InCameraComponent.GetCameraUniqueId(), DisplayClusterViewportStrings::icvfx::camera);
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateCameraViewport(FDisplayClusterViewportConfiguration& InConfiguration, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	FDisplayClusterViewport* CameraViewport = FindCameraViewport(InConfiguration, InCameraComponent);
	if (CameraViewport == nullptr)
	{
		// Create new camera viewport
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> CameraProjectionPolicy;
		if (!CreateProjectionPolicyCameraICVFX(InConfiguration, InCameraComponent, InCameraSettings, CameraProjectionPolicy))
		{
			return nullptr;
		}

		CameraViewport = CreateViewportICVFX(InConfiguration, InCameraComponent.GetCameraUniqueId(), DisplayClusterViewportStrings::icvfx::camera, CameraProjectionPolicy);
		if (CameraViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Gain direct access to internal resources of the CameraViewport:
	FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = CameraViewport->GetRenderSettingsICVFXImpl();

	// Mark viewport as used
	EnumRemoveFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Incamera
	EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera);

	return CameraViewport;
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::ReuseUVLightCardViewportWithinClusterNode(FDisplayClusterViewport& InUVLightCardViewport)
{
	if (FDisplayClusterViewportManager* ViewportManager = InUVLightCardViewport.Configuration->GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetEntireClusterViewports())
		{
			if (ViewportIt.IsValid() && ViewportIt != InUVLightCardViewport.AsShared() && !ViewportIt->GetRenderSettings().IsViewportOverridden()
				&& EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
			{
				if (ViewportIt->IsOpenColorIOEquals(InUVLightCardViewport))
				{
					// Gain direct access to internal resources of the viewport:
					FDisplayClusterViewport_RenderSettings& InOutRenderSettings = InUVLightCardViewport.GetRenderSettingsImpl();

					// Reuse exist viewport:
					InOutRenderSettings.SetViewportOverride(ViewportIt->GetId(), EDisplayClusterViewportOverrideMode::All);
					break;
				}
			}
		}
	}
}

TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> FDisplayClusterViewportConfigurationHelpers_ICVFX::GetAllVisibleInnerCameraViewports(FDisplayClusterViewportConfiguration& InConfiguration, bool bGetChromakey)
{
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> OutViewports;

	ADisplayClusterRootActor* ConfigurationRootActor = InConfiguration.GetRootActor(EDisplayClusterRootActorType::Configuration);
	const UDisplayClusterConfigurationData* ConfigurationData = InConfiguration.GetConfigurationData();
	if (ConfigurationRootActor && ConfigurationData)
	{
		TArray<UDisplayClusterICVFXCameraComponent*> ExistsICVFXCameraComponents;
		ConfigurationRootActor->GetComponents<UDisplayClusterICVFXCameraComponent>(ExistsICVFXCameraComponents);

		for (UDisplayClusterICVFXCameraComponent* CfgCineCameraComponent : ExistsICVFXCameraComponents)
		{
			if (CfgCineCameraComponent && CfgCineCameraComponent->GetCameraSettingsICVFX().IsICVFXEnabled(*ConfigurationData, InConfiguration.GetClusterNodeId()))
			{
				const FString& CameraName = CfgCineCameraComponent->GetCameraUniqueId();

				OutViewports.Append(FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewGetRenderedInCameraViewports(InConfiguration, CameraName, bGetChromakey));
			}
		}
	}

	return OutViewports;
}

TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewGetRenderedInCameraViewports(FDisplayClusterViewportConfiguration& InConfiguration, const FString& InICVFXCameraId, bool bGetChromakey)
{
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> OutViewports;

	// Search for rendered camera viewport on other cluster nodes
	const EDisplayClusterViewportRuntimeICVFXFlags InRuntimeFlagsMask = bGetChromakey ? EDisplayClusterViewportRuntimeICVFXFlags::Chromakey : EDisplayClusterViewportRuntimeICVFXFlags::InCamera;
	const FString ViewportTypeId = bGetChromakey ? DisplayClusterViewportStrings::icvfx::chromakey : DisplayClusterViewportStrings::icvfx::camera;

	if (FDisplayClusterViewportManager* ViewportManager = InConfiguration.GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetEntireClusterViewports())
		{
			if (ViewportIt.IsValid()
				&& EnumHasAnyFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, InRuntimeFlagsMask)
				&& ( ViewportIt->GetViewportResources(EDisplayClusterViewportResource::InputShaderResources).Num() > 0
					&& ViewportIt->GetViewportResources(EDisplayClusterViewportResource::InputShaderResources)[0] != nullptr
					&& ViewportIt->GetContexts().Num() > 0)
				&& (!ViewportIt->GetRenderSettings().IsViewportOverridden()))
			{
				// this is incamera viewport. Check by name
				const FString RequiredViewportId = ImplGetNameICVFX(ViewportIt->GetClusterNodeId(), InICVFXCameraId, ViewportTypeId);
				if (RequiredViewportId.Equals(ViewportIt->GetId()))
				{
					OutViewports.Add(ViewportIt);
				}
			}
		}
	}

	return OutViewports;
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseInnerFrustumViewportWithinClusterNodes(FDisplayClusterViewport& InCameraViewport, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	if (!InCameraViewport.Configuration->GetRenderFrameSettings().CanReuseViewportWithinClusterNodes())
	{
		return;
	}

	if (FDisplayClusterViewportManager* ViewportManager = InCameraViewport.Configuration->GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : PreviewGetRenderedInCameraViewports(*InCameraViewport.Configuration, InCameraComponent.GetCameraUniqueId(), false))
		{
			if (ViewportIt.IsValid() && ViewportIt != InCameraViewport.AsShared() && ViewportIt->GetClusterNodeId() != InCameraViewport.GetClusterNodeId()
				&& FDisplayClusterViewportConfigurationHelpers_Postprocess::IsInnerFrustumViewportSettingsEqual(*ViewportIt, InCameraViewport, InCameraSettings))
			{
				EDisplayClusterViewportOverrideMode ViewportOverrideMode = ViewportIt->IsOpenColorIOEquals(InCameraViewport) ?
					EDisplayClusterViewportOverrideMode::All : EDisplayClusterViewportOverrideMode::InernalRTT;

				// Gain direct access to internal resources of the viewport:
				FDisplayClusterViewport_RenderSettings& InOutRenderSettings = InCameraViewport.GetRenderSettingsImpl();

				// Reuse exist viewport:
				InOutRenderSettings.SetViewportOverride(ViewportIt->GetId(), ViewportOverrideMode);
				return;
			}
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseChromakeyViewportWithinClusterNodes(FDisplayClusterViewport& InChromakeyViewport, const FString& InICVFXCameraId)
{
	if (!InChromakeyViewport.Configuration->GetRenderFrameSettings().CanReuseViewportWithinClusterNodes())
	{
		return;
	}
	
	if (FDisplayClusterViewportManager* ViewportManager = InChromakeyViewport.Configuration->GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : PreviewGetRenderedInCameraViewports(*InChromakeyViewport.Configuration, InICVFXCameraId, true))
		{
			if (ViewportIt.IsValid() && ViewportIt != InChromakeyViewport.AsShared() && ViewportIt->GetClusterNodeId() != InChromakeyViewport.GetClusterNodeId())
			{
				// Chromakey support OCIO
				EDisplayClusterViewportOverrideMode ViewportOverrideMode = ViewportIt->IsOpenColorIOEquals(InChromakeyViewport) ?
					EDisplayClusterViewportOverrideMode::All : EDisplayClusterViewportOverrideMode::InernalRTT;

				// Gain direct access to internal resources of the viewport:
				FDisplayClusterViewport_RenderSettings& InOutRenderSettings = InChromakeyViewport.GetRenderSettingsImpl();

				// Reuse exist viewport from other node
				InOutRenderSettings.SetViewportOverride(ViewportIt->GetId(), ViewportOverrideMode);
				return;
			}
		}
	}
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateChromakeyViewport(FDisplayClusterViewportConfiguration& InConfiguration, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	const FString InICVFXCameraId = InCameraComponent.GetCameraUniqueId();
	FDisplayClusterViewport* ChromakeyViewport = FindViewportICVFX(InConfiguration, InICVFXCameraId, DisplayClusterViewportStrings::icvfx::chromakey);

	// Create new chromakey viewport
	if (ChromakeyViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ChromakeyProjectionPolicy;
		if (!CreateProjectionPolicyICVFX(InConfiguration, InICVFXCameraId, DisplayClusterViewportStrings::icvfx::chromakey, false, ChromakeyProjectionPolicy))
		{
			return nullptr;
		}

		ChromakeyViewport = CreateViewportICVFX(InConfiguration, InICVFXCameraId, DisplayClusterViewportStrings::icvfx::chromakey, ChromakeyProjectionPolicy);
		if (ChromakeyViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Gain direct access to internal resources of the viewport:
	FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = ChromakeyViewport->GetRenderSettingsICVFXImpl();

	// Mark viewport as used
	EnumRemoveFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Chromakey
	EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Chromakey);

	return ChromakeyViewport;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateLightcardViewport(FDisplayClusterViewport& BaseViewport)
{
	// Create new lightcard viewport
	const FString ResourceId = DisplayClusterViewportStrings::icvfx::lightcard;

	FDisplayClusterViewport* LightcardViewport = FindViewportICVFX(*BaseViewport.Configuration, BaseViewport.GetId(), ResourceId);
	if (LightcardViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> LightcardProjectionPolicy;
		if (!CreateProjectionPolicyICVFX(*BaseViewport.Configuration, BaseViewport.GetId(), ResourceId, false, LightcardProjectionPolicy))
		{
			return nullptr;
		}

		LightcardViewport = CreateViewportICVFX(*BaseViewport.Configuration, BaseViewport.GetId(), ResourceId, LightcardProjectionPolicy);
		if (LightcardViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Gain direct access to internal resources of the viewport:
	FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = LightcardViewport->GetRenderSettingsICVFXImpl();

	// Mark viewport as used
	EnumRemoveFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Lightcard
	EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard);

	return LightcardViewport;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateUVLightcardViewport(FDisplayClusterViewport& BaseViewport)
{
	// Create new lightcard viewport
	const FString ResourceId = DisplayClusterViewportStrings::icvfx::uv_lightcard;

	FDisplayClusterViewport* UVLightcardViewport = FindViewportICVFX(*BaseViewport.Configuration, BaseViewport.GetId(), ResourceId);
	if (UVLightcardViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> UVLightcardProjectionPolicy;
		if (!CreateProjectionPolicyICVFX(*BaseViewport.Configuration, BaseViewport.GetId(), ResourceId, false, UVLightcardProjectionPolicy))
		{
			return nullptr;
		}

		UVLightcardViewport = CreateViewportICVFX(*BaseViewport.Configuration, BaseViewport.GetId(), ResourceId, UVLightcardProjectionPolicy);
		if (UVLightcardViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Gain direct access to internal resources of the viewport:
	FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = UVLightcardViewport->GetRenderSettingsICVFXImpl();

	// Mark viewport as used
	EnumRemoveFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Lightcard
	EnumAddFlags(InOutRenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard);

	return UVLightcardViewport;
}

bool FDisplayClusterViewportConfigurationHelpers_ICVFX::IsCameraUsed(const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	// Check rules for camera settings:
	if (InCameraSettings.bEnable == false)
	{
		// dont use camera if disabled
		return false;
	}

	if (InCameraSettings.RenderSettings.Replace.bAllowReplace && InCameraSettings.RenderSettings.Replace.SourceTexture == nullptr)
	{
		// RenderSettings.Override require source texture
		return false;
	}

	return true;
}

FDisplayClusterShaderParameters_ICVFX::FCameraSettings FDisplayClusterViewportConfigurationHelpers_ICVFX::GetShaderParametersCameraSettings(const FDisplayClusterViewport& InCameraViewport, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	FDisplayClusterShaderParameters_ICVFX::FCameraSettings Result;

	ADisplayClusterRootActor* SceneRootActor = InCameraViewport.Configuration->GetRootActor(EDisplayClusterRootActorType::Scene);
	ADisplayClusterRootActor* ConfigurationRootActor = InCameraViewport.Configuration->GetRootActor(EDisplayClusterRootActorType::Configuration);
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = InCameraViewport.Configuration->GetStageSettings();

	if (SceneRootActor && ConfigurationRootActor && StageSettings)
	{
		Result.Resource.ViewportId = InCameraViewport.GetId();

		UCineCameraComponent* ActualCineCameraComponent = InCameraComponent.GetActualCineCameraComponent();
		check(ActualCineCameraComponent);

		// Get camera border settings
		InCameraSettings.GetCameraBorder(*StageSettings, Result.InnerCameraBorderColor, Result.InnerCameraBorderThickness);
		Result.InnerCameraFrameAspectRatio = InCameraSettings.GetCameraFrameAspectRatio(*StageSettings, *ActualCineCameraComponent);

		// Soft edges
		Result.SoftEdge = InCameraSettings.GetCameraSoftEdge(*StageSettings, *ActualCineCameraComponent);

		// Rendering order for camera overlap
		const FString InnerFrustumID = InCameraComponent.GetCameraUniqueId();
		const int32 CameraRenderOrder = ConfigurationRootActor->GetInnerFrustumPriority(InnerFrustumID);
		Result.RenderOrder = (CameraRenderOrder < 0) ? InCameraSettings.RenderSettings.RenderOrder : CameraRenderOrder;
	}

	return Result;
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportSettings(FDisplayClusterViewport& DstViewport, UDisplayClusterICVFXCameraComponent& InCameraComponent, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = DstViewport.Configuration->GetStageSettings();
	if (!StageSettings)
	{
		return;
	}

	// Gain direct access to internal resources of the viewport:
	FDisplayClusterViewport_RenderSettings& InOutRenderSettings = DstViewport.GetRenderSettingsImpl();

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// incamera textrure used as overlay
	InOutRenderSettings.bVisible = false;

	// use framecolor instead of viewport rendering
	if (InCameraSettings.Chromakey.GetChromakeyType(*StageSettings) == EDisplayClusterShaderParametersICVFX_ChromakeySource::FrameColor)
	{
		InOutRenderSettings.bSkipRendering = true;
	}

	// Update camera viewport projection policy settings
	UpdateCameraProjectionSettingsICVFX(*DstViewport.Configuration, InCameraComponent, InCameraSettings, DstViewport.GetProjectionPolicy());

	// Update OCIO for Camera Viewport
	FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateCameraViewportOCIO(DstViewport, InCameraSettings);

	// Motion blur:
	DstViewport.UpdateConfiguration_CameraMotionBlur(InCameraComponent.GetMotionBlurParameters());

	// Depth of field
	DstViewport.UpdateConfiguration_CameraDepthOfField(InCameraComponent.GetDepthOfFieldParameters());

	// FDisplayClusterConfigurationICVFX_CameraSettings
	InOutRenderSettings.CameraId.Empty();

	UCineCameraComponent* ActualCineCameraComponent = InCameraComponent.GetActualCineCameraComponent();
	check(ActualCineCameraComponent);

	// UDisplayClusterConfigurationICVFX_CameraRenderSettings
	const FIntPoint DesiredSize = InCameraSettings.GetCameraFrameSize(*StageSettings, *ActualCineCameraComponent);

	InOutRenderSettings.Rect = FDisplayClusterViewportHelpers::GetValidViewportRect(FIntRect(FIntPoint(0, 0), DesiredSize), DstViewport.GetId(), TEXT("Configuration Camera Frame Size"));

	// Apply postprocess for camera
	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateCameraPostProcessSettings(DstViewport, InCameraComponent, InCameraSettings);

	DstViewport.UpdateConfiguration_PostRenderOverride(InCameraSettings.RenderSettings.Replace);
	DstViewport.UpdateConfiguration_PostRenderBlur(InCameraSettings.RenderSettings.PostprocessBlur);
	DstViewport.UpdateConfiguration_PostRenderGenerateMips(InCameraSettings.RenderSettings.GenerateMips);

	// UDisplayClusterConfigurationICVFX_CameraAdvancedRenderSettings
	const FDisplayClusterConfigurationICVFX_CameraAdvancedRenderSettings& InAdvancedRS = InCameraSettings.RenderSettings.AdvancedRenderSettings;
	{
		InOutRenderSettings.RenderTargetRatio = InAdvancedRS.RenderTargetRatio;
		InOutRenderSettings.GPUIndex = InAdvancedRS.GPUIndex;
		InOutRenderSettings.StereoGPUIndex = InAdvancedRS.StereoGPUIndex;
		InOutRenderSettings.bForceMono = FDisplayClusterViewportConfigurationHelpers::IsForceMonoscopicRendering(InAdvancedRS.StereoMode);
	}

	// Support inner camera custom frustum
	FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(InCameraSettings.CustomFrustum, InOutRenderSettings.CustomFrustumSettings);

	// Set RenderTargetAdaptRatio
	InOutRenderSettings.RenderTargetAdaptRatio = InCameraSettings.CustomFrustum.GetCameraAdaptResolutionRatio(*StageSettings);

	// Set viewport buffer ratio
	DstViewport.SetViewportBufferRatio(InCameraSettings.GetCameraBufferRatio(*StageSettings));

	// InCamera tile rendering.
	FDisplayClusterViewportConfigurationHelpers_Tile::UpdateICVFXCameraViewportTileSettings(DstViewport, InCameraSettings.RenderSettings.Media);
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateChromakeyViewportSettings(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& InCameraViewport, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings)
{
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = DstViewport.Configuration->GetStageSettings();
	if (!StageSettings)
	{
		return;
	}

	const FDisplayClusterConfigurationICVFX_ChromakeySettings& InChromakeySettings = InCameraSettings.Chromakey;

	// Gain direct access to internal resources of the viewport:
	FDisplayClusterViewport_RenderSettings&               InOutRenderSettings = DstViewport.GetRenderSettingsImpl();
	FDisplayClusterViewport_RenderSettings& InOutCameraViewportRenderSettings = InCameraViewport.GetRenderSettingsImpl();

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// Chromakey used as overlay
	InOutRenderSettings.bVisible = false;

	// Use special capture mode (this change RTT format and render flags)
	InOutRenderSettings.CaptureMode = EDisplayClusterViewportCaptureMode::Chromakey;

	// UDisplayClusterConfigurationICVFX_ChromakeyRenderSettings
	if (const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* InChromakeyRenderSettings = InChromakeySettings.GetChromakeyRenderSettings(*StageSettings))
	{
		DstViewport.UpdateConfiguration_PostRenderOverride(InChromakeyRenderSettings->Replace);
		DstViewport.UpdateConfiguration_PostRenderBlur(InChromakeyRenderSettings->PostprocessBlur);
		DstViewport.UpdateConfiguration_PostRenderGenerateMips(InChromakeyRenderSettings->GenerateMips);

		// Update visibility settings only for rendered viewports
		if (!DstViewport.GetPostRenderSettings().Replace.IsEnabled())
		{
			check(InChromakeyRenderSettings->ShowOnlyList.IsVisibilityListValid());

			FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(DstViewport, InChromakeyRenderSettings->ShowOnlyList);
		}

		DstViewport.UpdateConfiguration_OverlayRenderSettings(InChromakeyRenderSettings->AdvancedRenderSettings);

		// Support custom overlay size
		if (InChromakeyRenderSettings->CustomSize.bUseCustomSize)
		{
			FIntPoint DesiredSize;
			DesiredSize.X = InChromakeyRenderSettings->CustomSize.CustomWidth;
			DesiredSize.Y = InChromakeyRenderSettings->CustomSize.CustomHeight;

			InOutRenderSettings.Rect = FDisplayClusterViewportHelpers::GetValidViewportRect(FIntRect(FIntPoint(0, 0), DesiredSize), DstViewport.GetId(), TEXT("Configuration custom chromakey Frame Size"));
		}

		// Debug: override the texture of the target viewport from this chromakeyRTT
		if (InChromakeyRenderSettings->bReplaceCameraViewport)
		{
			InOutCameraViewportRenderSettings.SetViewportOverride(DstViewport.GetId());
		}
	}

	// Update OCIO for Chromakey Viewport
	// Note: Chromakey OCIO is temporarily disabled
	// FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateChromakeyViewportOCIO(DstViewport, RootActor, InCameraComponent);

	// Support inner camera custom frustum
	FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(InCameraSettings.CustomFrustum, InOutRenderSettings.CustomFrustumSettings);

	// Attach to parent viewport
	InOutRenderSettings.AssignParentViewport(InCameraViewport.GetId(), InOutCameraViewportRenderSettings);
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_Chromakey(FDisplayClusterShaderParameters_ICVFX::FCameraSettings& InOutCameraSettings, const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings, bool bEnableChromakeyMarkers, const FString& InChromakeyViewportId)
{
	// Set chromakey color
	InOutCameraSettings.ChromakeyColor = InCameraSettings.Chromakey.GetChromakeyColor(InStageSettings);

	// Set chromakey source
	switch (InOutCameraSettings.ChromakeySource = InCameraSettings.Chromakey.GetChromakeyType(InStageSettings))
	{
	case EDisplayClusterShaderParametersICVFX_ChromakeySource::ChromakeyLayers:
		if (InChromakeyViewportId.IsEmpty())
		{
			// Disable chromakey: CK viewport required
			InOutCameraSettings.ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled;
			return;
		}

		if (const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* ChromakeyRenderSettings = InCameraSettings.Chromakey.GetChromakeyRenderSettings(InStageSettings))
		{
			if (ChromakeyRenderSettings->bReplaceCameraViewport)
			{
				// Do not show Chromakey layers in this in-camera viewport, because they will be replaced by the Chromakey image (for debugging purposes).
				InOutCameraSettings.ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled;
				return;
			}
		}

		// Set the chromakey viewport name in the ICVFX shader params for current camera
		InOutCameraSettings.Chromakey.ViewportId = InChromakeyViewportId;

		break;

	case EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled:
		return;

	default:
		break;
	}

	//Setup chromakey markers
	if (bEnableChromakeyMarkers)
	{
		// Setup chromakey markers
		if (const FDisplayClusterConfigurationICVFX_ChromakeyMarkers* ChromakeyMarkers = InCameraSettings.Chromakey.GetChromakeyMarkers(InStageSettings))
		{
			InOutCameraSettings.ChromakeyMarkersColor = ChromakeyMarkers->MarkerColor;
			InOutCameraSettings.ChromakeyMarkersScale = ChromakeyMarkers->MarkerSizeScale;
			InOutCameraSettings.ChromakeyMarkersDistance = ChromakeyMarkers->MarkerTileDistance;
			InOutCameraSettings.ChromakeyMarkersOffset = ChromakeyMarkers->MarkerTileOffset;

			// Assign texture RHI ref
			if (FTextureResource* MarkersResource = ChromakeyMarkers->MarkerTileRGBA->GetResource())
			{
				InOutCameraSettings.ChromakeMarkerTextureRHI = MarkersResource->TextureRHI;
			}
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_OverlapChromakey(FDisplayClusterShaderParameters_ICVFX::FCameraSettings& InOutCameraSettings, const FDisplayClusterConfigurationICVFX_StageSettings& InStageSettings, const FDisplayClusterConfigurationICVFX_CameraSettings& InCameraSettings, bool bEnableChromakeyMarkers)
{
	// Setup overlap chromakey color
	InOutCameraSettings.OverlapChromakeyColor = InCameraSettings.Chromakey.GetOverlapChromakeyColor(InStageSettings);

	// Setup overlap chromakey markers
	if (bEnableChromakeyMarkers)
	{
		if (const FDisplayClusterConfigurationICVFX_ChromakeyMarkers* OverlapChromakeyMarkers = InCameraSettings.Chromakey.GetOverlapChromakeyMarkers(InStageSettings))
		{
			InOutCameraSettings.OverlapChromakeyMarkersColor = OverlapChromakeyMarkers->MarkerColor;
			InOutCameraSettings.OverlapChromakeyMarkersScale = OverlapChromakeyMarkers->MarkerSizeScale;
			InOutCameraSettings.OverlapChromakeyMarkersDistance = OverlapChromakeyMarkers->MarkerTileDistance;
			InOutCameraSettings.OverlapChromakeyMarkersOffset = OverlapChromakeyMarkers->MarkerTileOffset;

			// Assign texture RHI ref
			if (FTextureResource* OverlapMarkersResource = OverlapChromakeyMarkers->MarkerTileRGBA->GetResource())
			{
				InOutCameraSettings.OverlapChromakeyMarkerTextureRHI = OverlapMarkersResource->TextureRHI;
			}
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport)
{
	const FDisplayClusterConfigurationICVFX_StageSettings* InStageSettings = DstViewport.Configuration->GetStageSettings();
	if (!InStageSettings)
	{
		return;
	}

	const FDisplayClusterConfigurationICVFX_LightcardSettings& InLightcardSettings = InStageSettings->Lightcard;

	// Gain direct access to internal settings of the viewport:
	FDisplayClusterViewport_RenderSettings&                       InOutRenderSettings = DstViewport.GetRenderSettingsImpl();
	FDisplayClusterViewport_RenderSettings&           InOutBaseViewportRenderSettings = BaseViewport.GetRenderSettingsImpl();
	FDisplayClusterViewport_RenderSettingsICVFX& InOutBaseViewportRenderSettingsICVFX = BaseViewport.GetRenderSettingsICVFXImpl();

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// LIghtcard texture used as overlay
	InOutRenderSettings.bVisible = false;

	if (!InLightcardSettings.bEnable)
	{
		// Disable this viewport
		InOutRenderSettings.bEnable = false;
		return;
	}

	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateLightcardPostProcessSettings(DstViewport, BaseViewport);

	// Update OCIO for Lightcard Viewport
	FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateLightcardViewportOCIO(DstViewport, BaseViewport);

	InOutRenderSettings.CaptureMode = EDisplayClusterViewportCaptureMode::Lightcard;

	const FDisplayClusterConfigurationICVFX_LightcardRenderSettings& InRenderSettings = InLightcardSettings.RenderSettings;
	{
		DstViewport.UpdateConfiguration_PostRenderOverride(InRenderSettings.Replace);
		DstViewport.UpdateConfiguration_PostRenderBlur(InRenderSettings.PostprocessBlur);
		DstViewport.UpdateConfiguration_PostRenderGenerateMips(InRenderSettings.GenerateMips);

		// Update visibility settings only for rendered viewports
		if (!DstViewport.GetPostRenderSettings().Replace.IsEnabled())
		{
			check(InLightcardSettings.ShowOnlyList.IsVisibilityListValid());

			FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(DstViewport, InLightcardSettings.ShowOnlyList);
		}

		DstViewport.UpdateConfiguration_OverlayRenderSettings(InRenderSettings.AdvancedRenderSettings);
	}

	// Attach to parent viewport
	InOutRenderSettings.AssignParentViewport(BaseViewport.GetId(), BaseViewport.GetRenderSettings());

	if (EnumHasAnyFlags(DstViewport.GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard))
	{
		// Debug: override the texture of the target viewport from this lightcard RTT
		if (InRenderSettings.bReplaceViewport)
		{
			InOutBaseViewportRenderSettings.SetViewportOverride(DstViewport.GetId());
		}
		else
		{
			InOutBaseViewportRenderSettingsICVFX.ICVFX.LightCard.ViewportId = DstViewport.GetId();
		}
	}

	if (EnumHasAnyFlags(DstViewport.GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		InOutBaseViewportRenderSettingsICVFX.ICVFX.UVLightCard.ViewportId = DstViewport.GetId();
	}

	if (EnumHasAnyFlags(DstViewport.GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		// Gain direct access to internal settings of the viewport:
		FDisplayClusterViewport_RenderSettingsICVFX& InOutRenderSettingsICVFX = DstViewport.GetRenderSettingsICVFXImpl();

		// Set the light card gamma used to linearize the light card textures before blending during final composite
		// The OCIO pass will have already linearized the light card renders, so set the gamma to 1 if using OCIO on the light cards
		InOutRenderSettingsICVFX.ICVFX.LightCardGamma = DstViewport.GetOpenColorIO().IsValid() ? 1.0 : 2.2;
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(
	const FDisplayClusterConfigurationICVFX_CameraCustomFrustum& InCameraCustomFrustumConfiguration,
	FDisplayClusterViewport_CustomFrustumSettings& OutCustomFrustumSettings)
{
	OutCustomFrustumSettings.bEnabled = false;

	if (InCameraCustomFrustumConfiguration.bEnable)
	{
		OutCustomFrustumSettings.bAdaptResolution = InCameraCustomFrustumConfiguration.bAdaptResolution;

		switch (InCameraCustomFrustumConfiguration.Mode)
		{
		case EDisplayClusterConfigurationViewportCustomFrustumMode::Percent:
			OutCustomFrustumSettings.bEnabled = true;
			OutCustomFrustumSettings.Unit = EDisplayClusterViewport_FrustumUnit::Percent;

			// Scale 0..100% to 0..1 range
			OutCustomFrustumSettings.Left = .01f * InCameraCustomFrustumConfiguration.Left;
			OutCustomFrustumSettings.Right = .01f * InCameraCustomFrustumConfiguration.Right;
			OutCustomFrustumSettings.Top = .01f * InCameraCustomFrustumConfiguration.Top;
			OutCustomFrustumSettings.Bottom = .01f * InCameraCustomFrustumConfiguration.Bottom;
			break;

		case EDisplayClusterConfigurationViewportCustomFrustumMode::Pixels:
			OutCustomFrustumSettings.bEnabled = true;
			OutCustomFrustumSettings.Unit = EDisplayClusterViewport_FrustumUnit::Pixels;

			OutCustomFrustumSettings.Left = InCameraCustomFrustumConfiguration.Left;
			OutCustomFrustumSettings.Right = InCameraCustomFrustumConfiguration.Right;
			OutCustomFrustumSettings.Top = InCameraCustomFrustumConfiguration.Top;
			OutCustomFrustumSettings.Bottom = InCameraCustomFrustumConfiguration.Bottom;
			break;

		default:
			break;
		}
	}
};
