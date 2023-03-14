// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport_VisibilitySettings.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"

#include "EngineUtils.h"
#include "SceneView.h"

static void GetPrimitiveComponentsFromLayers(UWorld* World, const TArray<FName>& SourceLayers, TSet<FPrimitiveComponentId>& OutPrimitives)
{
	if (SourceLayers.Num())
	{
		// Iterate over all actors, looking for actors in the specified layers.
		for (const TWeakObjectPtr<AActor> WeakActor : FActorRange(World))
		{
			AActor* Actor = WeakActor.Get();
			if (Actor)
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
						if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
						{
							OutPrimitives.Add(PrimComp->ComponentId);
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
			GetPrimitiveComponentsFromLayers(World, ActorLayers, InOutView.ShowOnlyPrimitives.GetValue());
			InOutView.ShowOnlyPrimitives.GetValue().Append(AdditionalComponentsList);
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
