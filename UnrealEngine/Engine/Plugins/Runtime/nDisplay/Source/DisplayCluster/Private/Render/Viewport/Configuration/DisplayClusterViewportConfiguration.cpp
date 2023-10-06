// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfiguration.h"

#include "DisplayClusterConfigurationStrings.h"
#include "DisplayClusterViewportConfigurationBase.h"
#include "DisplayClusterViewportConfigurationICVFX.h"
#include "DisplayClusterViewportConfigurationProjectionPolicy.h"

#include "DisplayClusterRootActor.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

#include "Render/IPDisplayClusterRenderManager.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterLog.h"

#include "Engine/RendererSettings.h"


///////////////////////////////////////////////////////////////////
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

/**
* This enum is for CVar values only and is used to process the logic that converts the values to the runtime enum in GetAlphaChannelCaptureMode().
*/
enum class ECVarDisplayClusterAlphaChannelCaptureMode : uint8
{
	/** [Disabled]
	 * Disable alpha channel saving.
	 */
	Disabled,

	/** [ThroughTonemapper]
	 * When rendering with the PropagateAlpha experimental mode turned on, the alpha channel is forwarded to post-processes.
	 * In this case, the alpha channel is anti-aliased along with the color.
	 * Since some post-processing may change the alpha, it is copied at the beginning of the PP and restored after all post-processing is completed.
	 */
	ThroughTonemapper,

	/** [FXAA]
	 * Otherwise, if the PropagateAlpha mode is disabled in the project settings, we need to save the alpha before it becomes invalid.
	 * The alpha is valid until the scene color is resolved (on the ResolvedSceneColor callback). Therefore, it is copied to a temporary resource on this cb.
	 * Since we need to remove AA jittering (because alpha copied before AA), anti-aliasing is turned off for this viewport.
	 * And finally the FXAA is used for smoothing.
	 */
	FXAA,

	/** [Copy]
	 * Disable AA and TAA, Copy alpha from scenecolor texture to final.
	 * These experimental (temporary) modes for the performance tests.
	 */
	 Copy,

	/** [CopyAA]
	 * Use AA, disable TAA, Copy alpha from scenecolor texture to final.
	 * These experimental (temporary) modes for the performance tests.
	 */
	 CopyAA,

	COUNT
};

/**
 *Choose method to preserve alpha channel
 */
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
FDisplayClusterViewportConfiguration::FDisplayClusterViewportConfiguration(FDisplayClusterViewportManager& InViewportManager)
	: ViewportManagerWeakPtr(InViewportManager.AsShared())
{ }

FDisplayClusterViewportConfiguration::~FDisplayClusterViewportConfiguration()
{ }

EDisplayClusterRenderFrameAlphaChannelCaptureMode FDisplayClusterViewportConfiguration::GetAlphaChannelCaptureMode() const
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

bool FDisplayClusterViewportConfiguration::SetRootActor(ADisplayClusterRootActor* InRootActorPtr)
{
	check(IsInGameThread());
	check(InRootActorPtr);

	if (!RootActorRef.IsDefinedSceneActor() || GetRootActor() != InRootActorPtr)
	{
		// Update root actor reference:
		RootActorRef.ResetSceneActor();
		RootActorRef.SetSceneActor(InRootActorPtr);

		// return true, if changed
		return true;
	}

	return false;
}

ADisplayClusterRootActor* FDisplayClusterViewportConfiguration::GetRootActor() const
{
	check(IsInGameThread());

	AActor* ActorPtr = RootActorRef.GetOrFindSceneActor();
	if (ActorPtr)
	{
		return static_cast<ADisplayClusterRootActor*>(ActorPtr);
	}

	return nullptr;
}

bool FDisplayClusterViewportConfiguration::ImplUpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, const FDisplayClusterPreviewSettings* InPreviewSettings, const TArray<FString>* InViewportNames)
{
	check(IsInGameThread());

	ADisplayClusterRootActor* RootActor = GetRootActor();
	if (RootActor)
	{
		const UDisplayClusterConfigurationData* ConfigurationData = RootActor->GetConfigData();
		FDisplayClusterViewportManager* ViewportManager = GetViewportManager();
		if (ConfigurationData && ViewportManager)
		{
			FDisplayClusterViewportConfigurationBase ConfigurationBase(*ViewportManager, *RootActor, *ConfigurationData);
			FDisplayClusterViewportConfigurationICVFX ConfigurationICVFX(*RootActor);
			FDisplayClusterViewportConfigurationProjectionPolicy ConfigurationProjectionPolicy(*ViewportManager, *RootActor, *ConfigurationData);

			ImplUpdateRenderFrameConfiguration(RootActor->GetRenderFrameSettings());

			// Set current rendering mode
			RenderFrameSettings.RenderMode = InRenderMode;
			RenderFrameSettings.ClusterNodeId = InClusterNodeId;

			// Support alpha channel capture
			RenderFrameSettings.AlphaChannelCaptureMode = GetAlphaChannelCaptureMode();

			if (InPreviewSettings != nullptr)
			{
				// Downscale resources with PreviewDownscaleRatio
				RenderFrameSettings.PreviewRenderTargetRatioMult = InPreviewSettings->PreviewRenderTargetRatioMult;

				// Limit preview textures max size
				RenderFrameSettings.PreviewMaxTextureDimension = InPreviewSettings->PreviewMaxTextureDimension;

				// Hack preview gamma.
				// In a scene, PostProcess always renders on top of the preview textures.
				// But in it, PostProcess is also rendered with the flag turned off.
				RenderFrameSettings.bPreviewEnablePostProcess = InPreviewSettings->bPreviewEnablePostProcess;

				// Support mGPU for preview rendering
				RenderFrameSettings.bAllowMultiGPURenderingInEditor = InPreviewSettings->bAllowMultiGPURenderingInEditor;
				RenderFrameSettings.PreviewMinGPUIndex = InPreviewSettings->MinGPUIndex;
				RenderFrameSettings.PreviewMaxGPUIndex = InPreviewSettings->MaxGPUIndex;

				RenderFrameSettings.bIsRenderingInEditor = true;
				RenderFrameSettings.bIsPreviewRendering = true;
				RenderFrameSettings.bFreezePreviewRender = InPreviewSettings->bFreezePreviewRender;
				
				if (InPreviewSettings->bIsPIE)
				{
					// Allow TextureShare+nDisplay from PIE
					RenderFrameSettings.bIsPreviewRendering = false;
				}
			}
			else
			{
				RenderFrameSettings.bPreviewEnablePostProcess = false;
				RenderFrameSettings.bAllowMultiGPURenderingInEditor = false;
				RenderFrameSettings.bIsRenderingInEditor = false;
				RenderFrameSettings.bIsPreviewRendering = false;
			}

			if (InViewportNames)
			{
				ConfigurationBase.Update(*InViewportNames, RenderFrameSettings);
			}
			else
			{
				ConfigurationBase.Update(InClusterNodeId);
			}

			ConfigurationICVFX.Update();
			ConfigurationProjectionPolicy.Update();
			ConfigurationICVFX.PostUpdate();

#if WITH_EDITOR
			if (RenderFrameSettings.bIsPreviewRendering)
			{
				ConfigurationICVFX.PostUpdatePreview_Editor(*InPreviewSettings);
			}
#endif

			ImplUpdateConfigurationVisibility(*RootActor, *ConfigurationData);

			if (!InClusterNodeId.IsEmpty())
			{
				// support postprocess only for per-node render
				ConfigurationBase.UpdateClusterNodePostProcess(InClusterNodeId, RenderFrameSettings);
			}

			ImplPostUpdateRenderFrameConfiguration();

			return true;
		}
	}

	return false;
}

bool FDisplayClusterViewportConfiguration::UpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId)
{
	return ImplUpdateConfiguration(InRenderMode, InClusterNodeId, nullptr, nullptr);
}

bool FDisplayClusterViewportConfiguration::UpdateCustomConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames)
{
	return ImplUpdateConfiguration(InRenderMode, TEXT(""), nullptr, &InViewportNames);
}

#if WITH_EDITOR
bool FDisplayClusterViewportConfiguration::UpdatePreviewConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, const FDisplayClusterPreviewSettings& InPreviewSettings)
{
	if (InClusterNodeId.Equals(DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll, ESearchCase::IgnoreCase))
	{
		check(!InPreviewSettings.bIsPIE);

		// initialize all nodes
		ADisplayClusterRootActor* RootActor = GetRootActor();
		if (RootActor != nullptr)
		{
			const UDisplayClusterConfigurationData* ConfigurationData = RootActor->GetConfigData();
			if (ConfigurationData != nullptr && ConfigurationData->Cluster != nullptr)
			{
				TArray<FString> ClusterNodesIDs;
				ConfigurationData->Cluster->GetNodeIds(ClusterNodesIDs);
				for (const FString& ClusterNodeIdIt : ClusterNodesIDs)
				{
					ImplUpdateConfiguration(InRenderMode, ClusterNodeIdIt, &InPreviewSettings, nullptr);
				}

				// all cluster nodes viewports updated
				return true;
			}
		}

		return false;
	}

	return ImplUpdateConfiguration(InRenderMode, InClusterNodeId, &InPreviewSettings, nullptr);
}
#endif

void FDisplayClusterViewportConfiguration::ImplUpdateConfigurationVisibility(ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationData& ConfigurationData) const
{
	// Hide root actor components for all viewports
	FDisplayClusterViewportManager* ViewportManager = GetViewportManager();

	TSet<FPrimitiveComponentId> RootActorHidePrimitivesList;
	if (ViewportManager && RootActor.GetHiddenInGamePrimitives(RootActorHidePrimitivesList))
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid())
			{
				ViewportIt->VisibilitySettings.SetRootActorHideList(RootActorHidePrimitivesList);
			}
		}
	}
}

void FDisplayClusterViewportConfiguration::ImplPostUpdateRenderFrameConfiguration()
{
	if (FDisplayClusterViewportManager* ViewportManager = GetViewportManager())
	{
		// Some frame postprocess require additional render targetable resources
		RenderFrameSettings.bShouldUseAdditionalFrameTargetableResource = ViewportManager->ShouldUseAdditionalFrameTargetableResource();
		RenderFrameSettings.bShouldUseFullSizeFrameTargetableResource = ViewportManager->ShouldUseFullSizeFrameTargetableResource();
	}
}

void FDisplayClusterViewportConfiguration::ImplUpdateRenderFrameConfiguration(const FDisplayClusterConfigurationRenderFrame& InRenderFrameConfiguration)
{
	// Global RTT sizes mults
	RenderFrameSettings.ClusterRenderTargetRatioMult = InRenderFrameConfiguration.ClusterRenderTargetRatioMult;
	RenderFrameSettings.ClusterICVFXInnerViewportRenderTargetRatioMult = InRenderFrameConfiguration.ClusterICVFXInnerViewportRenderTargetRatioMult;
	RenderFrameSettings.ClusterICVFXOuterViewportRenderTargetRatioMult = InRenderFrameConfiguration.ClusterICVFXOuterViewportRenderTargetRatioMult;

	// Global Buffer ratio mults
	RenderFrameSettings.ClusterBufferRatioMult = InRenderFrameConfiguration.ClusterBufferRatioMult;
	RenderFrameSettings.ClusterICVFXInnerFrustumBufferRatioMult = InRenderFrameConfiguration.ClusterICVFXInnerFrustumBufferRatioMult;
	RenderFrameSettings.ClusterICVFXOuterViewportBufferRatioMult = InRenderFrameConfiguration.ClusterICVFXOuterViewportBufferRatioMult;

	// Allow warpblend render
	RenderFrameSettings.bAllowWarpBlend = InRenderFrameConfiguration.bAllowWarpBlend;

	// Performance: Allow merge multiple viewports on single RTT with atlasing (required for bAllowViewFamilyMergeOptimization)
	RenderFrameSettings.bAllowRenderTargetAtlasing = InRenderFrameConfiguration.bAllowRenderTargetAtlasing;

	// Performance: Allow viewfamily merge optimization (render multiple viewports contexts within single family)
	// [not implemented yet] Experimental
	switch (InRenderFrameConfiguration.ViewFamilyMode)
	{
	case EDisplayClusterConfigurationRenderFamilyMode::AllowMergeForGroups:
		RenderFrameSettings.ViewFamilyMode = EDisplayClusterRenderFamilyMode::AllowMergeForGroups;
		break;
	case EDisplayClusterConfigurationRenderFamilyMode::AllowMergeForGroupsAndStereo:
		RenderFrameSettings.ViewFamilyMode = EDisplayClusterRenderFamilyMode::AllowMergeForGroupsAndStereo;
		break;
	case EDisplayClusterConfigurationRenderFamilyMode::MergeAnyPossible:
		RenderFrameSettings.ViewFamilyMode = EDisplayClusterRenderFamilyMode::MergeAnyPossible;
		break;
	case EDisplayClusterConfigurationRenderFamilyMode::None:
	default:
		RenderFrameSettings.ViewFamilyMode = EDisplayClusterRenderFamilyMode::None;
		break;
	}

	// Performance: nDisplay has its own implementation of cross-GPU transfer.
	RenderFrameSettings.CrossGPUTransfer.bEnable    = GDisplayClusterCrossGPUTransferEnable != 0;
	RenderFrameSettings.CrossGPUTransfer.bLockSteps = GDisplayClusterCrossGPUTransferLockSteps != 0;
	RenderFrameSettings.CrossGPUTransfer.bPullData  = GDisplayClusterCrossGPUTransferPullData != 0;

	// Performance: Allow to use parent ViewFamily from parent viewport 
	// (icvfx has child viewports: lightcard and chromakey with prj_view matrices copied from parent viewport. May sense to use same viewfamily?)
	// [not implemented yet] Experimental
	RenderFrameSettings.bShouldUseParentViewportRenderFamily = InRenderFrameConfiguration.bShouldUseParentViewportRenderFamily;

	RenderFrameSettings.bIsRenderingInEditor = false;

#if WITH_EDITOR
	// Use special renderin mode, if DCRenderDevice not used right now
	const bool bIsNDisplayClusterMode = (GEngine->StereoRenderingDevice.IsValid() && GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);
	if (bIsNDisplayClusterMode == false)
	{
		RenderFrameSettings.bIsRenderingInEditor = true;
	}
#endif /*WITH_EDITOR*/
}
