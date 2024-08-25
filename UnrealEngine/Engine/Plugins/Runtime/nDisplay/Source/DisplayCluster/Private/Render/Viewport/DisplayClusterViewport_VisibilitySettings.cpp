// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport_VisibilitySettings.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"

#include "EngineUtils.h"
#include "SceneView.h"

static void GetPrimitiveComponentsFromLayers(UWorld* World, const TArray<FName>& SourceLayers, TSet<FPrimitiveComponentId>& OutPrimitives,
                                             const TSet<FPrimitiveComponentId>* InExcludePrimitivesList = nullptr)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DCRootActor_GetPrimitiveComponentsFromLayers);

	if (SourceLayers.Num())
	{
		// Iterate over all actors, looking for actors in the specified layers.
		for (const TWeakObjectPtr<AActor> WeakActor : FActorRange(World))
		{
			if (const AActor* Actor = WeakActor.Get())
			{
				bool bActorFoundOnSourceLayers = false;

				// Search actor on source layers
				for (const FName& LayerIt : SourceLayers)
				{
					if (Actor && Actor->Layers.Contains(LayerIt))
					{
						bActorFoundOnSourceLayers = true;
						break;
					}
				}

				if (bActorFoundOnSourceLayers)
				{
					// Save all actor components to OutPrimitives
					for (UActorComponent* Component : Actor->GetComponents())
					{
						if (const UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
						{
							if (!InExcludePrimitivesList || !InExcludePrimitivesList->Contains(PrimComp->GetPrimitiveSceneId()))
							{
								OutPrimitives.Add(PrimComp->GetPrimitiveSceneId());
							}
						}
					}
				}
			}
		}
	}
}

void FDisplayClusterViewport_VisibilitySettings::SetupSceneView(class UWorld* World, FSceneView& InOutView) const
{
	check(World);
	
	switch (LayersMode)
	{
	case EDisplayClusterViewport_VisibilityMode::ShowOnly:
	{
		if (ActorLayers.Num() > 0 || AdditionalComponentsList.Num() > 0)
		{
			InOutView.ShowOnlyPrimitives.Emplace();
			GetPrimitiveComponentsFromLayers(World, ActorLayers,InOutView.ShowOnlyPrimitives.GetValue(),
				&RootActorHidePrimitivesList);

			for (const FPrimitiveComponentId& AdditionalPrimitiveId : AdditionalComponentsList)
			{
				if (!RootActorHidePrimitivesList.Contains(AdditionalPrimitiveId))
				{
					InOutView.ShowOnlyPrimitives.GetValue().Add(AdditionalPrimitiveId);
				}
			}
			return;
		}
		break;
	}
	case EDisplayClusterViewport_VisibilityMode::Hide:
	{
		GetPrimitiveComponentsFromLayers(World, ActorLayers, InOutView.HiddenPrimitives);
		InOutView.HiddenPrimitives.Append(AdditionalComponentsList);
		break;
	}
	default:
		break;
	}

	// Also hide components from root actor 
	InOutView.HiddenPrimitives.Append(RootActorHidePrimitivesList);
}
