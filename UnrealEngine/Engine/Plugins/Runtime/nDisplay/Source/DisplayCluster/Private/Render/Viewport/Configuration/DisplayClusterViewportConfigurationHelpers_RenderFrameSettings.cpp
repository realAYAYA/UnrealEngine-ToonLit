// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_RenderFrameSettings.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/Preview/DisplayClusterViewportManagerPreview.h"

#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"

#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "Engine/RendererSettings.h"

//////////////////////////////////////////////////////////////////////////////////////////////
int32 GDisplayClusterPreviewAllowMultiGPURendering = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewAllowMultiGPURendering(
	TEXT("nDisplay.render.preview.AllowMultiGPURendering"),
	GDisplayClusterPreviewAllowMultiGPURendering,
	TEXT("Allow mGPU for preview rendering (0 == disabled)"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewMultiGPURenderingMinIndex = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewMultiGPURenderingMinIndex(
	TEXT("nDisplay.render.preview.MultiGPURenderingMinIndex"),
	GDisplayClusterPreviewMultiGPURenderingMinIndex,
	TEXT("Distribute mGPU render on GPU from #min to #max indices"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewMultiGPURenderingMaxIndex = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewMultiGPURenderingMaxIndex(
	TEXT("nDisplay.render.preview.MultiGPURenderingMaxIndex"),
	GDisplayClusterPreviewMultiGPURenderingMaxIndex,
	TEXT("Distribute mGPU render on GPU from #min to #max indices"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterCrossGPUTransferEnable = 0;
static FAutoConsoleVariableRef CDisplayClusterCrossGPUTransferEnable(
	TEXT("nDisplay.render.CrossGPUTransfer.Enable"),
	GDisplayClusterCrossGPUTransferEnable,
	TEXT("Enable cross-GPU transfers using nDisplay implementation (0 - disable, default) \n")
	TEXT("That replaces the default cross-GPU transfers using UE Core for the nDisplay viewports viewfamilies.\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterCrossGPUTransferLockSteps = 1;
static FAutoConsoleVariableRef CDisplayClusterCrossGPUTransferLockSteps(
	TEXT("nDisplay.render.CrossGPUTransfer.LockSteps"),
	GDisplayClusterCrossGPUTransferLockSteps,
	TEXT("The bLockSteps parameter is simply passed to the FTransferResourceParams structure. (0 - disable)\n")
	TEXT("Whether the GPUs must handshake before and after the transfer. Required if the texture rect is being written to in several render passes.\n")
	TEXT("Otherwise, minimal synchronization will be used.\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterCrossGPUTransferPullData = 1;
static FAutoConsoleVariableRef CVarDisplayClusterCrossGPUTransferPullData(
	TEXT("nDisplay.render.CrossGPUTransfer.PullData"),
	GDisplayClusterCrossGPUTransferPullData,
	TEXT("The bPullData parameter is simply passed to the FTransferResourceParams structure. (0 - disable)\n")
	TEXT("Whether the data is read by the dest GPU, or written by the src GPU (not allowed if the texture is a backbuffer)\n"),
	ECVF_RenderThreadSafe
);

// Choose method to preserve alpha channel
int32 GDisplayClusterAlphaChannelCaptureMode = (uint8)ECVarDisplayClusterAlphaChannelCaptureMode::FXAA;
static FAutoConsoleVariableRef CVarDisplayClusterAlphaChannelCaptureMode(
	TEXT("nDisplay.render.AlphaChannelCaptureMode"),
	GDisplayClusterAlphaChannelCaptureMode,
	TEXT("Alpha channel capture mode (FXAA - default)\n")
	TEXT("0 - Disabled\n")
	TEXT("1 - ThroughTonemapper\n")
	TEXT("2 - FXAA\n")
	TEXT("3 - Copy [experimental]\n")
	TEXT("4 - CopyAA [experimental]\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration
///////////////////////////////////////////////////////////////////
EDisplayClusterRenderFrameAlphaChannelCaptureMode FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::GetAlphaChannelCaptureMode()
{
	ECVarDisplayClusterAlphaChannelCaptureMode AlphaChannelCaptureMode = (ECVarDisplayClusterAlphaChannelCaptureMode)FMath::Clamp(GDisplayClusterAlphaChannelCaptureMode, 0, (int32)ECVarDisplayClusterAlphaChannelCaptureMode::COUNT - 1);

	static const auto CVarPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
	const EAlphaChannelMode::Type PropagateAlpha = EAlphaChannelMode::FromInt(CVarPropagateAlpha->GetValueOnGameThread());
	const bool bAllowThroughTonemapper = PropagateAlpha == EAlphaChannelMode::AllowThroughTonemapper;

	switch (AlphaChannelCaptureMode)
	{
	case ECVarDisplayClusterAlphaChannelCaptureMode::ThroughTonemapper:
		// Disable alpha capture if PropagateAlpha not valid
		return bAllowThroughTonemapper ? EDisplayClusterRenderFrameAlphaChannelCaptureMode::ThroughTonemapper : EDisplayClusterRenderFrameAlphaChannelCaptureMode::None;

	case ECVarDisplayClusterAlphaChannelCaptureMode::FXAA:
		return EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA;

	case ECVarDisplayClusterAlphaChannelCaptureMode::Copy:
		return EDisplayClusterRenderFrameAlphaChannelCaptureMode::Copy;

	case ECVarDisplayClusterAlphaChannelCaptureMode::CopyAA:
		return EDisplayClusterRenderFrameAlphaChannelCaptureMode::CopyAA;

	case ECVarDisplayClusterAlphaChannelCaptureMode::Disabled:
	default:
		break;
	}

	return EDisplayClusterRenderFrameAlphaChannelCaptureMode::None;
}

bool FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::UpdateRenderFrameConfiguration(FDisplayClusterViewportManager* ViewportManager, EDisplayClusterRenderFrameMode InRenderMode, FDisplayClusterViewportConfiguration& InOutConfiguration)
{
	if (InRenderMode == EDisplayClusterRenderFrameMode::Unknown)
	{
		// Do not initialize for unknown rendering type
		return false;
	}

	check(ViewportManager);

	ADisplayClusterRootActor* ConfigurationRootActor = InOutConfiguration.GetRootActor(EDisplayClusterRootActorType::Configuration);
	if (!(ConfigurationRootActor))
	{
		// If the ConfigurationRootActor is not defined, initialization cannot be performed.
		return false;
	}

	FDisplayClusterRenderFrameSettings NewRenderFrameSettings = InOutConfiguration.GetRenderFrameSettings();

	// Set current rendering mode
	NewRenderFrameSettings.RenderMode = InRenderMode;

	// Preview Settings : experimental mGPU feature
	NewRenderFrameSettings.PreviewMultiGPURendering.Reset();
	if (GDisplayClusterPreviewAllowMultiGPURendering)
	{
		const int32 MinGPUIndex = FMath::Max(0, GDisplayClusterPreviewMultiGPURenderingMinIndex);
		const int32 MaxGPUIndex = FMath::Max(MinGPUIndex, GDisplayClusterPreviewMultiGPURenderingMaxIndex);

		NewRenderFrameSettings.PreviewMultiGPURendering = FIntPoint(MinGPUIndex, MaxGPUIndex);
	}

	// Support alpha channel capture
	NewRenderFrameSettings.AlphaChannelCaptureMode = GetAlphaChannelCaptureMode();

	// Update RenderFrame configuration
	const FDisplayClusterConfigurationRenderFrame& InRenderFrameConfiguration = ConfigurationRootActor->GetRenderFrameSettings();
	{
		// Global RTT sizes mults
		NewRenderFrameSettings.ClusterRenderTargetRatioMult = InRenderFrameConfiguration.ClusterRenderTargetRatioMult;
		NewRenderFrameSettings.ClusterICVFXInnerViewportRenderTargetRatioMult = InRenderFrameConfiguration.ClusterICVFXInnerViewportRenderTargetRatioMult;
		NewRenderFrameSettings.ClusterICVFXOuterViewportRenderTargetRatioMult = InRenderFrameConfiguration.ClusterICVFXOuterViewportRenderTargetRatioMult;

		// Global Buffer ratio mults
		NewRenderFrameSettings.ClusterBufferRatioMult = InRenderFrameConfiguration.ClusterBufferRatioMult;
		NewRenderFrameSettings.ClusterICVFXInnerFrustumBufferRatioMult = InRenderFrameConfiguration.ClusterICVFXInnerFrustumBufferRatioMult;
		NewRenderFrameSettings.ClusterICVFXOuterViewportBufferRatioMult = InRenderFrameConfiguration.ClusterICVFXOuterViewportBufferRatioMult;

		// Allow warpblend render
		NewRenderFrameSettings.bAllowWarpBlend = InRenderFrameConfiguration.bAllowWarpBlend;

		// Performance: nDisplay has its own implementation of cross-GPU transfer.
		NewRenderFrameSettings.CrossGPUTransfer.bEnable = GDisplayClusterCrossGPUTransferEnable != 0;
		NewRenderFrameSettings.CrossGPUTransfer.bLockSteps = GDisplayClusterCrossGPUTransferLockSteps != 0;
		NewRenderFrameSettings.CrossGPUTransfer.bPullData = GDisplayClusterCrossGPUTransferPullData != 0;

		// Performance: Allow to use parent ViewFamily from parent viewport 
		// (icvfx has child viewports: lightcard and chromakey with prj_view matrices copied from parent viewport. May sense to use same viewfamily?)
		// [not implemented yet] Experimental
		NewRenderFrameSettings.bShouldUseParentViewportRenderFamily = InRenderFrameConfiguration.bShouldUseParentViewportRenderFamily;
	}

	if (NewRenderFrameSettings.IsPreviewRendering())
	{
		// Preview use its own rendering pipeline
		NewRenderFrameSettings.bUseDisplayClusterRenderDevice = false;
	}
	else
	{
		// Configuring the use of DC RenderDevice:
		static IDisplayCluster& DisplayClusterAPI = IDisplayCluster::Get();
		NewRenderFrameSettings.bUseDisplayClusterRenderDevice = (GEngine->StereoRenderingDevice.IsValid() && DisplayClusterAPI.GetOperationMode() == EDisplayClusterOperationMode::Cluster);
	}

	// Apply new settings:
	InOutConfiguration.SetRenderFrameSettings(NewRenderFrameSettings);

	return true;
}

void FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::PostUpdateRenderFrameConfiguration(FDisplayClusterViewportConfiguration& InOutConfiguration)
{
	if (FDisplayClusterViewportManager* ViewportManager = InOutConfiguration.GetViewportManagerImpl())
	{
		FDisplayClusterRenderFrameSettings NewRenderFrameSettings = InOutConfiguration.GetRenderFrameSettings();

		// Update global flags that control viewport resources
		NewRenderFrameSettings.bShouldUseOutputTargetableResources = ViewportManager->ShouldUseOutputTargetableResources();
		NewRenderFrameSettings.bShouldUseAdditionalFrameTargetableResource = ViewportManager->ShouldUseAdditionalFrameTargetableResource();
		NewRenderFrameSettings.bShouldUseFullSizeFrameTargetableResource = ViewportManager->ShouldUseFullSizeFrameTargetableResource();

		// Apply new settings:
		InOutConfiguration.SetRenderFrameSettings(NewRenderFrameSettings);
	}
}
