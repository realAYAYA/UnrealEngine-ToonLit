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

#include "Containers/DisplayClusterProjectionCameraPolicySettings.h"
#include "DisplayClusterProjectionStrings.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"


#include "Misc/DisplayClusterLog.h"

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
	static float ImplGetFieldOfViewMultiplier(const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings)
	{
		return CameraSettings.CustomFrustum.bEnable ? CameraSettings.CustomFrustum.FieldOfViewMultiplier : 1.f;
	}

	// Initialize camera policy with camera component and settings
	static bool ImplUpdateCameraProjectionSettings(TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InOutCameraProjection, const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings, UCameraComponent* const CameraComponent)
	{
		FDisplayClusterProjectionCameraPolicySettings PolicyCameraSettings;
		PolicyCameraSettings.FOVMultiplier = ImplGetFieldOfViewMultiplier(CameraSettings);

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
			FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(InRootActor);
			if (ViewportManager)
			{
				const FDisplayClusterRenderFrameSettings& RenderFrameSettings = ViewportManager->GetRenderFrameSettings();
				switch (RenderFrameSettings.RenderMode)
				{
				case EDisplayClusterRenderFrameMode::PreviewInScene:
					return true;
				default:
					break;
				}
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
	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor);
	if (ViewportManager != nullptr)
	{
		return ViewportManager->ImplFindViewport(ImplGetNameICVFX(ViewportManager->GetRenderFrameSettings().ClusterNodeId, InViewportId, InResourceId));
	}

	return nullptr;
}

static bool ImplCreateProjectionPolicy(ADisplayClusterRootActor& RootActor, const FString& InViewportId, const FString& InResourceId, bool bIsCameraProjection, TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& OutProjPolicy)
{
	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor);
	if (ViewportManager != nullptr)
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

	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor);
	if (ViewportManager != nullptr)
	{
		// Create viewport for new projection policy
		FDisplayClusterViewport* NewViewport = ViewportManager->ImplCreateViewport(ImplGetNameICVFX(ViewportManager->GetRenderFrameSettings().ClusterNodeId, InViewportId, InResourceId), InProjectionPolicy);
		if (NewViewport != nullptr)
		{
			// Mark as internal resource
			NewViewport->RenderSettingsICVFX.RuntimeFlags |= ViewportRuntime_InternalResource;

			// Dont show ICVFX composing viewports on frame target
			NewViewport->RenderSettings.bVisible = false;

			return NewViewport;
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
	CameraViewport->RenderSettingsICVFX.RuntimeFlags &= ~(ViewportRuntime_Unused);

	// Add viewport ICVFX usage as Incamera
	CameraViewport->RenderSettingsICVFX.RuntimeFlags |= ViewportRuntime_ICVFXIncamera;

	return CameraViewport;
}

#if WITH_EDITOR
TArray<FDisplayClusterViewport*> FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewGetRenderedInCameraViewports(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent, bool bGetChromakey)
{
	TArray<FDisplayClusterViewport*> OutViewports;

	// Search for rendered camera viewport on other cluster nodes
	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(InRootActor);
	if (ViewportManager != nullptr)
	{
		const FString ICVFXCameraId = InCameraComponent.GetCameraUniqueId();
		const EDisplayClusterViewportRuntimeICVFXFlags InMask = bGetChromakey ? ViewportRuntime_ICVFXChromakey : ViewportRuntime_ICVFXIncamera;
		const FString ViewportTypeId = bGetChromakey ? DisplayClusterViewportStrings::icvfx::chromakey : DisplayClusterViewportStrings::icvfx::camera;

		for (FDisplayClusterViewport* ViewportIt : ViewportManager->ImplGetWholeClusterViewports_Editor())
		{
			if (ViewportIt != nullptr
				&& (ViewportIt->RenderSettingsICVFX.RuntimeFlags & InMask) != 0
				&& (ViewportIt->InputShaderResources.Num() > 0 && ViewportIt->InputShaderResources[0] != nullptr && ViewportIt->Contexts.Num() > 0)
				&& (ViewportIt->RenderSettings.OverrideViewportId.IsEmpty()))
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

	const FDisplayClusterRenderFrameSettings& RenderFrameSettingsConstRef = InCameraViewport.GetRenderFrameSettings();
	switch (RenderFrameSettingsConstRef.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Only for preview mode
		break;
	default:
		return;
	}

	for (FDisplayClusterViewport* ViewportIt : PreviewGetRenderedInCameraViewports(InRootActor, InCameraComponent, false))
	{
		if (ViewportIt && ViewportIt->GetClusterNodeId() != InCameraViewport.GetClusterNodeId()
			&& FDisplayClusterViewportConfigurationHelpers_OpenColorIO::IsInnerFrustumViewportSettingsEqual_Editor(*ViewportIt, InCameraViewport, InCameraComponent)
			&& FDisplayClusterViewportConfigurationHelpers_Postprocess::IsInnerFrustumViewportSettingsEqual_Editor(*ViewportIt, InCameraViewport, InCameraComponent))
		{
			// Reuse exist viewport:
			InCameraViewport.RenderSettings.OverrideViewportId = ViewportIt->GetId();
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

	const FDisplayClusterRenderFrameSettings& RenderFrameSettingsConstRef = InChromakeyViewport.GetRenderFrameSettings();
	switch (RenderFrameSettingsConstRef.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Only for preview mode
		break;
	default:
		return;
	}

	for (FDisplayClusterViewport* ViewportIt : PreviewGetRenderedInCameraViewports(InRootActor, InCameraComponent, true))
	{
		if (ViewportIt && ViewportIt->GetClusterNodeId() != InChromakeyViewport.GetClusterNodeId())
		{
			// Reuse exist viewport from other node
			InChromakeyViewport.RenderSettings.OverrideViewportId = ViewportIt->GetId();
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
	ChromakeyViewport->RenderSettingsICVFX.RuntimeFlags &= ~(ViewportRuntime_Unused);

	// Add viewport ICVFX usage as Chromakey
	ChromakeyViewport->RenderSettingsICVFX.RuntimeFlags |= ViewportRuntime_ICVFXChromakey;

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
	LightcardViewport->RenderSettingsICVFX.RuntimeFlags &= ~(ViewportRuntime_Unused);

	// Add viewport ICVFX usage as Lightcard
	LightcardViewport->RenderSettingsICVFX.RuntimeFlags |= ViewportRuntime_ICVFXLightcard;

	return LightcardViewport;
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

	FDisplayClusterShaderParameters_ICVFX::FCameraSettings Result;

	Result.Resource.ViewportId = InCameraViewport.GetId();
	Result.Local2WorldTransform = OriginComp->GetComponentTransform();

	const float RealThicknessScaleValue = 0.1f;
	Result.InnerCameraBorderColor = CameraSettings.Border.Color;
	Result.InnerCameraBorderThickness = CameraSettings.Border.Enable ? CameraSettings.Border.Thickness * RealThicknessScaleValue : 0.0f;
	Result.InnerCameraFrameAspectRatio = (float)CameraSettings.RenderSettings.CustomFrameSize.CustomWidth / (float)CameraSettings.RenderSettings.CustomFrameSize.CustomHeight;

	const FString InnerFrustumID = InCameraComponent.GetCameraUniqueId();
	const int32 CameraRenderOrder = RootActor.GetInnerFrustumPriority(InnerFrustumID);
	Result.RenderOrder = (CameraRenderOrder<0) ? CameraSettings.RenderSettings.RenderOrder : CameraRenderOrder;

	const float FieldOfViewMultiplier = ImplGetFieldOfViewMultiplier(CameraSettings);

	// softedge adjustments	
	const float Overscan = (FieldOfViewMultiplier > 0) ? FieldOfViewMultiplier : 1;
	
	// remap values from 0-1 GUI range into acceptable 0.0 - 0.25 shader range
	Result.SoftEdge.X = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0f), FVector2D(0.0, 0.25), CameraSettings.SoftEdge.Horizontal) / Overscan; // Left
	Result.SoftEdge.Y = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0f), FVector2D(0.0, 0.25), CameraSettings.SoftEdge.Vertical) / Overscan; // Top
	Result.SoftEdge.Z = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0f), FVector2D(0.0, 0.25), CameraSettings.SoftEdge.Horizontal) / Overscan; // right
	Result.SoftEdge.W = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0f), FVector2D(0.0, 0.25), CameraSettings.SoftEdge.Vertical) / Overscan; // bottom

	// default - percents
	const float ConvertToPercent = 0.01f;
	float Left = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Left * ConvertToPercent);
	float Right = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Right * ConvertToPercent);
	float Top = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Top * ConvertToPercent);
	float Bottom = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Bottom * ConvertToPercent);

	if (CameraSettings.CustomFrustum.Mode == EDisplayClusterConfigurationViewportCustomFrustumMode::Pixels)
	{
		float FrameWidth = RootActor.GetStageSettings().DefaultFrameSize.Width * CameraSettings.BufferRatio;
		float FrameHeight = RootActor.GetStageSettings().DefaultFrameSize.Height * CameraSettings.BufferRatio;

		if (CameraSettings.RenderSettings.CustomFrameSize.bUseCustomSize)
		{
			FrameWidth = CameraSettings.RenderSettings.CustomFrameSize.CustomWidth * CameraSettings.BufferRatio;
			FrameHeight = CameraSettings.RenderSettings.CustomFrameSize.CustomHeight * CameraSettings.BufferRatio;
		}

		Left = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Left / FrameWidth);
		Right = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Right / FrameWidth);
		Top = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Top / FrameHeight);
		Bottom = FDisplayClusterViewport_OverscanSettings::ClampPercent(CameraSettings.CustomFrustum.Bottom / FrameHeight);
	}

	// recalculate soft edge related offsets based on frustum
	Result.SoftEdge.X /= (1 + Left + Right);
	Result.SoftEdge.Y /= (1 + Top + Bottom);
	Result.SoftEdge.Z /= (1 + Left + Right);
	Result.SoftEdge.W /= (1 + Top + Bottom);

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
	if (!ImplUpdateCameraProjectionSettings(CameraProjectionPolicy, CameraSettings, InCameraComponent.GetCameraComponent()))
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
	const FDisplayClusterConfigurationICVFX_ChromakeySettings& ChromakeySettings = CameraSettings.Chromakey;
	if (ChromakeySettings.bEnable && !ChromakeySettings.ChromakeyRenderTexture.bEnable)
	{
		DstViewport.RenderSettings.bSkipRendering = true;
	}

	// Update camera viewport projection policy settings
	ImplUpdateCameraProjectionSettings(DstViewport.ProjectionPolicy, CameraSettings, InCameraComponent.GetCameraComponent());

	// OCIO
	FDisplayClusterViewportConfigurationHelpers_OpenColorIO::UpdateICVFXCameraViewport(DstViewport, RootActor, InCameraComponent);

	// Motion blur:
	const FDisplayClusterViewport_CameraMotionBlur CameraMotionBlurParameters = InCameraComponent.GetMotionBlurParameters();
	DstViewport.CameraMotionBlur.BlurSetup = CameraMotionBlurParameters;

	// FDisplayClusterConfigurationICVFX_CameraSettings
	DstViewport.RenderSettings.CameraId.Empty();

	// UDisplayClusterConfigurationICVFX_CameraRenderSettings
	FIntPoint DesiredSize(0);
	// Camera viewport frame size:
	if (CameraSettings.RenderSettings.CustomFrameSize.bUseCustomSize)
	{
		DesiredSize.X = CameraSettings.RenderSettings.CustomFrameSize.CustomWidth;
		DesiredSize.Y = CameraSettings.RenderSettings.CustomFrameSize.CustomHeight;
	}
	else
	{
		DesiredSize.X = StageSettings.DefaultFrameSize.Width;
		DesiredSize.Y = StageSettings.DefaultFrameSize.Height;
	}

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
	UpdateCameraViewportBufferRatio(DstViewport, CameraSettings);

	// Set media related configuration (runtime only for now)
	if (IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
	{
		const FDisplayClusterConfigurationMedia& MediaSettings = InCameraComponent.CameraSettings.RenderSettings.Media;

		const FString ThisClusterNodeId = DstViewport.GetClusterNodeId();
		const bool bThisNodeSharesMedia = MediaSettings.IsMediaSharingUsed() && MediaSettings.MediaSharingNode.Equals(ThisClusterNodeId, ESearchCase::IgnoreCase);

		// Don't render the viewport if media input assigned
		DstViewport.RenderSettings.bSkipSceneRenderingButLeaveResourcesAvailable = MediaSettings.IsMediaSharingUsed() ?
			!bThisNodeSharesMedia :
			!!MediaSettings.MediaSource;

		// Mark this viewport is going to be captured by a capture device
		DstViewport.RenderSettings.bIsBeingCaptured = MediaSettings.IsMediaSharingUsed() ?
			bThisNodeSharesMedia :
			!!MediaSettings.MediaOutput;
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateChromakeyViewportSettings(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& InCameraViewport, ADisplayClusterRootActor& RootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
	const FDisplayClusterConfigurationICVFX_ChromakeySettings& ChromakeySettings = CameraSettings.Chromakey;

	check(ChromakeySettings.bEnable && ChromakeySettings.ChromakeyRenderTexture.bEnable);

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// Chromakey used as overlay
	DstViewport.RenderSettings.bVisible = false;

	// Use special capture mode (this change RTT format and render flags)
	DstViewport.RenderSettings.CaptureMode = EDisplayClusterViewportCaptureMode::Chromakey;

	// UDisplayClusterConfigurationICVFX_ChromakeyRenderSettings
	const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings& InRenderSettings = ChromakeySettings.ChromakeyRenderTexture;
	{
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_Override(DstViewport, InRenderSettings.Replace);
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_PostprocessBlur(DstViewport, InRenderSettings.PostprocessBlur);
		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_GenerateMips(DstViewport, InRenderSettings.GenerateMips);

		// Update visibility settings only for rendered viewports
		if (!DstViewport.PostRenderSettings.Replace.IsEnabled())
		{
			check(FDisplayClusterViewportConfigurationHelpers_Visibility::IsValid(InRenderSettings.ShowOnlyList));

			FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(DstViewport, RootActor, InRenderSettings.ShowOnlyList);
		}
	}

	FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_OverlayRenderSettings(DstViewport, InRenderSettings.AdvancedRenderSettings);

	// Support inner camera custom frustum
	FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraCustomFrustum(DstViewport, CameraSettings.CustomFrustum);

	// Attach to parent viewport
	DstViewport.RenderSettings.AssignParentViewport(InCameraViewport.GetId(), InCameraViewport.RenderSettings);

	// Support custom overlay size
	if (ChromakeySettings.ChromakeyRenderTexture.CustomSize.bUseCustomSize)
	{
		FIntPoint DesiredSize;
		DesiredSize.X = ChromakeySettings.ChromakeyRenderTexture.CustomSize.CustomWidth;
		DesiredSize.Y = ChromakeySettings.ChromakeyRenderTexture.CustomSize.CustomHeight;

		DstViewport.RenderSettings.Rect = DstViewport.GetValidRect(FIntRect(FIntPoint(0, 0), DesiredSize), TEXT("Configuration custom chromakey Frame Size"));
	}

	// Debug: override the texture of the target viewport from this chromakeyRTT
	if (ChromakeySettings.ChromakeyRenderTexture.bReplaceCameraViewport)
	{
		InCameraViewport.RenderSettings.OverrideViewportId = DstViewport.GetId();
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_Chromakey(FDisplayClusterShaderParameters_ICVFX::FCameraSettings& InOutCameraSettings, const FDisplayClusterConfigurationICVFX_ChromakeySettings& InChromakeySettings, FDisplayClusterViewport* InChromakeyViewport)
{
	if (InChromakeySettings.bEnable)
	{
		if (InChromakeySettings.ChromakeyRenderTexture.bEnable)
		{
			if (InChromakeySettings.ChromakeyRenderTexture.bReplaceCameraViewport)
			{
				InOutCameraSettings.ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled;
				return;
			}
			else
			{
				check(InChromakeyViewport);

				InOutCameraSettings.ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::ChromakeyLayers;
				InOutCameraSettings.Chromakey.ViewportId = InChromakeyViewport->GetId();
				InOutCameraSettings.ChromakeyColor = InChromakeySettings.ChromakeyColor;
			}
		}
		else
		{
			InOutCameraSettings.ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::FrameColor;
			InOutCameraSettings.ChromakeyColor = InChromakeySettings.ChromakeyColor;
		}
	}
	else
	{
		InOutCameraSettings.ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled;
	}
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_ChromakeyMarkers(FDisplayClusterShaderParameters_ICVFX::FCameraSettings& InOutCameraSettings, const FDisplayClusterConfigurationICVFX_ChromakeyMarkers& InChromakeyMarkers)
{
	InOutCameraSettings.ChromakeMarkerTextureRHI.SafeRelease();

	if (InChromakeyMarkers.bEnable && InChromakeyMarkers.MarkerTileRGBA != nullptr)
	{
		InOutCameraSettings.ChromakeyMarkersColor = InChromakeyMarkers.MarkerColor;
		InOutCameraSettings.ChromakeyMarkersScale = InChromakeyMarkers.MarkerSizeScale;
		InOutCameraSettings.ChromakeyMarkersDistance = InChromakeyMarkers.MarkerTileDistance;
		InOutCameraSettings.ChromakeyMarkersOffset = InChromakeyMarkers.MarkerTileOffset;

		// Assign texture RHI ref
		FTextureResource* MarkersResource = InChromakeyMarkers.MarkerTileRGBA->GetResource();
		if (MarkersResource)
		{
			InOutCameraSettings.ChromakeMarkerTextureRHI = MarkersResource->TextureRHI;
		}
	}
}

bool FDisplayClusterViewportConfigurationHelpers_ICVFX::IsShouldUseLightcard(const FDisplayClusterConfigurationICVFX_LightcardSettings& InLightcardSettings)
{
	if (InLightcardSettings.bEnable == false)
	{
		// dont use lightcard if disabled
		return false;
	}

	if (InLightcardSettings.RenderSettings.Replace.bAllowReplace)
	{
		if (InLightcardSettings.RenderSettings.Replace.SourceTexture == nullptr)
		{
			// LightcardSettings.Override require source texture.
			return false;
		}

		return true;
	}

	// Lightcard require layers for render
	return FDisplayClusterViewportConfigurationHelpers_Visibility::IsValid(InLightcardSettings.ShowOnlyList);
}

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(FDisplayClusterViewport& DstViewport, FDisplayClusterViewport& BaseViewport, ADisplayClusterRootActor& RootActor)
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
	const FDisplayClusterConfigurationICVFX_LightcardSettings& LightcardSettings = StageSettings.Lightcard;

	check(LightcardSettings.bEnable);

	// Reset runtime flags from prev frame:
	DstViewport.ResetRuntimeParameters();

	// LIghtcard texture used as overlay
	DstViewport.RenderSettings.bVisible = false;

	FDisplayClusterViewportConfigurationHelpers_Postprocess::UpdateLightcardPostProcessSettings(DstViewport, BaseViewport, RootActor);
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
			check(FDisplayClusterViewportConfigurationHelpers_Visibility::IsValid(LightcardSettings.ShowOnlyList));

			FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(DstViewport, RootActor, LightcardSettings.ShowOnlyList);
		}

		FDisplayClusterViewportConfigurationHelpers::UpdateViewportSetting_OverlayRenderSettings(DstViewport, InRenderSettings.AdvancedRenderSettings);
	}

	// Attach to parent viewport
	DstViewport.RenderSettings.AssignParentViewport(BaseViewport.GetId(), BaseViewport.RenderSettings);

	// Global lighcard rendering mode
	if ((BaseViewport.RenderSettingsICVFX.Flags & ViewportICVFX_OverrideLightcardMode) == 0)
	{
		// Use global lightcard blending mode
		switch (LightcardSettings.Blendingmode)
		{
		default:
		case EDisplayClusterConfigurationICVFX_LightcardRenderMode::Over:
			BaseViewport.RenderSettingsICVFX.ICVFX.LightcardMode = EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Over;
			break;
		case EDisplayClusterConfigurationICVFX_LightcardRenderMode::Under:
			BaseViewport.RenderSettingsICVFX.ICVFX.LightcardMode = EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Under;
			break;
		};
	}
	// Debug: override the texture of the target viewport from this lightcard RTT
	if (InRenderSettings.bReplaceViewport)
	{
		BaseViewport.RenderSettings.OverrideViewportId = DstViewport.GetId();
	}
	else
	{
		BaseViewport.RenderSettingsICVFX.ICVFX.Lightcard.ViewportId = DstViewport.GetId();
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

void FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportBufferRatio(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings)
{
	float BufferRatio = CameraSettings.BufferRatio;

	if (CameraSettings.CustomFrustum.bAdaptResolution)
	{
		const float FieldOfViewMultiplier = ImplGetFieldOfViewMultiplier(CameraSettings);

		// adapt resolution should work as a shortcut to improve rendering quality
		DstViewport.RenderSettings.RenderTargetAdaptRatio = FieldOfViewMultiplier;
	}
	else
	{
		// Don't use an adaptive resolution multiplier
		DstViewport.RenderSettings.RenderTargetAdaptRatio = 1.f;
	}

	DstViewport.Owner.SetViewportBufferRatio(DstViewport, BufferRatio);
}
