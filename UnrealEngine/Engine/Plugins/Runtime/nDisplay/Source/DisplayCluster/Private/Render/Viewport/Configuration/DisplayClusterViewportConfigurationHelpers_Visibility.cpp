// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationHelpers_Visibility.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

static void ImplCollectVisibility(ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList, TArray<FName>& OutActorLayerNames, TSet<FPrimitiveComponentId>& OutAdditionalComponentsList)
{
	// Collect RootActor components
	if (InVisibilityList.RootActorComponentNames.Num() > 0)
	{
		RootActor.FindPrimitivesByName(InVisibilityList.RootActorComponentNames, OutAdditionalComponentsList);
	}

	auto CollectActorRefs = [&](const TArray<TSoftObjectPtr<AActor>>& InActorRefs)
 
	{
		for (const TSoftObjectPtr<AActor>& ActorSOPtrIt : InActorRefs)
		{
			if (ActorSOPtrIt.IsValid())
			{
				for (const UActorComponent* Component : ActorSOPtrIt->GetComponents())
				{
					if (const UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
					{
						OutAdditionalComponentsList.Add(PrimComp->ComponentId);
					}
				}
			}
		}
	};
	
	// Collect Actors refs
	CollectActorRefs(InVisibilityList.Actors);
	CollectActorRefs(InVisibilityList.AutoAddedActors);

	// Collect ActorLayers
	for (const FActorLayer& ActorLayerIt : InVisibilityList.ActorLayers)
	{
		if (!ActorLayerIt.Name.IsNone())
		{
			OutActorLayerNames.AddUnique(ActorLayerIt.Name);
		}
	}
}

void FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateShowOnlyList(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& RootActor, const FDisplayClusterConfigurationICVFX_VisibilityList& InVisibilityList)
{
	TSet<FPrimitiveComponentId> AdditionalComponentsList;
	TArray<FName> ActorLayers;
	ImplCollectVisibility(RootActor, InVisibilityList, ActorLayers, AdditionalComponentsList);

	DstViewport.VisibilitySettings.UpdateConfiguration(EDisplayClusterViewport_VisibilityMode::ShowOnly, ActorLayers, AdditionalComponentsList);
}

void FDisplayClusterViewportConfigurationHelpers_Visibility::AppendHideList_ICVFX(FDisplayClusterViewport& DstViewport, ADisplayClusterRootActor& InRootActor, const FDisplayClusterConfigurationICVFX_VisibilityList& InHideList)
{
	TSet<FPrimitiveComponentId> AdditionalComponentsList;
	TArray<FName> ActorLayers;
	ImplCollectVisibility(InRootActor, InHideList, ActorLayers, AdditionalComponentsList);

	DstViewport.VisibilitySettings.AppendHideList(ActorLayers, AdditionalComponentsList);
}

void FDisplayClusterViewportConfigurationHelpers_Visibility::UpdateHideList_ICVFX(TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& DstViewports, ADisplayClusterRootActor& InRootActor)
{
	if (DstViewports.Num() > 0)
	{
		TArray<FName> ActorLayerNames;
		TSet<FPrimitiveComponentId> AdditionalComponentsList;

		const FDisplayClusterConfigurationICVFX_StageSettings& StageSettings = InRootActor.GetStageSettings();

		ImplCollectVisibility(InRootActor, StageSettings.HideList, ActorLayerNames, AdditionalComponentsList);

		// Hide lightcard
		ImplCollectVisibility(InRootActor, StageSettings.Lightcard.ShowOnlyList, ActorLayerNames, AdditionalComponentsList);

		// Also hide chromakeys for all cameras
		TArray<UDisplayClusterICVFXCameraComponent*> ICVFXCamerasComps;
		InRootActor.GetComponents(ICVFXCamerasComps);

		for (const UDisplayClusterICVFXCameraComponent* ICVFXCameraIt : ICVFXCamerasComps)
		{
			if (ICVFXCameraIt)
			{
				if (const FDisplayClusterConfigurationICVFX_ChromakeyRenderSettings* ChromakeyRenderSettings = ICVFXCameraIt->GetCameraSettingsICVFX().Chromakey.GetChromakeyRenderSettings(StageSettings))
				{
					ImplCollectVisibility(InRootActor, ChromakeyRenderSettings->ShowOnlyList, ActorLayerNames, AdditionalComponentsList);
				}
			}
		}

		TArray<FName> OuterActorLayerNames;
		TSet<FPrimitiveComponentId> OuterAdditionalComponentsList;
		ImplCollectVisibility(InRootActor, StageSettings.OuterViewportHideList, OuterActorLayerNames, OuterAdditionalComponentsList);

		// Update hide list for all desired viewports:
		for (TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : DstViewports)
		{
			if (ViewportIt.IsValid())
			{
				ViewportIt->VisibilitySettings.UpdateConfiguration(EDisplayClusterViewport_VisibilityMode::Hide, ActorLayerNames, AdditionalComponentsList);

				// Support additional hide list for outer viewports
				if (EnumHasAllFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target))
				{
					ViewportIt->VisibilitySettings.AppendHideList(OuterActorLayerNames, OuterAdditionalComponentsList);
				}
			}
		}
	}
}
