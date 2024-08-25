// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Visibility.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

namespace UE::DisplayCluster::Configuration::VisibilityHelpers
{
	static inline void ImplCollectActorComponents(AActor& InActor, TSet<FPrimitiveComponentId>& OutAdditionalComponentsList)
	{
		for (const UActorComponent* Component : InActor.GetComponents())
		{
			if (const UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
			{
				OutAdditionalComponentsList.Add(PrimComp->GetPrimitiveSceneId());
			}
		}
	}

	static inline void ImplCollectVisibility(FDisplayClusterViewportConfiguration& InConfiguration, const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList, TArray<FName>& OutActorLayerNames, TSet<FPrimitiveComponentId>& OutAdditionalComponentsList)
	{
		// Collect RootActor components
		if (InVisibilityList.RootActorComponentNames.Num() > 0)
		{
			if (ADisplayClusterRootActor* SceneRootActor = InConfiguration.GetRootActor(EDisplayClusterRootActorType::Scene))
			{
				SceneRootActor->FindPrimitivesByName(InVisibilityList.RootActorComponentNames, OutAdditionalComponentsList);
			}
		}

		auto CollectActorRefs = [&](UWorld* CurrentWorld, const TArray<TSoftObjectPtr<AActor>>& InActorRefs)
			{
				for (const TSoftObjectPtr<AActor>& ActorSOPtrIt : InActorRefs)
				{
					if (ActorSOPtrIt.IsValid())
					{
						if (ActorSOPtrIt->GetWorld() == CurrentWorld)
						{
							ImplCollectActorComponents(*ActorSOPtrIt.Get(), OutAdditionalComponentsList);
						}
						else if(CurrentWorld)
						{
							// re-reference to the current world
							// Not implemented.
							//!
						}
					}
				}
			};

		// Collect Actors refs
		UWorld* CurrentWorld = InConfiguration.GetCurrentWorld();
		CollectActorRefs(CurrentWorld, InVisibilityList.Actors);
		CollectActorRefs(CurrentWorld, InVisibilityList.AutoAddedActors);

		// Collect ActorLayers
		for (const FActorLayer& ActorLayerIt : InVisibilityList.ActorLayers)
		{
			if (!ActorLayerIt.Name.IsNone())
			{
				OutActorLayerNames.AddUnique(ActorLayerIt.Name);
			}
		}
	}
};
using namespace UE::DisplayCluster::Configuration;

void FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList)
{
	TSet<FPrimitiveComponentId> AdditionalComponentsList;
	TArray<FName> ActorLayers;
	VisibilityHelpers::ImplCollectVisibility(*DstViewport.Configuration, InVisibilityList, ActorLayers, AdditionalComponentsList);

	DstViewport.GetVisibilitySettingsImpl().UpdateVisibilitySettings(EDisplayClusterViewport_VisibilityMode::ShowOnly, ActorLayers, AdditionalComponentsList);
}

void FDisplayClusterViewportConfigurationHelpers_Visibility::AppendHideList_ICVFX(FDisplayClusterViewport& DstViewport, const FDisplayClusterConfigurationICVFX_VisibilityList& InHideList)
{
	TSet<FPrimitiveComponentId> AdditionalComponentsList;
	TArray<FName> ActorLayers;
	VisibilityHelpers::ImplCollectVisibility(*DstViewport.Configuration, InHideList, ActorLayers, AdditionalComponentsList);

	DstViewport.GetVisibilitySettingsImpl().AppendHideList(ActorLayers, AdditionalComponentsList);
}

void FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateHideList_ICVFX(FDisplayClusterViewportConfiguration& InConfiguration, TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& DstViewports)
{
	ADisplayClusterRootActor* ConfigurationRootActor = InConfiguration.GetRootActor(EDisplayClusterRootActorType::Configuration);
	const FDisplayClusterConfigurationICVFX_StageSettings* StageSettings = InConfiguration.GetStageSettings();

	if (DstViewports.Num() > 0 && ConfigurationRootActor && StageSettings)
	{
		TArray<FName> ActorLayerNames;
		TSet<FPrimitiveComponentId> AdditionalComponentsList;

		VisibilityHelpers::ImplCollectVisibility(InConfiguration, StageSettings->HideList, ActorLayerNames, AdditionalComponentsList);

		// Hide lightcard
		VisibilityHelpers::ImplCollectVisibility(InConfiguration, StageSettings->Lightcard.ShowOnlyList, ActorLayerNames, AdditionalComponentsList);

		// Also hide chromakeys for all cameras
		TArray<UDisplayClusterICVFXCameraComponent*> ConfigurationRootActorCameras;
		ConfigurationRootActor->GetComponents(ConfigurationRootActorCameras);

		for (const UDisplayClusterICVFXCameraComponent* ConfigurationCameraIt : ConfigurationRootActorCameras)
		{
			if (ConfigurationCameraIt)
			{
				if (const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* ChromakeyRenderSettings = ConfigurationCameraIt->GetCameraSettingsICVFX().Chromakey.GetChromakeyRenderSettings(*StageSettings))
				{
					VisibilityHelpers::ImplCollectVisibility(InConfiguration, ChromakeyRenderSettings->ShowOnlyList, ActorLayerNames, AdditionalComponentsList);
				}
			}
		}

		TArray<FName> OuterActorLayerNames;
		TSet<FPrimitiveComponentId> OuterAdditionalComponentsList;
		VisibilityHelpers::ImplCollectVisibility(InConfiguration, StageSettings->OuterViewportHideList, OuterActorLayerNames, OuterAdditionalComponentsList);

		// Update hide list for all desired viewports:
		for (TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : DstViewports)
		{
			if (ViewportIt.IsValid())
			{
				ViewportIt->GetVisibilitySettingsImpl().UpdateVisibilitySettings(EDisplayClusterViewport_VisibilityMode::Hide, ActorLayerNames, AdditionalComponentsList);

				// Support additional hide list for outer viewports
				if (EnumHasAllFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target))
				{
					ViewportIt->GetVisibilitySettingsImpl().AppendHideList(OuterActorLayerNames, OuterAdditionalComponentsList);
				}
			}
		}
	}
}
