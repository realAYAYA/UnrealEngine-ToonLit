// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MoviePipelineRenderLayerSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "MovieRenderPipelineCoreModule.h"
#include "UObject/Package.h"

void UMoviePipelineMaterialModifier::ApplyModifier(const UWorld* World)
{
	UMaterialInterface* NewMaterial = MaterialToApply.LoadSynchronous();
	if (!NewMaterial)
	{
		return;
	}

	ModifiedComponents.Empty();
	
	for (const UMoviePipelineCollection* Collection : Collections)
	{
		if (!Collection)
		{
			continue;
		}

		for (const AActor* Actor : Collection->GetMatchingActors(World, bUseInvertedActors))
		{
			const bool bIncludeFromChildActors = true;
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents, bIncludeFromChildActors);

			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				TArray<FMaterialSlotAssignment>& ModifiedMaterials = ModifiedComponents.FindOrAdd(PrimitiveComponent);
				
				for (int32 Index = 0; Index < PrimitiveComponent->GetNumMaterials(); ++Index)
				{
					ModifiedMaterials.Add(FMaterialSlotAssignment(Index, PrimitiveComponent->GetMaterial(Index)));
				
					PrimitiveComponent->SetMaterial(Index, NewMaterial);
				}
			}
		}
	}
}

void UMoviePipelineMaterialModifier::UndoModifier()
{
	for (const FComponentToMaterialMap::ElementType& ModifiedComponent : ModifiedComponents) 
	{
		UPrimitiveComponent* MeshComponent = ModifiedComponent.Key.LoadSynchronous();
		const TArray<FMaterialSlotAssignment>& OldMaterials = ModifiedComponent.Value;

		if (!MeshComponent)
		{
			continue;
		}

		for (const FMaterialSlotAssignment& MaterialPair : OldMaterials)
		{
			UMaterialInterface* MaterialInterface = MaterialPair.Value.LoadSynchronous();
			if (!MaterialInterface)
			{
				continue;
			}

			const int32 ElementIndex = MaterialPair.Key;
			MeshComponent->SetMaterial(ElementIndex, MaterialInterface);
		}
	}

	ModifiedComponents.Empty();
}

void UMoviePipelineVisibilityModifier::ApplyModifier(const UWorld* World)
{
	ModifiedActors.Empty();
	
	for (const UMoviePipelineCollection* Collection : Collections)
	{
		if (!Collection)
		{
			continue;
		}

		for (AActor* Actor : Collection->GetMatchingActors(World, bUseInvertedActors))
		{
			ModifiedActors.Add(Actor, Actor->IsHidden());
			SetActorHiddenState(Actor, bIsHidden);
		}
	}
}

void UMoviePipelineVisibilityModifier::UndoModifier()
{
	for (const TTuple<TSoftObjectPtr<AActor>, bool>& Pair : ModifiedActors)
	{
		SetActorHiddenState(Pair.Key.LoadSynchronous(), Pair.Value);
	}

	ModifiedActors.Empty();
}

void UMoviePipelineVisibilityModifier::SetActorHiddenState(AActor* Actor, const bool bInIsHidden) const
{
	Actor->SetActorHiddenInGame(bInIsHidden);

#if WITH_EDITOR
	Actor->SetIsTemporarilyHiddenInEditor(bInIsHidden);
#endif
}

// TODO: This really should be "DoesComponentMatchQuery()"
bool UMoviePipelineCollectionCommonQuery::DoesActorMatchQuery(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// TODO: This method should short-circuit so all query types aren't executed for every actor when the query mode is OR

	bool bMatchesActorNames = false;
	if (!ActorNames.IsEmpty())
	{
		bMatchesActorNames = ActorNames.Contains(Actor->GetName());
	}
	
	bool bMatchesComponentTypes = false;
	if (!ComponentTypes.IsEmpty())
	{
		const bool bIncludeFromChildActors = true;
		TArray<UActorComponent*> ActorComponents;
		Actor->GetComponents(ActorComponents, bIncludeFromChildActors);
		for (const UActorComponent* Component : ActorComponents)
		{
			for (const UClass* ComponentType : ComponentTypes)
			{
				if (Component->IsA(ComponentType))
				{
					bMatchesComponentTypes = true;
					break;
				}
			}
		}
	}

	bool bMatchesTags = false;
	if (!Tags.IsEmpty())
	{
		for (const FName& Tag : Tags)
		{
			if (Actor->Tags.Contains(Tag))
			{
				bMatchesTags = true;
				break;
			}
		}
	}

	const bool bUsingActorNames = !ActorNames.IsEmpty();
	const bool bUsingComponentTypes = !ComponentTypes.IsEmpty();
	const bool bUsingTags = !Tags.IsEmpty();

	if (QueryMode == EMoviePipelineCollectionCommonQueryMode::And)
	{
		return (!bUsingActorNames || bMatchesActorNames) &&
			   (!bUsingTags || bMatchesTags) &&
			   (!bUsingComponentTypes || bMatchesComponentTypes);
	}
	
	return bMatchesActorNames || bMatchesTags || bMatchesComponentTypes;
}

TArray<AActor*> UMoviePipelineCollection::GetMatchingActors(const UWorld* World, const bool bInvertResult) const
{
	TArray<AActor*> MatchingActors;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!Actor)
		{
			continue;
		}

		// If there aren't any queries, and the result should be inverted, just include the actor
		if (bInvertResult && Queries.IsEmpty())
		{
			MatchingActors.Add(Actor);
			continue;
		}

		for (const UMoviePipelineCollectionQuery* Query : Queries)
		{
			const bool bActorMatchesQuery = Query->DoesActorMatchQuery(Actor);
			
			if (bActorMatchesQuery && !bInvertResult)
			{
				MatchingActors.Add(Actor);
				break;
			}

			if (!bActorMatchesQuery && bInvertResult)
			{
				MatchingActors.Add(Actor);
				break;
			}
		}
	}
	
	return MatchingActors;
}

void UMoviePipelineCollectionModifier::AddCollection(UMoviePipelineCollection* Collection)
{
	// Don't allow adding a duplicate collection
	for (const UMoviePipelineCollection* ExistingCollection : Collections)
	{
		if (Collection && ExistingCollection && Collection->GetCollectionName().Equals(ExistingCollection->GetCollectionName()))
		{
			return;
		}
	}
	
	Collections.Add(Collection);
}

void UMoviePipelineCollection::AddQuery(UMoviePipelineCollectionQuery* Query)
{
	if (!Queries.Contains(Query))
	{
		Queries.Add(Query);
	}
}

UMoviePipelineCollection* UMoviePipelineRenderLayer::GetCollectionByName(const FString& Name) const
{
	for (const UMoviePipelineCollectionModifier* Modifier : Modifiers)
	{
		if (!Modifier)
		{
			continue;
		}
		
		for (UMoviePipelineCollection* Collection : Modifier->GetCollections())
		{
			if (Collection && Collection->GetCollectionName().Equals(Name))
			{
				return Collection;
			}
		}
	}

	return nullptr;
}

void UMoviePipelineRenderLayer::AddModifier(UMoviePipelineCollectionModifier* Modifier)
{
	if (!Modifiers.Contains(Modifier))
	{
		Modifiers.Add(Modifier);
	}
}

void UMoviePipelineRenderLayer::RemoveModifier(UMoviePipelineCollectionModifier* Modifier)
{
	Modifiers.Remove(Modifier);
}

void UMoviePipelineRenderLayer::Preview(const UWorld* World)
{
	if (!World)
	{
		return;
	}
	
	// Apply all modifiers
	for (UMoviePipelineCollectionModifier* Modifier : Modifiers)
	{
		Modifier->ApplyModifier(World);
	}
}

void UMoviePipelineRenderLayer::UndoPreview(const UWorld* World)
{
	if (!World)
	{
		return;
	}

	// Undo actions performed by all modifiers. Do this in the reverse order that they were applied, since the undo
	// state of one modifier may depend on modifiers that were previously applied.
	for (int32 Index = Modifiers.Num() - 1; Index >= 0; Index--)
	{
		if (UMoviePipelineCollectionModifier* Modifier = Modifiers[Index])
		{
			Modifier->UndoModifier();
		}
	}
}


UMoviePipelineRenderLayerSubsystem* UMoviePipelineRenderLayerSubsystem::GetFromWorld(const UWorld* World)
{
	if (World)
	{
		return UWorld::GetSubsystem<UMoviePipelineRenderLayerSubsystem>(World);
	}

	return nullptr;
}

void UMoviePipelineRenderLayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	VisualizationEmptyCollection = NewObject<UMoviePipelineCollection>(GetTransientPackage(), NAME_None, RF_Transient);

	// By default, the visualizer should hide everything in the world
	VisualizationModifier_HideWorld = NewObject<UMoviePipelineVisibilityModifier>(GetTransientPackage(), NAME_None, RF_Transient);
	VisualizationModifier_HideWorld->AddCollection(VisualizationEmptyCollection);
	VisualizationModifier_HideWorld->SetHidden(true);
	VisualizationModifier_HideWorld->SetIsInverted(true);

	// Selectively show collections in the visualization
	VisualizationModifier_VisibleCollections = NewObject<UMoviePipelineVisibilityModifier>(GetTransientPackage(), NAME_None, RF_Transient);

	// The visualizer render layer will hide the world, selectively show specified collections, and then apply any other provided modifiers
	VisualizationRenderLayer = NewObject<UMoviePipelineRenderLayer>(GetTransientPackage(), NAME_None, RF_Transient);
	VisualizationRenderLayer->AddModifier(VisualizationModifier_HideWorld);
	VisualizationRenderLayer->AddModifier(VisualizationModifier_VisibleCollections);
}

void UMoviePipelineRenderLayerSubsystem::Deinitialize()
{
}

void UMoviePipelineRenderLayerSubsystem::Reset()
{
	ClearAllPreviews();
	RenderLayers.Empty();
}

bool UMoviePipelineRenderLayerSubsystem::AddRenderLayer(UMoviePipelineRenderLayer* RenderLayer)
{
	if (!RenderLayer)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Invalid render layer provided to AddRenderLayer()."));
		return false;
	}
	
	const bool bRenderLayerExists = RenderLayers.ContainsByPredicate([RenderLayer](const UMoviePipelineRenderLayer* RL)
	{
		return RL && (RenderLayer->GetRenderLayerName() == RL->GetName());
	});

	if (bRenderLayerExists)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Render layer '%s' already exists in the render layer subsystem; it will not be added again."), *RenderLayer->GetRenderLayerName());
		return false;
	}

	RenderLayers.Add(RenderLayer);
	return true;
}

void UMoviePipelineRenderLayerSubsystem::RemoveRenderLayer(const FString& RenderLayerName)
{
	if (ActiveRenderLayer && (ActiveRenderLayer->GetName() == RenderLayerName))
	{
		ClearAllPreviews();
	}
	
	const uint32 Index = RenderLayers.IndexOfByPredicate([&RenderLayerName](const UMoviePipelineRenderLayer* RenderLayer)
	{
		return RenderLayer->GetRenderLayerName() == RenderLayerName;
	});

	if (Index != INDEX_NONE)
	{
		RenderLayers.RemoveAt(Index);
	}
}

void UMoviePipelineRenderLayerSubsystem::SetActiveRenderLayerByObj(UMoviePipelineRenderLayer* RenderLayer)
{
	if (!RenderLayer)
	{
		return;
	}
	
	ClearAllPreviews();
	SetAndPreviewRenderLayer(RenderLayer);
}

void UMoviePipelineRenderLayerSubsystem::SetActiveRenderLayerByName(const FString& RenderLayerName)
{
	const uint32 Index = RenderLayers.IndexOfByPredicate([&RenderLayerName](const UMoviePipelineRenderLayer* RenderLayer)
	{
		return RenderLayer->GetRenderLayerName() == RenderLayerName;
	});

	if (Index != INDEX_NONE)
	{
		SetActiveRenderLayerByObj(RenderLayers[Index]);
	}
}

void UMoviePipelineRenderLayerSubsystem::ClearActiveRenderLayer()
{
	ClearAllPreviews();
}

void UMoviePipelineRenderLayerSubsystem::PreviewCollection(UMoviePipelineCollection* Collection)
{
	if (!Collection)
	{
		return;
	}

	ClearAllPreviews();
	
	ActiveCollection = Collection;
	VisualizationModifier_VisibleCollections->AddCollection(ActiveCollection);

	SetAndPreviewRenderLayer(VisualizationRenderLayer);
}

void UMoviePipelineRenderLayerSubsystem::ClearCollectionPreview()
{
	ClearAllPreviews();
}

void UMoviePipelineRenderLayerSubsystem::ClearAllPreviews()
{
	// Render layer previews and collection previews both use the active render layer, so undoing the preview this
	// way will clear previews for both
	if (ActiveRenderLayer)
	{
		ActiveRenderLayer->UndoPreview(GetWorld());

		// Remove the modifier preview if present (requires an active render layer)
		if (ActiveModifier)
		{
			ActiveRenderLayer->RemoveModifier(ActiveModifier);
		}
	}

	// Reset the viz modifier for the collection preview
	VisualizationModifier_VisibleCollections->SetCollections({});

	ActiveRenderLayer = nullptr;
	ActiveCollection = nullptr;
	ActiveModifier = nullptr;
}

void UMoviePipelineRenderLayerSubsystem::SetAndPreviewRenderLayer(UMoviePipelineRenderLayer* RenderLayer)
{
	ActiveRenderLayer = RenderLayer;
	ActiveRenderLayer->Preview(GetWorld());
}

void UMoviePipelineRenderLayerSubsystem::PreviewModifier(UMoviePipelineCollectionModifier* Modifier)
{
	if (!Modifier)
	{
		return;
	}

	ClearAllPreviews();

	ActiveModifier = Modifier;
	VisualizationRenderLayer->AddModifier(ActiveModifier);

	// Add the modifier's collections to the viz as well, so the actors that the modifier affects are actually visible
	// TODO: This may need special handing for visibility modifiers
	for (UMoviePipelineCollection* Collection : ActiveModifier->GetCollections())
	{
		VisualizationModifier_VisibleCollections->AddCollection(Collection);
	}
	
	SetAndPreviewRenderLayer(VisualizationRenderLayer);
}

void UMoviePipelineRenderLayerSubsystem::ClearModifierPreview()
{
	ClearAllPreviews();
}