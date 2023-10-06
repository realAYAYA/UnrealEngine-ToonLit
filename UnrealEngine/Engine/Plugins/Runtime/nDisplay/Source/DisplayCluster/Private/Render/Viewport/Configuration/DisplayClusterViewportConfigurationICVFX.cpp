// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationICVFX.h"

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

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationCameraViewport
////////////////////////////////////////////////////////////////////////////////
class FDisplayClusterViewportConfigurationCameraViewport
{
public:
	bool Initialize(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
	{
		return FDisplayClusterViewportConfigurationHelpers_ICVFX::GetCameraContext(InRootActor, InCameraComponent, CameraContext);
	}

	bool CreateCameraViewport(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
	{
		CameraViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateCameraViewport(InRootActor, InCameraComponent);

		if (CameraViewport)
		{
			// overlay rendered only for enabled incamera
			const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
			check(CameraSettings.bEnable);

			// Update camera viewport settings
			FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraViewportSettings(*CameraViewport, InRootActor, InCameraComponent);

			// Support projection policy update
			FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(*CameraViewport);

#if WITH_EDITOR
			// Reuse for EditorPreview
			FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseInnerFrustumViewportWithinClusterNodes_Editor(*CameraViewport, InRootActor, InCameraComponent);
#endif

			return true;
		}

		return false;
	}

	bool CreateChromakeyViewport(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
	{
		check(CameraViewport);

		// Create new chromakey viewport
		ChromakeyViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateChromakeyViewport(InRootActor, InCameraComponent);

		if (ChromakeyViewport)
		{
			// Update chromakey viewport settings
			FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateChromakeyViewportSettings(*ChromakeyViewport, *CameraViewport, InRootActor, InCameraComponent);

			// Support projection policy update
			FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(*ChromakeyViewport);

#if WITH_EDITOR
			// reuse for EditorPreview
			FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewReuseChromakeyViewportWithinClusterNodes_Editor(*ChromakeyViewport, InRootActor, InCameraComponent);
#endif

			return true;
		}

		return false;
	}

	bool IsCameraProjectionVisibleOnViewport(FDisplayClusterViewport* TargetViewport)
	{
		if (TargetViewport && TargetViewport->ProjectionPolicy.IsValid())
		{
			// Currently, only mono context is supported to check the visibility of the inner camera.
			if(TargetViewport->ProjectionPolicy->IsCameraProjectionVisible(CameraContext.ViewRotation, CameraContext.ViewLocation, CameraContext.PrjMatrix))
			{
				return true;
			}
		}
		
		// do not use camera for this viewport
		return false;
	}

	bool GetShaderParametersCameraSettings(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent, FDisplayClusterShaderParameters_ICVFX::FCameraSettings& OutShaderParametersCameraSettings) const
	{
		if (CameraViewport)
		{
			OutShaderParametersCameraSettings = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetShaderParametersCameraSettings(*CameraViewport, InRootActor, InCameraComponent);
			return true;
		}
		
		return false;
	}

public:
	// Camera context, used for visibility test vs outer
	FCameraContext_ICVFX CameraContext;

	FDisplayClusterViewport* CameraViewport = nullptr;
	FDisplayClusterViewport* ChromakeyViewport = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationCameraICVFX
////////////////////////////////////////////////////////////////////////////////

class FDisplayClusterViewportConfigurationCameraICVFX
{
public:
	
	FDisplayClusterViewportConfigurationCameraICVFX(ADisplayClusterRootActor& InRootActor, UDisplayClusterICVFXCameraComponent& InCameraComponent)
		: RootActor(InRootActor)
		, CameraComponent(InCameraComponent)
	{}

public:
	// Create camera resources and intialize targets
	void Update()
	{
		const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = CameraComponent.GetCameraSettingsICVFX();
		if (CameraConfiguration.CreateCameraViewport(RootActor, CameraComponent))
		{
			FDisplayClusterShaderParameters_ICVFX::FCameraSettings ShaderParametersCameraSettings;
			if (CameraConfiguration.GetShaderParametersCameraSettings(RootActor, CameraComponent, ShaderParametersCameraSettings))
			{
				// Add this camera data to all visible targets:
				for (TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : VisibleTargets)
				{
					if (ViewportIt.IsValid())
					{
						ViewportIt->RenderSettingsICVFX.ICVFX.Cameras.Add(ShaderParametersCameraSettings);
					}
				}
			}

			// Create and assign chromakey for all targets for this camera
			CreateAndSetupInCameraChromakey();
		}
	}

	bool Initialize()
	{
		return CameraConfiguration.Initialize(RootActor, CameraComponent);
	}

	bool IsCameraProjectionVisibleOnViewport(FDisplayClusterViewport* TargetViewport)
	{
		return CameraConfiguration.IsCameraProjectionVisibleOnViewport(TargetViewport);
	}

	const FDisplayClusterConfigurationICVFX_CameraSettings& GetCameraSettings() const
	{
		return CameraComponent.GetCameraSettingsICVFX();
	}

	FDisplayClusterViewport* GetCameraViewport() const
	{
		return CameraConfiguration.CameraViewport;
	}

protected:
	bool CreateAndSetupInCameraChromakey()
	{
		const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();
		const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = CameraComponent.GetCameraSettingsICVFX();
		const FDisplayClusterConfigurationICVFX_ChromakeySettings& ChromakeySettings = CameraSettings.Chromakey;

		// Try create chromakey render on demand
		if(const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* ChromakeyRenderSettings = ChromakeySettings.GetChromakeyRenderSettings(StageSettings))
		{
			if (ChromakeyRenderSettings->ShouldUseChromakeyViewport(StageSettings))
			{
				CameraConfiguration.CreateChromakeyViewport(RootActor, CameraComponent);
			}
		}

		// Chromakey viewport name with alpha channel
		const FString ChromakeyViewportId(CameraConfiguration.ChromakeyViewport ? CameraConfiguration.ChromakeyViewport->GetId() : TEXT(""));

		// Assign this chromakey to all supported targets
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& OuterViewportIt : VisibleTargets)
		{
			if (OuterViewportIt.IsValid())
			{
				FDisplayClusterShaderParameters_ICVFX::FCameraSettings& DstCameraData = OuterViewportIt->RenderSettingsICVFX.ICVFX.Cameras.Last();

				const bool bEnableChromakey = !EnumHasAnyFlags(OuterViewportIt->RenderSettingsICVFX.Flags, EDisplayClusterViewportICVFXFlags::DisableChromakey);
				const bool bEnableChromakeyMarkers = !EnumHasAnyFlags(OuterViewportIt->RenderSettingsICVFX.Flags, EDisplayClusterViewportICVFXFlags::DisableChromakeyMarkers);

				if (bEnableChromakey)
				{
					// Setup chromakey with markers
					FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_Chromakey(DstCameraData, StageSettings, CameraSettings, bEnableChromakeyMarkers, ChromakeyViewportId);
				}

				// Setup overlap chromakey with markers
				FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_OverlapChromakey(DstCameraData, StageSettings, CameraSettings, bEnableChromakeyMarkers);
			}
		}

		return true;
	}

public:
	// List of targets for this camera
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> VisibleTargets;

private:
	FDisplayClusterViewportConfigurationCameraViewport CameraConfiguration;

	ADisplayClusterRootActor& RootActor;
	UDisplayClusterICVFXCameraComponent& CameraComponent;
};

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationICVFX
///////////////////////////////////////////////////////////////////
FDisplayClusterViewportConfigurationICVFX::FDisplayClusterViewportConfigurationICVFX(ADisplayClusterRootActor& InRootActor)
	: RootActor(InRootActor)
	, StageCameras(*(new TArray<class FDisplayClusterViewportConfigurationCameraICVFX>()))
{
}

FDisplayClusterViewportConfigurationICVFX::~FDisplayClusterViewportConfigurationICVFX()
{
	delete (&StageCameras);
}

bool FDisplayClusterViewportConfigurationICVFX::CreateLightcardViewport(FDisplayClusterViewport& BaseViewport)
{
	FDisplayClusterViewport* LightcardViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateLightcardViewport(BaseViewport, RootActor);
	if (LightcardViewport)
	{
		// Update lightcard viewport settings
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(*LightcardViewport, BaseViewport, RootActor);

		// Support projection policy update
		FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(*LightcardViewport);

		return true;
	}

	return false;
}

bool FDisplayClusterViewportConfigurationICVFX::CreateUVLightcardViewport(FDisplayClusterViewport& BaseViewport)
{
	FDisplayClusterViewport* UVLightCardViewport = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetOrCreateUVLightcardViewport(BaseViewport, RootActor);
	if (UVLightCardViewport)
	{
		// Update UV LightCard viewport settings
		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateLightcardViewportSetting(*UVLightCardViewport, BaseViewport, RootActor);

		// Support projection policy update
		FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(*UVLightCardViewport);

		// Optimize: re-use UVLightCard viewports with equals OCIO
		FDisplayClusterViewportConfigurationHelpers_ICVFX::ReuseUVLightCardViewportWithinClusterNode(*UVLightCardViewport);

		return true;
	}

	return false;
}

void FDisplayClusterViewportConfigurationICVFX::Update()
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor);
	if (ViewportManager)
	{
		ImplBeginReallocateViewports(*ViewportManager);

		const FDisplayClusterRenderFrameSettings& RenderFrameSettings = ViewportManager->GetRenderFrameSettings();
		
		// Disable ICVFX if cluster node rendering is not used
		const FString& ClusterNodeId = RenderFrameSettings.ClusterNodeId;
		if(ClusterNodeId.IsEmpty())
		{
			// The nDisplay viewport should now always have the name of the cluster node.
			// When rendering MRQ viewports, the list of viewports is used without cluster node names.
			ImplFinishReallocateViewports(*ViewportManager);
			return;
		}

		TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> TargetViewports;
		const EDisplayClusterViewportICVFXFlags TargetViewportsFlags = ImplGetTargetViewports(*ViewportManager, TargetViewports);

		// Find ICVFX target viewports
		if (TargetViewports.Num() > 0)
		{
			// ICVFX used

			// If not all viewports disable camera:
			// Collect all ICVFX cameras from stage
			if (!EnumHasAnyFlags(TargetViewportsFlags, EDisplayClusterViewportICVFXFlags::DisableCamera))
			{
				ImplGetCameras();
			}

			// Allocate and assign camera resources
			if (StageCameras.Num() > 0)
			{
				// Collect visible targets for cameras:
				for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : TargetViewports)
				{
					// Target viewpot must support camera render:
					if (TargetIt.IsValid() && !EnumHasAnyFlags(TargetIt->RenderSettingsICVFX.Flags, EDisplayClusterViewportICVFXFlags::DisableCamera))
					{
						// Add this target to all cameras visible on it
						for (FDisplayClusterViewportConfigurationCameraICVFX& CameraIt : StageCameras)
						{
						if (CameraIt.IsCameraProjectionVisibleOnViewport(TargetIt.Get())
							&& !CameraIt.GetCameraSettings().HiddenICVFXViewports.ItemNames.Contains(TargetIt->GetId()))
							{
								CameraIt.VisibleTargets.Add(TargetIt);
							}
						}
					}
				}

				// Create camera resources and initialize target ICVFX viewports
				for (FDisplayClusterViewportConfigurationCameraICVFX& CameraIt : StageCameras)
				{
					if (CameraIt.VisibleTargets.Num() > 0)
					{
						CameraIt.Update();
					}
				}
			}

			// If not all viewports disable lightcard
			if (!EnumHasAnyFlags(TargetViewportsFlags, EDisplayClusterViewportICVFXFlags::DisableLightcard))
			{
				// UVLightCard must be enabled in LC manager
				const bool bUVLightCardEnabled = ViewportManager->GetLightCardManager().IsValid() && ViewportManager->GetLightCardManager()->IsUVLightCardEnabled();
					
				// Allocate and assign lightcard resources
				const bool bUseLightCard = StageSettings.Lightcard.ShouldUseLightCard(StageSettings);
				const bool bUseUVLightCard = bUVLightCardEnabled && StageSettings.Lightcard.ShouldUseUVLightCard(StageSettings);

				for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : TargetViewports)
				{
					// only for support targets
					if (TargetIt.IsValid() && !EnumHasAnyFlags(TargetIt->RenderSettingsICVFX.Flags, EDisplayClusterViewportICVFXFlags::DisableLightcard))
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
					// Sort cameras by render order for each target
					TargetIt->RenderSettingsICVFX.ICVFX.SortCamerasRenderOrder();

					// Setup incamera overlap mode
					TargetIt->RenderSettingsICVFX.ICVFX.CameraOverlappingRenderMode = StageSettings.GetCameraOverlappingRenderMode();
				}
			}

			if (StageSettings.bFreezeRenderOuterViewports
#if WITH_EDITOR
				|| (RenderFrameSettings.bIsPreviewRendering && RenderFrameSettings.bFreezePreviewRender)
#endif
				)
			{
				// Freeze rendering for outer viewports
				for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& TargetIt : TargetViewports)
				{
					if (TargetIt.IsValid())
					{
						TargetIt->RenderSettings.bFreezeRendering = true;
					}
				}

				// Freeze render for lightcards when outer viewports freezed
				if (StageSettings.Lightcard.bIgnoreOuterViewportsFreezingForLightcards == false)
				{
					for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
					{
						if (ViewportIt.IsValid() && EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
						{
							ViewportIt->RenderSettings.bFreezeRendering = true;
						}
					}
				}
			}
		}
		else
		{
			// We need to be able to render inner views (ICVFX camera view) without any outers. Current pipeline has
			// a bunch of optimizations that block inner rendering if no outers available. To avoid the limitation
			// we force inner data initialization if no outers found.
			static const bool bIsRenderingOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));

			// Cherry-pick ICVFX cameras initialization from IF-block above
			if (bIsRenderingOffscreen)
			{
				ImplGetCameras();
				for (FDisplayClusterViewportConfigurationCameraICVFX& CameraIt : StageCameras)
				{
					CameraIt.Update();
				}
			}
		}

		ImplFinishReallocateViewports(*ViewportManager);
	}
}

void FDisplayClusterViewportConfigurationICVFX::PostUpdate()
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor);
	if (ViewportManager)
	{
		// Update visibility for icvfx viewports and cameras
		UpdateHideList(*ViewportManager);

		// Support additional hide list for icvfx cameras:
		UpdateCameraHideList(*ViewportManager);

		// Support alpha channel rendering for WarpBlend
		if (GDisplayClusterEnableAlphaChannelRendering != 0)
		{
			for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
			{
				if (ViewportIt.IsValid())
				{
					ViewportIt->RenderSettings.bWarpBlendRenderAlphaChannel = true;
				}
			}
		}
	}
}

void FDisplayClusterViewportConfigurationICVFX::UpdateCameraHideList(FDisplayClusterViewportManager& ViewportManager)
{
	for (FDisplayClusterViewportConfigurationCameraICVFX& CameraIt : StageCameras)
	{
		FDisplayClusterViewport* CameraViewport = CameraIt.GetCameraViewport();
		if (CameraViewport)
		{
			const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = CameraIt.GetCameraSettings();
			FDisplayClusterViewportConfigurationHelpers_Visibility::AppendHideList_ICVFX(*CameraViewport, RootActor, CameraSettings.CameraHideList);
		}
	}
}

void FDisplayClusterViewportConfigurationICVFX::UpdateHideList(FDisplayClusterViewportManager& ViewportManager)
{
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> ICVFXViewports;

	// Collect viewports, that use ICVFX hide list
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager.ImplGetCurrentRenderFrameViewports())
	{
		if (ViewportIt.IsValid())
		{
			const bool bInternalResource = EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource);
			const bool bIsInCamera = EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InCamera);
			const bool bICVFX_Enable = EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.Flags, EDisplayClusterViewportICVFXFlags::Enable);

			if ((bICVFX_Enable && !bInternalResource) || (bInternalResource && bIsInCamera))
			{
				ICVFXViewports.Add(ViewportIt);
			}
		}
	}

	// Update hide list for all icvfx viewports
	FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateHideList_ICVFX(ICVFXViewports, RootActor);
}

void FDisplayClusterViewportConfigurationICVFX::ImplBeginReallocateViewports(FDisplayClusterViewportManager& ViewportManager)
{
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager.ImplGetCurrentRenderFrameViewports())
	{
		// Runtime icvfx viewport support reallocate feature:
		if (ViewportIt.IsValid() && EnumHasAllFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
		{
			// Mark all dynamic ICVFX viewports for delete
			EnumAddFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused);
		}
	}
}

void FDisplayClusterViewportConfigurationICVFX::ImplFinishReallocateViewports(FDisplayClusterViewportManager& ViewportManager)
{
	TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> UnusedViewports;

	// Collect all unused viewports for remove
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager.ImplGetCurrentRenderFrameViewports())
	{
		if (ViewportIt.IsValid() && EnumHasAllFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Unused))
		{
			UnusedViewports.Add(ViewportIt);
		}
	}

	// Delete unused viewports:
	for (TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& RemoveViewportIt : UnusedViewports)
	{
		if (RemoveViewportIt.IsValid())
		{
			ViewportManager.DeleteViewport(RemoveViewportIt->GetId());
		}
	}

	UnusedViewports.Empty();
}

void FDisplayClusterViewportConfigurationICVFX::ImplGetCameras()
{
	// Get all ICVFX camera components
	TArray<UDisplayClusterICVFXCameraComponent*> ICVFXCameraComponents;
	RootActor.GetComponents(ICVFXCameraComponents);

	// Filter active cameras only
	for (UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent : ICVFXCameraComponents)
	{
		if (ICVFXCameraComponent && ICVFXCameraComponent->IsICVFXEnabled())
		{
			const FString InnerFrustumID = ICVFXCameraComponent->GetCameraUniqueId();
			if (RootActor.IsInnerFrustumEnabled(InnerFrustumID))
			{
				FDisplayClusterViewportConfigurationCameraICVFX NewCamera(RootActor, *ICVFXCameraComponent);
				if (NewCamera.Initialize())
				{
					StageCameras.Add(NewCamera);
				}
			}
		}
	}
}

EDisplayClusterViewportICVFXFlags FDisplayClusterViewportConfigurationICVFX::ImplGetTargetViewports(FDisplayClusterViewportManager& ViewportManager, TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& OutTargets)
{
	EDisplayClusterViewportICVFXFlags InvFlags = EDisplayClusterViewportICVFXFlags::None;

	// Collect invertet disable flags from all target viewports
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager.ImplGetCurrentRenderFrameViewports())
	{
		// Process only external viewports:
		if (ViewportIt.IsValid() && !EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
		{
			//Raise new projection target if possible
			if (ViewportIt->RenderSettings.bEnable && EnumHasAnyFlags(ViewportIt->RenderSettingsICVFX.Flags, EDisplayClusterViewportICVFXFlags::Enable))
			{
				if (ViewportIt->ProjectionPolicy.IsValid() && ViewportIt->ProjectionPolicy->ShouldSupportICVFX())
				{
					// Collect this viewport ICVFX target
					OutTargets.Add(ViewportIt);

					// proj policy support ICVFX, Use this viewport as icvfx target
					EnumAddFlags(ViewportIt->RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target);

					// Update targets use flags:
					InvFlags |= ~(ViewportIt->RenderSettingsICVFX.Flags);
				}
			}
		}
	}

	// Collect all targets disable flags
	return ~(InvFlags);
}

#if WITH_EDITOR
void FDisplayClusterViewportConfigurationICVFX::PostUpdatePreview_Editor(const FDisplayClusterPreviewSettings& InPreviewSettings)
{
}
#endif
