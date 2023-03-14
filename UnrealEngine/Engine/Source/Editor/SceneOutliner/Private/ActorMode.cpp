// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorMode.h"
#include "ActorHierarchy.h"
#include "Engine/Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "SceneOutlinerDelegates.h"
#include "ActorEditorUtils.h"
#include "LevelUtils.h"
#include "GameFramework/WorldSettings.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ActorTreeItem.h"
#include "LevelTreeItem.h"
#include "FolderTreeItem.h"
#include "ComponentTreeItem.h"
#include "ActorDescTreeItem.h"
#include "WorldTreeItem.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

static int32 GSceneOutlinerAutoRepresentingWorldNetMode = NM_Client;
static FAutoConsoleVariableRef CVarAutoRepresentingWorldNetMode(
	TEXT("SceneOutliner.AutoRepresentingWorldNetMode"),
	GSceneOutlinerAutoRepresentingWorldNetMode,
	TEXT("The preferred NetMode of the world shown in the scene outliner when the 'Auto' option is chosen: 0=Standalone, 1=DedicatedServer, 2=ListenServer, 3=Client"));

#define LOCTEXT_NAMESPACE "SceneOutliner_ActorMode"

using FActorFilter = TSceneOutlinerPredicateFilter<FActorTreeItem>;
using FFolderFilter = TSceneOutlinerPredicateFilter<FFolderTreeItem>;

namespace SceneOutliner
{
	bool FWeakActorSelector::operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, TWeakObjectPtr<AActor>& DataOut) const
	{
		if (TSharedPtr<ISceneOutlinerTreeItem> ItemPtr = Item.Pin())
		{
			if (FActorTreeItem* ActorItem = ItemPtr->CastTo<FActorTreeItem>())
			{
				if (ActorItem->IsValid())
				{
					DataOut = ActorItem->Actor;
					return true;
				}
			}
		}
		return false;
	}

	bool FActorSelector::operator()(const TWeakPtr<ISceneOutlinerTreeItem>& Item, AActor*& ActorPtrOut) const
	{
		if (TSharedPtr<ISceneOutlinerTreeItem> ItemPtr = Item.Pin())
		{
			if (FActorTreeItem* ActorItem = ItemPtr->CastTo<FActorTreeItem>())
			{
				if (ActorItem->IsValid())
				{
					AActor* Actor = ActorItem->Actor.Get();
					if (Actor)
					{
						ActorPtrOut = Actor;
						return true;
					}
				}
			}
			// If a component is selected, we meant for the owning actor to be selected
			else if (FComponentTreeItem* ComponentItem = ItemPtr->CastTo<FComponentTreeItem>())
			{
				if (ComponentItem->IsValid())
				{
					AActor* Actor = ComponentItem->Component->GetOwner();
					if (Actor)
					{
						ActorPtrOut = Actor;
						return true;
					}
				}
			}
		}

		return false;
	}
}

FActorMode::FActorMode(const FActorModeParams& Params)
	: ISceneOutlinerMode(Params.SceneOutliner)
	, SpecifiedWorldToDisplay(Params.SpecifiedWorldToDisplay)
	, bHideComponents(Params.bHideComponents)
	, bHideActorWithNoComponent(Params.bHideActorWithNoComponent)
	, bHideLevelInstanceHierarchy(Params.bHideLevelInstanceHierarchy)
	, bHideUnloadedActors(Params.bHideUnloadedActors)
	, bHideEmptyFolders(Params.bHideEmptyFolders)
{
	SceneOutliner->AddFilter(MakeShared<FActorFilter>(FActorTreeItem::FFilterPredicate::CreateLambda([this](const AActor* Actor)
	{
		return IsActorDisplayable(Actor);
	}), FSceneOutlinerFilter::EDefaultBehaviour::Pass));

	auto FolderPassesFilter = [this](const FFolder& InFolder, bool bInCheckHideLevelInstanceFlag)
	{
		if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(InFolder.GetRootObjectPtr()))
		{
			if (LevelInstance->IsEditing())
			{
				return true;
			}
			if (bInCheckHideLevelInstanceFlag)
			{
				return !bHideLevelInstanceHierarchy;
			}
		}
		if (ULevel* Level = Cast<ULevel>(InFolder.GetRootObjectPtr()))
		{
			return true;
		}
		return false;
	};

	SceneOutliner->AddFilter(MakeShared<FFolderFilter>(FFolderTreeItem::FFilterPredicate::CreateLambda([FolderPassesFilter](const FFolder& InFolder)
	{
		return FolderPassesFilter(InFolder, /*bCheckHideLevelInstanceFlag*/true);
	}), FSceneOutlinerFilter::EDefaultBehaviour::Pass));

	SceneOutliner->AddInteractiveFilter(MakeShared<FFolderFilter>(FFolderTreeItem::FFilterPredicate::CreateLambda([FolderPassesFilter](const FFolder& InFolder)
	{
		return FolderPassesFilter(InFolder, /*bCheckHideLevelInstanceFlag*/false);
	}), FSceneOutlinerFilter::EDefaultBehaviour::Pass));
}

FActorMode::~FActorMode()
{
}

TUniquePtr<ISceneOutlinerHierarchy> FActorMode::CreateHierarchy()
{
	TUniquePtr<FActorHierarchy> ActorHierarchy = FActorHierarchy::Create(this, RepresentingWorld);
	ActorHierarchy->SetShowingComponents(!bHideComponents);
	ActorHierarchy->SetShowingOnlyActorWithValidComponents(!bHideComponents && bHideActorWithNoComponent);
	ActorHierarchy->SetShowingLevelInstances(!bHideLevelInstanceHierarchy);
	ActorHierarchy->SetShowingUnloadedActors(!bHideUnloadedActors);
	ActorHierarchy->SetShowingEmptyFolders(!bHideEmptyFolders);

	return ActorHierarchy;
}

void FActorMode::Rebuild()
{
	ChooseRepresentingWorld();

	Hierarchy = CreateHierarchy();
}

void FActorMode::ChooseRepresentingWorld()
{
	// Select a world to represent

	RepresentingWorld = nullptr;

	// If a specified world was provided, represent it
	if (SpecifiedWorldToDisplay.IsValid())
	{
		RepresentingWorld = SpecifiedWorldToDisplay.Get();
	}

	// check if the user-chosen world is valid and in the editor contexts

	if (!RepresentingWorld.IsValid() && UserChosenWorld.IsValid())
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UserChosenWorld.Get() == Context.World())
			{
				RepresentingWorld = UserChosenWorld.Get();
				break;
			}
		}
	}

	// If the user did not manually select a world, try to pick the most suitable world context
	if (!RepresentingWorld.IsValid())
	{
		// Ideally we want a PIE world that is standalone or the first client, unless the preferred NetMode is overridden by CVar
		int32 LowestPIEInstanceSeen = MAX_int32;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && Context.WorldType == EWorldType::PIE)
			{
				if (World->GetNetMode() == NM_Standalone)
				{
					RepresentingWorld = World;
					break;
				}
				else if ((World->GetNetMode() == ENetMode(GSceneOutlinerAutoRepresentingWorldNetMode)) && (Context.PIEInstance < LowestPIEInstanceSeen))
				{
					RepresentingWorld = World;
					LowestPIEInstanceSeen = Context.PIEInstance;
				}
			}
		}
	}

	if (RepresentingWorld == nullptr)
	{
		// still no world so fallback to old logic where we just prefer PIE over Editor

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				RepresentingWorld = Context.World();
				break;
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				RepresentingWorld = Context.World();
			}
		}
	}
}

void FActorMode::BuildWorldPickerMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Worlds", LOCTEXT("WorldsHeading", "Worlds"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutoWorld", "Auto"),
			LOCTEXT("AutoWorldToolTip", "Automatically pick the world to display based on context."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FActorMode::OnSelectWorld, TWeakObjectPtr<UWorld>()),
				FCanExecuteAction(),
				FIsActionChecked::CreateRaw(this, &FActorMode::IsWorldChecked, TWeakObjectPtr<UWorld>())
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Editor))
			{
				MenuBuilder.AddMenuEntry(
					SceneOutliner::GetWorldDescription(World),
					LOCTEXT("ChooseWorldToolTip", "Display actors for this world."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateRaw(this, &FActorMode::OnSelectWorld, MakeWeakObjectPtr(World)),
						FCanExecuteAction(),
						FIsActionChecked::CreateRaw(this, &FActorMode::IsWorldChecked, MakeWeakObjectPtr(World))
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}
		}
	}
	MenuBuilder.EndSection();
}

void FActorMode::OnSelectWorld(TWeakObjectPtr<UWorld> World)
{
	UserChosenWorld = World;
	SceneOutliner->FullRefresh();
}

bool FActorMode::IsWorldChecked(TWeakObjectPtr<UWorld> World) const
{
	return (UserChosenWorld == World) || (World.IsExplicitlyNull() && !UserChosenWorld.IsValid());
}

void FActorMode::SynchronizeActorSelection()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();

	// Deselect actors in the tree that are no longer selected in the world
	const FSceneOutlinerItemSelection Selection(SceneOutliner->GetSelection());
	auto DeselectActors = [this](FActorTreeItem& Item)
	{
		if (!Item.Actor.IsValid() || !Item.Actor.Get()->IsSelected())
		{
			SceneOutliner->SetItemSelection(Item.AsShared(), false);
		}
	};
	Selection.ForEachItem<FActorTreeItem>(DeselectActors);

	// Show actor selection but only if sub objects are not selected
	if (!Selection.Has<FComponentTreeItem>())
	{
		// See if the tree view selector is pointing at a selected item
		bool bSelectorInSelectionSet = false;

		TArray<FSceneOutlinerTreeItemPtr> ActorItems;
		for (FSelectionIterator SelectionIt(*SelectedActors); SelectionIt; ++SelectionIt)
		{
			AActor* Actor = CastChecked< AActor >(*SelectionIt);
			if (FSceneOutlinerTreeItemPtr ActorItem = SceneOutliner->GetTreeItem(Actor))
			{
				if (!bSelectorInSelectionSet && SceneOutliner->HasSelectorFocus(ActorItem))
				{
					bSelectorInSelectionSet = true;
				}

				ActorItems.Add(ActorItem);
			}
		}

		// If NOT bSelectorInSelectionSet then we want to just move the selector to the first selected item.
		ESelectInfo::Type SelectInfo = bSelectorInSelectionSet ? ESelectInfo::Direct : ESelectInfo::OnMouseClick;
		SceneOutliner->AddToSelection(ActorItems, SelectInfo);
	}
	FSceneOutlinerDelegates::Get().SelectionChanged.Broadcast();
}

bool FActorMode::IsActorDisplayable(const AActor* Actor) const
{
	return FActorMode::IsActorDisplayable(SceneOutliner, Actor);
}

FFolder::FRootObject FActorMode::GetRootObject() const
{
	return FFolder::GetWorldRootFolder(RepresentingWorld.Get()).GetRootObject();
}

FFolder::FRootObject FActorMode::GetPasteTargetRootObject() const
{
	if (UWorld* World = RepresentingWorld.Get())
	{
		return FFolder::GetOptionalFolderRootObject(World->GetCurrentLevel()).Get(FFolder::GetWorldRootFolder(World).GetRootObject());
	}
	return FFolder::GetInvalidRootObject();
}

bool FActorMode::IsActorDisplayable(const SSceneOutliner* SceneOutliner, const AActor* Actor)
{
	return Actor &&
		!SceneOutliner->GetSharedData().bOnlyShowFolders && 									// Don't show actors if we're only showing folders
		Actor->IsEditable() &&																	// Only show actors that are allowed to be selected and drawn in editor
		Actor->IsListedInSceneOutliner() &&
		(((Actor->GetWorld() && Actor->GetWorld()->IsPlayInEditor()) || !Actor->HasAnyFlags(RF_Transient)) ||
		(SceneOutliner->GetSharedData().bShowTransient && Actor->HasAnyFlags(RF_Transient))) &&	// Don't show transient actors in non-play worlds
		!Actor->IsTemplate() &&																	// Should never happen, but we never want CDOs displayed
		!FActorEditorUtils::IsABuilderBrush(Actor) &&											// Don't show the builder brush
		!Actor->IsA(AWorldSettings::StaticClass()) &&											// Don't show the WorldSettings actor, even though it is technically editable
		IsValidChecked(Actor) &&																// We don't want to show actors that are about to go away
		FLevelUtils::IsLevelVisible(Actor->GetLevel());											// Only show Actors whose level is visible
}

bool FActorMode::IsActorLevelDisplayable(ULevel* InLevel)
{
	// Don't show level tree item for the persistent level
	return (InLevel && !InLevel->IsPersistentLevel());
}

void FActorMode::OnFilterTextChanged(const FText& InFilterText)
{
	// Scroll last item (if it passes the filter) into view - this means if we are multi-selecting, we show newest selection that passes the filter
	if (const AActor* LastSelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>())
	{
		// This part is different than that of OnLevelSelectionChanged(nullptr) because IsItemVisible(TreeItem) & ScrollItemIntoView(TreeItem) are applied to
		// the current visual state, not to the one after applying the filter. Thus, the scroll would go to the place where the object was located
		// before applying the FilterText

		// If the object is already in the list, but it does not passes the filter, then we do not want to re-add it, because it will be removed by the filter
		const FSceneOutlinerTreeItemPtr TreeItem = SceneOutliner->GetTreeItem(LastSelectedActor);
		if (!TreeItem.IsValid() || !SceneOutliner->PassesTextFilter(TreeItem))
		{
			return;
		}

		// If the object is not in the list, and it does not passes the filter, then we should not re-add it, because it would be removed by the filter again. Unfortunately,
		// there is no code to check if a future element (i.e., one that is currently not in the TreeItemMap list) will pass the filter. Therefore, we kind of overkill it
		// by re-adding that element (even though it will be removed). However, AddItemToTree(FSceneOutlinerTreeItemRef Item) and similar functions already check the element before
		// adding it. So this solution is fine.
		// This solution might affect the performance of the World Outliner when a key is pressed, but it will still work properly when the remove/del keys are pressed. Not
		// updating the filter when !TreeItem.IsValid() would result in the focus not being updated when the remove/del keys are pressed.

		// In any other case (i.e., if the object passes the current filter), re-add it
		SceneOutliner->ScrollItemIntoView(TreeItem);

		SetAsMostRecentOutliner();
	}
}

void FActorMode::SetAsMostRecentOutliner() const
{
	TWeakPtr<ILevelEditor> LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorInstance();

	if(TSharedPtr<ILevelEditor> LevelEditorPin = LevelEditor.Pin())
	{
		LevelEditorPin->SetMostRecentlyUsedSceneOutliner(SceneOutliner->GetOutlinerIdentifier());
	}
}

int32 FActorMode::GetTypeSortPriority(const ISceneOutlinerTreeItem& Item) const
{
	if (Item.IsA<FWorldTreeItem>())
	{
		return EItemSortOrder::World;
	}
	else if (Item.IsA<FLevelTreeItem>())
	{
		return EItemSortOrder::Level;
	}
	else if (Item.IsA<FFolderTreeItem>())
	{
		return EItemSortOrder::Folder;
	}
	else if (Item.IsA<FActorTreeItem>() || Item.IsA<FComponentTreeItem>())
	{
		return EItemSortOrder::Actor;
	}
	else if (Item.IsA<FActorDescTreeItem>())
	{
		return EItemSortOrder::Unloaded;
	}

	// Warning: using actor mode with an unsupported item type!
	check(false);
	return -1;
}

#undef LOCTEXT_NAMESPACE