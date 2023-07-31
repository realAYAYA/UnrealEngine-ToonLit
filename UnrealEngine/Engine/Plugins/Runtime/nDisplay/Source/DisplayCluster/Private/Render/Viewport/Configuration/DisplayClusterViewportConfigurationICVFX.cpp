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

#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Misc/DisplayClusterLog.h"

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

	void AssignChromakeyToTargetViewport(FDisplayClusterViewport& DstViewport, UDisplayClusterICVFXCameraComponent& InCameraComponent)
	{
		if ((DstViewport.RenderSettingsICVFX.Flags & ViewportICVFX_DisableChromakey) != 0)
		{
			// chromakey disabled for this viewport
			return;
		}

		const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCameraComponent.GetCameraSettingsICVFX();
		FDisplayClusterShaderParameters_ICVFX::FCameraSettings& DstCameraData = DstViewport.RenderSettingsICVFX.ICVFX.Cameras.Last();

		FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_Chromakey(DstCameraData, CameraSettings.Chromakey, ChromakeyViewport);

		if ((DstViewport.RenderSettingsICVFX.Flags & ViewportICVFX_DisableChromakeyMarkers) == 0)
		{
			FDisplayClusterViewportConfigurationHelpers_ICVFX::UpdateCameraSettings_ChromakeyMarkers(DstCameraData, CameraSettings.Chromakey.ChromakeyMarkers);
		}
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
			bool bAllowChromakey = false;

			FDisplayClusterShaderParameters_ICVFX::FCameraSettings ShaderParametersCameraSettings;
			if (CameraConfiguration.GetShaderParametersCameraSettings(RootActor, CameraComponent, ShaderParametersCameraSettings))
			{
				// Add this camera data to all visible targets:
				for (FDisplayClusterViewport* ViewportIt : VisibleTargets)
				{
					ViewportIt->RenderSettingsICVFX.ICVFX.Cameras.Add(ShaderParametersCameraSettings);

					// At lest one target must accept chromakey
					bAllowChromakey |= (ViewportIt->RenderSettingsICVFX.Flags & ViewportICVFX_DisableChromakey) == 0;
				}
			}

			// Create and assign chromakey for all targets for this camera
			if (bAllowChromakey)
			{
				CreateChromakey();
			}
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
	bool CreateChromakey()
	{
		const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = CameraComponent.GetCameraSettingsICVFX();
		const FDisplayClusterConfigurationICVFX_ChromakeySettings& ChromakeySettings = CameraSettings.Chromakey;

		// Try create chromakey render on demand
		if (ChromakeySettings.bEnable && ChromakeySettings.ChromakeyRenderTexture.bEnable)
		{
			if (ChromakeySettings.ChromakeyRenderTexture.Replace.bAllowReplace && ChromakeySettings.ChromakeyRenderTexture.Replace.SourceTexture == nullptr)
			{
				// ChromakeyRenderTexture.Override require source texture.
				return false;
			}

			bool bIsChromakeyHasAnyRenderComponent = FDisplayClusterViewportConfigurationHelpers_Visibility::IsValid(ChromakeySettings.ChromakeyRenderTexture.ShowOnlyList);
			if (ChromakeySettings.ChromakeyRenderTexture.Replace.bAllowReplace == false && !bIsChromakeyHasAnyRenderComponent)
			{
				// ChromakeyRenderTexture requires actors for render.
				return false;
			}

			if (!CameraConfiguration.CreateChromakeyViewport(RootActor, CameraComponent))
			{
				return false;
			}
		}

		// Assign this chromakey to all supported targets
		for (FDisplayClusterViewport* TargetIt : VisibleTargets)
		{
			if (TargetIt)
			{
				CameraConfiguration.AssignChromakeyToTargetViewport(*TargetIt, CameraComponent);
			}
		}

		return true;
	}

public:
	// List of targets for this camera
	TArray<FDisplayClusterViewport*> VisibleTargets;

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

void FDisplayClusterViewportConfigurationICVFX::Update()
{
	const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = RootActor.GetStageSettings();

	FDisplayClusterViewportManager* ViewportManager = FDisplayClusterViewportConfigurationHelpers_ICVFX::GetViewportManager(RootActor);
	if (ViewportManager)
	{
		ImplBeginReallocateViewports(*ViewportManager);

		// Disable ICVFX if cluster node rendering is not used
		const FString& ClusterNodeId = ViewportManager->GetRenderFrameSettings().ClusterNodeId;
		if(ClusterNodeId.IsEmpty())
		{
			// The nDisplay viewport should now always have the name of the cluster node.
			// When rendering MRQ viewports, the list of viewports is used without cluster node names.
			ImplFinishReallocateViewports(*ViewportManager);
			return;
		}

		TArray<FDisplayClusterViewport*> TargetViewports;
		const EDisplayClusterViewportICVFXFlags TargetViewportsFlags = ImplGetTargetViewports(*ViewportManager, TargetViewports);

		// Find ICVFX target viewports
		if (TargetViewports.Num() > 0)
		{
			// ICVFX used

			// If not all viewports disable camera:
			// Collect all ICVFX cameras from stage
			if ((TargetViewportsFlags & ViewportICVFX_DisableCamera) == 0)
			{
				ImplGetCameras();
			}

			// Allocate and assign camera resources
			if (StageCameras.Num() > 0)
			{
				// Collect visible targets for cameras:
				for (FDisplayClusterViewport* TargetIt : TargetViewports)
				{
					// Target viewpot must support camera render:
					if ((TargetIt->RenderSettingsICVFX.Flags & ViewportICVFX_DisableCamera) == 0)
					{
						// Add this target to all cameras visible on it
						for (FDisplayClusterViewportConfigurationCameraICVFX& CameraIt : StageCameras)
						{
						if (CameraIt.IsCameraProjectionVisibleOnViewport(TargetIt)
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
			if ((TargetViewportsFlags & ViewportICVFX_DisableLightcard) == 0)
			{
				// Allocate and assign lightcard resources
				if (FDisplayClusterViewportConfigurationHelpers_ICVFX::IsShouldUseLightcard(StageSettings.Lightcard))
				{
					for (FDisplayClusterViewport* TargetIt : TargetViewports)
					{
						// only for support targets
						if (TargetIt && (TargetIt->RenderSettingsICVFX.Flags & ViewportICVFX_DisableLightcard) == 0)
						{
							CreateLightcardViewport(*TargetIt);
						}
					}
				}
			}

			for (FDisplayClusterViewport* TargetIt : TargetViewports)
			{
				if (TargetIt)
				{
					// Sort cameras by render order for each target
					TargetIt->RenderSettingsICVFX.ICVFX.SortCamerasRenderOrder();
				}
			}

			if (StageSettings.bFreezeRenderOuterViewports)
			{
				// Freeze rendering for outer viewports
				for (FDisplayClusterViewport* TargetIt : TargetViewports)
				{
					TargetIt->RenderSettings.bFreezeRendering = true;
				}

				// Freeze render for lightcards when outer viewports freezed
				if (StageSettings.Lightcard.bIgnoreOuterViewportsFreezingForLightcards == false)
				{
					const EDisplayClusterViewportRuntimeICVFXFlags LightcardViewportMask = ViewportRuntime_ICVFXLightcard;

					for (FDisplayClusterViewport* ViewportIt : ViewportManager->ImplGetViewports())
					{
						if (ViewportIt && (ViewportIt->RenderSettingsICVFX.RuntimeFlags & LightcardViewportMask) != 0)
						{
							ViewportIt->RenderSettings.bFreezeRendering = true;
						}
					}
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
			for (FDisplayClusterViewport* ViewportIt : ViewportManager->ImplGetViewports())
			{
				ViewportIt->RenderSettings.bWarpBlendRenderAlphaChannel = true;
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
	TArray<FDisplayClusterViewport*> ICVFXViewports;

	// Collect viewports, that use ICVFX hide list
	for (FDisplayClusterViewport* ViewportIt : ViewportManager.ImplGetViewports())
	{
		const bool bInternalResource = (ViewportIt->RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_InternalResource) != 0;
		const bool bIsInCamera = (ViewportIt->RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_ICVFXIncamera) != 0;
		const bool bICVFX_Enable = (ViewportIt->RenderSettingsICVFX.Flags & ViewportICVFX_Enable) != 0;

		if ((bICVFX_Enable && !bInternalResource)  || (bInternalResource && bIsInCamera))
		{
			ICVFXViewports.Add(ViewportIt);
		}
	}

	// Update hide list for all icvfx viewports
	FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateHideList_ICVFX(ICVFXViewports, RootActor);
}

void FDisplayClusterViewportConfigurationICVFX::ImplBeginReallocateViewports(FDisplayClusterViewportManager& ViewportManager)
{
	for (FDisplayClusterViewport* ViewportIt : ViewportManager.ImplGetViewports())
	{
		// Runtime icvfx viewport support reallocate feature:
		if ((ViewportIt->RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_InternalResource) != 0)
		{
			// Mark all dynamic ICVFX viewports for delete
			ViewportIt->RenderSettingsICVFX.RuntimeFlags |= ViewportRuntime_Unused;
		}
	}
}

void FDisplayClusterViewportConfigurationICVFX::ImplFinishReallocateViewports(FDisplayClusterViewportManager& ViewportManager)
{
	TArray<FDisplayClusterViewport*> UnusedViewports;

	// Collect all unused viewports for remove
	for (FDisplayClusterViewport* ViewportIt : ViewportManager.ImplGetViewports())
	{
		if ((ViewportIt->RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_Unused) != 0)
		{
			UnusedViewports.Add(ViewportIt);
		}
	}

	// Delete unused viewports:
	for (FDisplayClusterViewport* RemoveViewportIt : UnusedViewports)
	{
		ViewportManager.DeleteViewport(RemoveViewportIt->GetId());
	}

	UnusedViewports.Empty();
}

void FDisplayClusterViewportConfigurationICVFX::ImplGetCameras()
{
	for (UActorComponent* ActorComponentIt : RootActor.GetComponents())
	{
		// Try to create ICVFX camera from component:
		if (ActorComponentIt)
		{
			UDisplayClusterICVFXCameraComponent* CineCameraComponent = Cast<UDisplayClusterICVFXCameraComponent>(ActorComponentIt);
			if (CineCameraComponent && CineCameraComponent->IsICVFXEnabled())
			{
				const FString InnerFrustumID = CineCameraComponent->GetCameraUniqueId();
				if (RootActor.IsInnerFrustumEnabled(InnerFrustumID))
				{
					FDisplayClusterViewportConfigurationCameraICVFX NewCamera(RootActor, *CineCameraComponent);
					if (NewCamera.Initialize())
					{
						StageCameras.Add(NewCamera);
					}
				}
			}
		}
	}
}

EDisplayClusterViewportICVFXFlags FDisplayClusterViewportConfigurationICVFX::ImplGetTargetViewports(FDisplayClusterViewportManager& ViewportManager, TArray<class FDisplayClusterViewport*>& OutTargets)
{
	EDisplayClusterViewportICVFXFlags InvFlags = ViewportICVFX_None;

	// Collect invertet disable flags from all target viewports
	for (FDisplayClusterViewport* ViewportIt : ViewportManager.ImplGetViewports())
	{
		// Process only external viewports:
		if ((ViewportIt->RenderSettingsICVFX.RuntimeFlags & ViewportRuntime_InternalResource) == 0)
		{
			//Raise new projection target if possible
			if (ViewportIt->RenderSettings.bEnable && (ViewportIt->RenderSettingsICVFX.Flags & ViewportICVFX_Enable) != 0)
			{
				if (ViewportIt->ProjectionPolicy.IsValid() && ViewportIt->ProjectionPolicy->ShouldSupportICVFX())
				{
					// Collect this viewport ICVFX target
					OutTargets.Add(ViewportIt);

					// proj policy support ICVFX, Use this viewport as icvfx target
					ViewportIt->RenderSettingsICVFX.RuntimeFlags |= ViewportRuntime_ICVFXTarget;

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
