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

int32 GDisplayClusterOverrideMultiGPUMode = -1;
static FAutoConsoleVariableRef CVarDisplayClusterOverrideMultiGPUMode(
	TEXT("DC.OverrideMultiGPUMode"),
	GDisplayClusterOverrideMultiGPUMode,
	TEXT("Override Multi GPU Mode setting from component (-1 == no override, or EDisplayClusterConfigurationRenderMGPUMode enum)"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration
///////////////////////////////////////////////////////////////////

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
		if (ConfigurationData)
		{
			FDisplayClusterViewportConfigurationBase ConfigurationBase(ViewportManager, *RootActor, *ConfigurationData);
			FDisplayClusterViewportConfigurationICVFX ConfigurationICVFX(*RootActor);
			FDisplayClusterViewportConfigurationProjectionPolicy ConfigurationProjectionPolicy(ViewportManager, *RootActor, *ConfigurationData);

			ImplUpdateRenderFrameConfiguration(RootActor->GetRenderFrameSettings());

			// Set current rendering mode
			RenderFrameSettings.RenderMode = InRenderMode;
			RenderFrameSettings.ClusterNodeId = InClusterNodeId;

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

void FDisplayClusterViewportConfiguration::ImplUpdateConfigurationVisibility(ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationData& ConfigurationData)
{
	// Hide root actor components for all viewports
	TSet<FPrimitiveComponentId> RootActorHidePrimitivesList;
	if (RootActor.GetHiddenInGamePrimitives(RootActorHidePrimitivesList))
	{
		for (FDisplayClusterViewport* ViewportIt : ViewportManager.ImplGetViewports())
		{
			ViewportIt->VisibilitySettings.SetRootActorHideList(RootActorHidePrimitivesList);
		}
	}
}

void FDisplayClusterViewportConfiguration::ImplPostUpdateRenderFrameConfiguration()
{
	// Some frame postprocess require additional render targetable resources
	RenderFrameSettings.bShouldUseAdditionalFrameTargetableResource = ViewportManager.ShouldUseAdditionalFrameTargetableResource();
	RenderFrameSettings.bShouldUseFullSizeFrameTargetableResource = ViewportManager.ShouldUseFullSizeFrameTargetableResource();
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

	// Performance: Allow change global MGPU settings
	int32 ModeOverride = FMath::Min(GDisplayClusterOverrideMultiGPUMode,
		(int32)EDisplayClusterConfigurationRenderMGPUMode::Optimized_DisabledLockSteps);

	switch (ModeOverride >= 0 ? (EDisplayClusterConfigurationRenderMGPUMode)ModeOverride : InRenderFrameConfiguration.MultiGPUMode)
	{
	case EDisplayClusterConfigurationRenderMGPUMode::None:
		RenderFrameSettings.MultiGPUMode = EDisplayClusterMultiGPUMode::None;
		break;
	case EDisplayClusterConfigurationRenderMGPUMode::Optimized_DisabledLockSteps:
		RenderFrameSettings.MultiGPUMode = EDisplayClusterMultiGPUMode::Optimized_DisabledLockSteps;
		break;
	case EDisplayClusterConfigurationRenderMGPUMode::Optimized_EnabledLockSteps:
		RenderFrameSettings.MultiGPUMode = EDisplayClusterMultiGPUMode::Optimized_EnabledLockSteps;
		break;
	case EDisplayClusterConfigurationRenderMGPUMode::Enabled:
	default:
		RenderFrameSettings.MultiGPUMode = EDisplayClusterMultiGPUMode::Enabled;
		break;
	};

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
