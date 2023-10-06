// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_ICVFX.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_OpenColorIO.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Postprocess.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Visibility.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"

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
#include "Render/Viewport/Containers/ImplDisplayClusterViewport_CustomFrustum.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"

#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"
#include "DisplayClusterProjectionStrings.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Misc/DisplayClusterLog.h"
#include "TextureResource.h"

#include "HAL/IConsoleManager.h"


////////////////////////////////////////////////////////////////////////////////
// Experimental feature: to be approved after testing
int32 GDisplayClusterPreviewEnableReuseViewportInCluster = 1;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableReuseViewportInCluster(
	TEXT("DC.Preview.EnableReuseViewportInCluster"),
	GDisplayClusterPreviewEnableReuseViewportInCluster,
	TEXT("Experimental feature (0 == disabled, 1 == enabled)"),
	ECVF_RenderThreadSafe
);

////////////////////////////////////////////////////////////////////////////////
namespace DisplayClusterViewportConfigurationHelpers_ICVFX_Impl
{
	// Initialize camera policy with camera component and settings
	static bool ImplUpdateCameraProjectionSettings(TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InOutCameraProjection, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings, UCameraComponent* const CameraComponent)
	{
		FDisplayClusterProjectionCameraPolicySettings PolicyCameraSettings;
		PolicyCameraSettings.FOVMultiplier = CameraSettings.GetCameraFieldOfViewMultiplier(RootActor.GetStageSettings());

		// Lens correction
		PolicyCameraSettings.FrustumRotation = CameraSettings.FrustumRotation;
		PolicyCameraSettings.FrustumOffset = CameraSettings.FrustumOffset;

		// Initialize camera policy with camera component and settings
		return IDisplayClusterProjection::Get().CameraPolicySetCamera(InOutCameraProjection, CameraComponent, PolicyCameraSettings);
	}

	// Return unique ICVFX name
	static FString ImplGetNameICVFX(const FString& InClusterNodeId, const FString& InViewportId, const FString& InResourceId)
	{
		check(!InClusterNodeId.IsEmpty());
		check(!InViewportId.IsEmpty());
		check(!InResourceId.IsEmpty());

		return FString::Printf(TEXT("%s_%s_%s_%s"), *InClusterNodeId, DisplayClusterViewportStrings::icvfx::prefix, *InViewportId, *InResourceId);
	}

#if WITH_EDITOR
	static bool IsPreviewEnableReuseViewportInCluster_EditorImpl(ADisplayClusterRootActor& InRootActor)
	{
		if (GDisplayClusterPreviewEnableReuseViewportInCluster > 0)
		{
			if (InRootActor.GetRenderMode() == EDisplayClusterRenderFrameMode::PreviewInScene)
			{
				return true;
			}
		}

		return false;
	}
#endif
};

using namespace DisplayClusterViewportConfigurationHelpers_ICVFX_Impl;

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationHelpers_ICVFX
////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::ImplFindViewport(ADisplayClusterRootActor& RootActor, const FString& InViewportId, const FString& InResourceId)
{
	if (FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor))
	{
		TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> Viewport = ViewportManager->ImplFindViewport(ImplGetNameICVFX(ViewportManager->GetRenderFrameSettings().ClusterNodeId, InViewportId, InResourceId));

		return Viewport.Get();
	}

	return nullptr;
}

static bool ImplCreateProjectionPolicy(ADisplayClusterRootActor& RootActor, const FString& InViewportId, const FString& InResourceId, bool bIsCameraProjection, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& OutProjPolicy)
{
	if (FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor))
	{
		const FString& ClusterNodeId = ViewportManager->GetRenderFrameSettings().ClusterNodeId;

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

	return false;
}

//------------------------------------------------------------------------------------------
//                FDisplayClusterViewportConfigurationHelpers_ICVFX
//------------------------------------------------------------------------------------------

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::ImplCreateViewport(ADisplayClusterRootActor& RootActor, const FString& InViewportId, const FString& InResourceId, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
{
	check(InProjectionPolicy.IsValid());

	if (FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor))
	{
		// Create viewport for new projection policy
		TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> NewViewport = ViewportManager->ImplCreateViewport(ImplGetNameICVFX(ViewportManager->GetRenderFrameSettings().ClusterNodeId, InViewportId, InResourceId), InProjectionPolicy);
		if (NewViewport.IsValid())
		{
			// Mark as internal resource
			EnumAddFlags(NewViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource);

			// Dont show ICVFX composing viewports on frame target
			NewViewport->RenderSettings.bVisible = false;

			return NewViewport.Get();
		}
	}

	return nullptr;
}

FDisplayClusterViewportManager* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(ADisplayClusterRootActor& RootActor)
{
	FDisplayClusterViewportManager* ViewportManager = static_cast<FDisplayClusterViewportManager*>(RootActor.GetViewportManager());
	if (ViewportManager == nullptr)
	{
		UE_LOG(LogDisplayClusterViewport, Error, TEXT("Viewport manager not exist in root actor."));
	}

	return ViewportManager;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::FindCameraViewport(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FString CameraId = InCameraComponent.GetCameraUniqueId();

	return ImplFindViewport(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::camera);
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateCameraViewport(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FString CameraId = InCameraComponent.GetCameraUniqueId();

	FDisplayClusterViewport* CameraViewport = ImplFindViewport(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::camera);

	// Create new camera viewport
	if (CameraViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> CameraProjectionPolicy;
		if (!ImplCreateProjectionPolicy(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::camera, true, CameraProjectionPolicy))
		{
			return nullptr;
		}

		CameraViewport = ImplCreateViewport(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::camera, CameraProjectionPolicy);
		if (CameraViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Mark viewport as used
	EnumRemoveFlags(CameraViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Incamera
	EnumAddFlags(CameraViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera);

	return CameraViewport;
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::ReuseUVLightCardViewportWithinClusterNode(FDisplayClusterViewport& InUVLightCardViewport)
{
	if (FDisplayClusterViewportManager* ViewportManager = InUVLightCardViewport.GetViewportManagerImpl())
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetEntireClusterViewports())
		{
			if (ViewportIt.IsValid() && ViewportIt != InUVLightCardViewport.AsShared() && !ViewportIt->RenderSettings.IsViewportOverrided()
				&& EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
			{
				if (ViewportIt->IsOpenColorIOEquals(InUVLightCardViewport))
				{
					// Reuse exist viewport:
					InUVLightCardViewport.RenderSettings.SetViewportOverride(ViewportIt->GetId(), EDisplayClusterViewportOverrideMode::All);
					break;
				}
			}
		}
	}
}

#if WITH_EDITOR
TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewGetRenderedInCameraViewports(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent, bool bGetChromakey)
{
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> OutViewports;

	// Search for rendered camera viewport on other cluster nodes
	if (FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(InRootActor))
	{
		const FString ICVFXCameraId = InCameraComponent.GetCameraUniqueId();
		const EDisplayClusterViewportRuntimeICVFXFlags InRuntimeFlagsMask = bGetChromakey ? EDisplayClusterViewportRuntimeICVFXFlags::Chromakey : EDisplayClusterViewportRuntimeICVFXFlags::InCamera;
		const FString ViewportTypeId = bGetChromakey ? DisplayClusterViewportStrings::icvfx::chromakey : DisplayClusterViewportStrings::icvfx::camera;

		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetEntireClusterViewports())
		{
			if (ViewportIt.IsValid()
				&& EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, InRuntimeFlagsMask)
				&& (ViewportIt->InputShaderResources.Num() > 0 && ViewportIt->InputShaderResources[0] != nullptr && ViewportIt->Contexts.Num() > 0)
				&& (!ViewportIt->RenderSettings.IsViewportOverrided()))
			{
				// this is incamera viewport. Check by name
				const FString RequiredViewportId = ImplGetNameICVFX(ViewportIt->GetClusterNodeId(), ICVFXCameraId, ViewportTypeId);
				if (RequiredViewportId.Equals(ViewportIt->GetId()))
				{
					OutViewports.Add(ViewportIt);
				}
			}
		}
	}

	return OutViewports;
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseInnerFrustumViewportWithinClusterNodes_Editor(FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	if (!IsPreviewEnableReuseViewportInCluster_EditorImpl(InRootActor))
	{
		return;
	}

	// Only for preview mode
	if (InCameraViewport.GetRenderMode() != EDisplayClusterRenderFrameMode::PreviewInScene)
	{
		return;
	}

	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : PreviewGetRenderedInCameraViewports(InRootActor, InCameraComponent, false))
	{
		if (ViewportIt.IsValid() && ViewportIt != InCameraViewport.AsShared() && ViewportIt->GetClusterNodeId() != InCameraViewport.GetClusterNodeId()
			&& FDisplayClusterViewportConfigurationHelpers_Postprocess::IsInnerFrustumViewportSettingsEqual_Editor(*ViewportIt, InCameraViewport, InCameraComponent))
		{
			EDisplayClusterViewportOverrideMode ViewportOverrideMode = ViewportIt->IsOpenColorIOEquals(InCameraViewport) ?
				EDisplayClusterViewportOverrideMode::All : EDisplayClusterViewportOverrideMode::InernalRTT;

			// Reuse exist viewport:
			InCameraViewport.RenderSettings.SetViewportOverride(ViewportIt->GetId(), ViewportOverrideMode);
			return;
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseChromakeyViewportWithinClusterNodes_Editor(FDisplayClusterViewport& InChromakeyViewport, ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	if (!IsPreviewEnableReuseViewportInCluster_EditorImpl(InRootActor))
	{
		return;
	}
	
	// Only for preview mode
	if (InChromakeyViewport.GetRenderMode() != EDisplayClusterRenderFrameMode::PreviewInScene)
	{
		return;
	}

	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : PreviewGetRenderedInCameraViewports(InRootActor, InCameraComponent, true))
	{
		if (ViewportIt.IsValid() && ViewportIt != InChromakeyViewport.AsShared() && ViewportIt->GetClusterNodeId() != InChromakeyViewport.GetClusterNodeId())
		{
			// Chromakey support OCIO
			EDisplayClusterViewportOverrideMode ViewportOverrideMode = ViewportIt->IsOpenColorIOEquals(InChromakeyViewport) ?
				EDisplayClusterViewportOverrideMode::All : EDisplayClusterViewportOverrideMode::InernalRTT;

			// Reuse exist viewport from other node
			InChromakeyViewport.RenderSettings.SetViewportOverride(ViewportIt->GetId(), ViewportOverrideMode);
			return;
		}
	}
}
#endif

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateChromakeyViewport(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FString CameraId = InCameraComponent.GetCameraUniqueId();

	FDisplayClusterViewport* ChromakeyViewport = ImplFindViewport(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::chromakey);

	// Create new chromakey viewport
	if (ChromakeyViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ChromakeyProjectionPolicy;
		if (!ImplCreateProjectionPolicy(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::chromakey, false, ChromakeyProjectionPolicy))
		{
			return nullptr;
		}

		ChromakeyViewport = ImplCreateViewport(RootActor, CameraId, DisplayClusterViewportStrings::icvfx::chromakey, ChromakeyProjectionPolicy);
		if (ChromakeyViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Mark viewport as used
	EnumRemoveFlags(ChromakeyViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Chromakey
	EnumAddFlags(ChromakeyViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Chromakey);

	return ChromakeyViewport;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateLightcardViewport(FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	// Create new lightcard viewport
	const FString ResourceId = DisplayClusterViewportStrings::icvfx::lightcard;

	FDisplayClusterViewport* LightcardViewport = ImplFindViewport(RootActor, BaseViewport.GetId(), ResourceId);
	if (LightcardViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> LightcardProjectionPolicy;
		if (!ImplCreateProjectionPolicy(RootActor, BaseViewport.GetId(), ResourceId, false, LightcardProjectionPolicy))
		{
			return nullptr;
		}

		LightcardViewport = ImplCreateViewport(RootActor, BaseViewport.GetId(), ResourceId, LightcardProjectionPolicy);
		if (LightcardViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Mark viewport as used
	EnumRemoveFlags(LightcardViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Lightcard
	EnumAddFlags(LightcardViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard);

	return LightcardViewport;
}

FDisplayClusterViewport* FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateUVLightcardViewport(FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	// Create new lightcard viewport
	const FString ResourceId = DisplayClusterViewportStrings::icvfx::uv_lightcard;

	FDisplayClusterViewport* UVLightcardViewport = ImplFindViewport(RootActor, BaseViewport.GetId(), ResourceId);
	if (UVLightcardViewport == nullptr)
	{
		TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> UVLightcardProjectionPolicy;
		if (!ImplCreateProjectionPolicy(RootActor, BaseViewport.GetId(), ResourceId, false, UVLightcardProjectionPolicy))
		{
			return nullptr;
		}

		UVLightcardViewport = ImplCreateViewport(RootActor, BaseViewport.GetId(), ResourceId, UVLightcardProjectionPolicy);
		if (UVLightcardViewport == nullptr)
		{
			return nullptr;
		}
	}

	// Mark viewport as used
	EnumRemoveFlags(UVLightcardViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);

	// Add viewport ICVFX usage as Lightcard
	EnumAddFlags(UVLightcardViewport->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard);

	return UVLightcardViewport;
}

bool FDisplayClusterViewportConfigurationHelpers_ICVFX::IsCameraUsed(UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();

	// Check rules for camera settings:
	if (CameraSettings.bEnable == false)
	{
		// dont use camera if disabled
		return false;
	}

	if (CameraSettings.RenderSettings.Replace.bAllowReplace && CameraSettings.RenderSettings.Replace.SourceTexture == nullptr)
	{
		// RenderSettings.Override require source texture
		return false;
	}

	return true;
}

FDisplayClusterShaderParameters_ICVFX::FCameraSettings FDisplayClusterViewportConfigurationHelpers_ICVFX::GetShaderParametersCameraSettings(const FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const USceneComponent* OriginComp = RootActor.GetRootComponent();
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	FDisplayClusterShaderParameters_ICVFX::FCameraSettings Result;

	Result.Resource.ViewportId = InCameraViewport.GetId();
	Result.Local2WorldTransform = OriginComp->GetComponentTransform();

	// Get camera border settings
	CameraSettings.GetCameraBorder(StageSettings, Result.InnerCameraBorderColor, Result.InnerCameraBorderThickness);
	Result.InnerCameraFrameAspectRatio = CameraSettings.GetCameraFrameAspectRatio(StageSettings);

	// Soft edges
	Result.SoftEdge = CameraSettings.GetCameraSoftEdge(StageSettings);

	// Rendering order for camera overlap
	const FString InnerFrustumID = InCameraComponent.GetCameraUniqueId();
	const int32 CameraRenderOrder = RootActor.GetInnerFrustumPriority(InnerFrustumID);
	Result.RenderOrder = (CameraRenderOrder<0) ? CameraSettings.RenderSettings.RenderOrder : CameraRenderOrder;

	return Result;
}

bool FDisplayClusterViewportConfigurationHelpers_ICVFX::GetCameraContext(ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent, FCameraContext_ICVFX& OutCameraContext)
{
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();

	// Create new camera projection policy for camera viewport
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> CameraProjectionPolicy;
	if (!ImplCreateProjectionPolicy(RootActor, InCameraComponent.GetCameraUniqueId(), DisplayClusterViewportStrings::icvfx::camera, true, CameraProjectionPolicy))
	{
		return false;
	}

	// Initialize camera policy with camera component and settings
	if (!ImplUpdateCameraProjectionSettings(CameraProjectionPolicy, RootActor, CameraSettings, InCameraComponent.GetCameraComponent()))
	{
		return false;
	}

	// Get camera pos-rot-prj from policy
	const float WorldToMeters = 100.f;
	const float CfgNCP = 1.0f;
	const FVector ViewOffset = FVector::ZeroVector;

	if (CameraProjectionPolicy->CalculateView(nullptr, 0, OutCameraContext.ViewLocation, OutCameraContext.ViewRotation, ViewOffset, WorldToMeters, CfgNCP, CfgNCP) &&
		CameraProjectionPolicy->GetProjectionMatrix(nullptr, 0, OutCameraContext.PrjMatrix))
	{
		return true;
	}

	return false;
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportSettings(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_StageSettings&  StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// incamera textrure used as overlay
	DstViewport.RenderSettings.bVisible = false;

	// use framecolor instead of viewport rendering
	if (CameraSettings.Chromakey.GetChromakeyType(StageSettings) == EDisplayClusterShaderParametersICVFX_ChromakeySource::FrameColor)
	{
		DstViewport.RenderSettings.bSkipRendering = true;
	}

	// Update camera viewport projection policy settings
	ImplUpdateCameraProjectionSettings(DstViewport.ProjectionPolicy, RootActor, CameraSettings, InCameraComponent.GetCameraComponent());

	// Update OCIO for Camera Viewport
	FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateCameraViewport(DstViewport, RootActor, InCameraComponent);

	// Motion blur:
	const FDisplayClusterViewport_CameraMotionBlur CameraMotionBlurParameters = InCameraComponent.GetMotionBlurParameters();
	DstViewport.CameraMotionBlur.BlurSetup = CameraMotionBlurParameters;

	// FDisplayClusterConfigurationICVFX_CameraSettings
	DstViewport.RenderSettings.CameraId.Empty();

	// UDisplayClusterConfigurationICVFX_CameraRenderSettings
	const FIntPoint DesiredSize = CameraSettings.GetCameraFrameSize(StageSettings);

	DstViewport.RenderSettings.Rect = DstViewport.GetValidRect(FIntRect(FIntPoint(0, 0), DesiredSize), TEXT("Configuration Camera Frame Size"));

	// Apply postprocess for camera
	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateCameraPostProcessSettings(DstViewport, RootActor, InCameraComponent);

	FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_Override(DstViewport, CameraSettings.RenderSettings.Replace);
	FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_PostprocessBlur(DstViewport, CameraSettings.RenderSettings.PostprocessBlur);
	FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_GenerateMips(DstViewport, CameraSettings.RenderSettings.GenerateMips);

	// UDisplayClusterConfigurationICVFX_CameraAdvancedRenderSettings
	const FDisplayClusterConfigurationICVFX_CameraAdvancedRenderSettings& InAdvancedRS = CameraSettings.RenderSettings.AdvancedRenderSettings;
	{
		DstViewport.RenderSettings.RenderTargetRatio = InAdvancedRS.RenderTargetRatio;
		DstViewport.RenderSettings.GPUIndex = InAdvancedRS.GPUIndex;
		DstViewport.RenderSettings.StereoGPUIndex = InAdvancedRS.StereoGPUIndex;
		DstViewport.RenderSettings.RenderFamilyGroup = InAdvancedRS.RenderFamilyGroup;

		FDisplayClusterViewportConfigurationHelpers::UpdateViewportStereoMode(DstViewport, InAdvancedRS.StereoMode);
	}

	// Support inner camera custom frustum
	FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(DstViewport, CameraSettings.CustomFrustum);

	// Set RenderTargetAdaptRatio
	DstViewport.RenderSettings.RenderTargetAdaptRatio = CameraSettings.GetCameraAdaptResolutionRatio(StageSettings);

	// Set viewport buffer ratio
	DstViewport.SetViewportBufferRatio(CameraSettings.GetCameraBufferRatio(StageSettings));

	// Set media related configuration (runtime only for now)
	if (IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
	{
		// Check if nDisplay media enabled
		static const TConsoleVariableData<int32>* const ICVarMediaEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("nDisplay.media.Enabled"));
		if (ICVarMediaEnabled && !!ICVarMediaEnabled->GetValueOnGameThread())
		{
			const FDisplayClusterConfigurationMediaICVFX& MediaICVFXSettings = InCameraComponent.CameraSettings.RenderSettings.Media;

			if (MediaICVFXSettings.bEnable)
			{
				const FString ThisClusterNodeId = DstViewport.GetClusterNodeId();

				// Don't render the viewport if media input assigned
				DstViewport.RenderSettings.bSkipSceneRenderingButLeaveResourcesAvailable = MediaICVFXSettings.IsMediaInputAssigned(ThisClusterNodeId);

				// Mark this viewport is going to be captured by a capture device
				DstViewport.RenderSettings.bIsBeingCaptured = MediaICVFXSettings.IsMediaOutputAssigned(ThisClusterNodeId);
			}
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateChromakeyViewportSettings(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	const FDisplayClusterConfigurationICVFX_ChromakeySettings& ChromakeySettings = CameraSettings.Chromakey;

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// Chromakey used as overlay
	DstViewport.RenderSettings.bVisible = false;

	// Use special capture mode (this change RTT format and render flags)
	DstViewport.RenderSettings.CaptureMode = EDisplayClusterViewportCaptureMode::Chromakey;

	// UDisplayClusterConfigurationICVFX_ChromakeyRenderSettings
	if (const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* ChromakeyRenderSettings = ChromakeySettings.GetChromakeyRenderSettings(StageSettings))
	{
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_Override(DstViewport, ChromakeyRenderSettings->Replace);
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_PostprocessBlur(DstViewport, ChromakeyRenderSettings->PostprocessBlur);
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_GenerateMips(DstViewport, ChromakeyRenderSettings->GenerateMips);

		// Update visibility settings only for rendered viewports
		if (!DstViewport.PostRenderSettings.Replace.IsEnabled())
		{
			check(ChromakeyRenderSettings->ShowOnlyList.IsVisibilityListValid());

			FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(DstViewport, RootActor, ChromakeyRenderSettings->ShowOnlyList);
		}

		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_OverlayRenderSettings(DstViewport, ChromakeyRenderSettings->AdvancedRenderSettings);

		// Support custom overlay size
		if (ChromakeyRenderSettings->CustomSize.bUseCustomSize)
		{
			FIntPoint DesiredSize;
			DesiredSize.X = ChromakeyRenderSettings->CustomSize.CustomWidth;
			DesiredSize.Y = ChromakeyRenderSettings->CustomSize.CustomHeight;

			DstViewport.RenderSettings.Rect = DstViewport.GetValidRect(FIntRect(FIntPoint(0, 0), DesiredSize), TEXT("Configuration custom chromakey Frame Size"));
		}

		// Debug: override the texture of the target viewport from this chromakeyRTT
		if (ChromakeyRenderSettings->bReplaceCameraViewport)
		{
			InCameraViewport.RenderSettings.SetViewportOverride(DstViewport.GetId());
		}
	}

	// Update OCIO for Chromakey Viewport
	// Note: Chromakey OCIO is temporarily disabled
	// FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateChromakeyViewport(DstViewport, RootActor, InCameraComponent);

	// Support inner camera custom frustum
	FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(DstViewport, CameraSettings.CustomFrustum);

	// Attach to parent viewport
	DstViewport.RenderSettings.AssignParentViewport(InCameraViewport.GetId(), InCameraViewport.RenderSettings);
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

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_LightcardSettings& LightcardSettings = StageSettings.Lightcard;

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// LIghtcard texture used as overlay
	DstViewport.RenderSettings.bVisible = false;

	if (!LightcardSettings.bEnable)
	{
		// Disable this viewport
		DstViewport.RenderSettings.bEnable = false;
		return;
	}

	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateLightcardPostProcessSettings(DstViewport, BaseViewport, RootActor);

	// Update OCIO for Lightcard Viewport
	FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateLightcardViewport(DstViewport, BaseViewport, RootActor);

	DstViewport.RenderSettings.CaptureMode = EDisplayClusterViewportCaptureMode::Lightcard;

	const FDisplayClusterConfigurationICVFX_LightcardRenderSettings& InRenderSettings = LightcardSettings.RenderSettings;
	{
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_Override(DstViewport, InRenderSettings.Replace);
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_PostprocessBlur(DstViewport, InRenderSettings.PostprocessBlur);
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_GenerateMips(DstViewport, InRenderSettings.GenerateMips);

		// Update visibility settings only for rendered viewports
		if (!DstViewport.PostRenderSettings.Replace.IsEnabled())
		{
			check(LightcardSettings.ShowOnlyList.IsVisibilityListValid());

			FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(DstViewport, RootActor, LightcardSettings.ShowOnlyList);
		}

		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_OverlayRenderSettings(DstViewport, InRenderSettings.AdvancedRenderSettings);
	}

	// Attach to parent viewport
	DstViewport.RenderSettings.AssignParentViewport(BaseViewport.GetId(), BaseViewport.RenderSettings);

	if (EnumHasAnyFlags(DstViewport.RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard))
	{
		// Debug: override the texture of the target viewport from this lightcard RTT
		if (InRenderSettings.bReplaceViewport)
		{
			BaseViewport.RenderSettings.SetViewportOverride(DstViewport.GetId());
		}
		else
		{
			BaseViewport.RenderSettingsICVFX.ICVFX.LightCard.ViewportId = DstViewport.GetId();
		}
	}

	if (EnumHasAnyFlags(DstViewport.RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		BaseViewport.RenderSettingsICVFX.ICVFX.UVLightCard.ViewportId = DstViewport.GetId();
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_CameraCustomFrustum& InCameraCustomFrustumConfiguration)
{
	if (InCameraCustomFrustumConfiguration.bEnable)
	{
		FImplDisplayClusterViewport_CustomFrustumSettings CustomFrustumSettings;
		CustomFrustumSettings.bAdaptResolution = InCameraCustomFrustumConfiguration.bAdaptResolution;

		switch (InCameraCustomFrustumConfiguration.Mode)
		{
		case EDisplayClusterConfigurationViewportCustomFrustumMode::Percent:
			CustomFrustumSettings.Mode = EDisplayClusterViewport_CustomFrustumMode::Percent;

			// Scale 0..100% to 0..1 range
			CustomFrustumSettings.Left = .01f * InCameraCustomFrustumConfiguration.Left;
			CustomFrustumSettings.Right = .01f * InCameraCustomFrustumConfiguration.Right;
			CustomFrustumSettings.Top = .01f * InCameraCustomFrustumConfiguration.Top;
			CustomFrustumSettings.Bottom = .01f * InCameraCustomFrustumConfiguration.Bottom;
			break;

		case EDisplayClusterConfigurationViewportCustomFrustumMode::Pixels:
			CustomFrustumSettings.Mode = EDisplayClusterViewport_CustomFrustumMode::Pixels;

			CustomFrustumSettings.Left = InCameraCustomFrustumConfiguration.Left;
			CustomFrustumSettings.Right = InCameraCustomFrustumConfiguration.Right;
			CustomFrustumSettings.Top = InCameraCustomFrustumConfiguration.Top;
			CustomFrustumSettings.Bottom = InCameraCustomFrustumConfiguration.Bottom;
			break;

		default:
			break;
		}

		DstViewport.CustomFrustumRendering.Set(CustomFrustumSettings);
	}
};
