// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerHierarchy.h"

#include "DataLayer/DataLayerEditorSubsystem.h"
#include "DataLayerActorTreeItem.h"
#include "DataLayerMode.h"
#include "DataLayerTreeItem.h"
#include "DataLayersActorDescTreeItem.h"
#include "Delegates/Delegate.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"
#include "ISceneOutlinerMode.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "WorldDataLayersTreeItem.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

TUniquePtr<FDataLayerHierarchy> FDataLayerHierarchy::Create(FDataLayerMode* Mode, const TWeakObjectPtr<UWorld>& World)
{
	return TUniquePtr<FDataLayerHierarchy>(new FDataLayerHierarchy(Mode, World));
}

FDataLayerHierarchy::FDataLayerHierarchy(FDataLayerMode* Mode, const TWeakObjectPtr<UWorld>& World)
	: ISceneOutlinerHierarchy(Mode)
	, RepresentingWorld(World)
	, bShowEditorDataLayers(true)
	, bShowRuntimeDataLayers(true)
	, bShowDataLayerActors(true)
	, bShowUnloadedActors(true)
	, bShowOnlySelectedActors(false)
	, bHighlightSelectedDataLayers(false)
	, bShowLevelInstanceContent(false)
{
	if (GEngine)
	{
		GEngine->OnLevelActorAdded().AddRaw(this, &FDataLayerHierarchy::OnLevelActorAdded);
		GEngine->OnLevelActorDeleted().AddRaw(this, &FDataLayerHierarchy::OnLevelActorDeleted);
		GEngine->OnLevelActorListChanged().AddRaw(this, &FDataLayerHierarchy::OnLevelActorListChanged);
	}

	IWorldPartitionEditorModule& WorldPartitionEditorModule = FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");
	WorldPartitionEditorModule.OnWorldPartitionCreated().AddRaw(this, &FDataLayerHierarchy::OnWorldPartitionCreated);

	if (World.IsValid())
	{
		if (World->PersistentLevel)
		{
			World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddRaw(this, &FDataLayerHierarchy::OnLoadedActorAdded);
			World->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.AddRaw(this, &FDataLayerHierarchy::OnLoadedActorRemoved);
		}

		World->OnWorldPartitionInitialized().AddRaw(this, &FDataLayerHierarchy::OnWorldPartitionInitialized);
		World->OnWorldPartitionUninitialized().AddRaw(this, &FDataLayerHierarchy::OnWorldPartitionUninitialized);
		
		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			WorldPartition->OnActorDescAddedEvent.AddRaw(this, &FDataLayerHierarchy::OnActorDescAdded);
			WorldPartition->OnActorDescRemovedEvent.AddRaw(this, &FDataLayerHierarchy::OnActorDescRemoved);
		}
	}

	UDataLayerEditorSubsystem::Get()->OnDataLayerChanged().AddRaw(this, &FDataLayerHierarchy::OnDataLayerChanged);
	UDataLayerEditorSubsystem::Get()->OnActorDataLayersChanged().AddRaw(this, &FDataLayerHierarchy::OnActorDataLayersChanged);
	FWorldDelegates::LevelAddedToWorld.AddRaw(this, &FDataLayerHierarchy::OnLevelAdded);
	FWorldDelegates::LevelRemovedFromWorld.AddRaw(this, &FDataLayerHierarchy::OnLevelRemoved);
}

FDataLayerHierarchy::~FDataLayerHierarchy()
{
	if (GEngine)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
	}

	IWorldPartitionEditorModule& WorldPartitionEditorModule = FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");
	WorldPartitionEditorModule.OnWorldPartitionCreated().RemoveAll(this);

	if (RepresentingWorld.IsValid())
	{
		if (RepresentingWorld->PersistentLevel)
		{
			RepresentingWorld->PersistentLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
			RepresentingWorld->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.RemoveAll(this);
		}

		RepresentingWorld->OnWorldPartitionInitialized().RemoveAll(this);
		RepresentingWorld->OnWorldPartitionUninitialized().RemoveAll(this);
		
		if (UWorldPartition* WorldPartition = RepresentingWorld->GetWorldPartition())
		{
			WorldPartition->OnActorDescAddedEvent.RemoveAll(this);
			WorldPartition->OnActorDescRemovedEvent.RemoveAll(this);
		}
	}

	UDataLayerEditorSubsystem::Get()->OnDataLayerChanged().RemoveAll(this);
	UDataLayerEditorSubsystem::Get()->OnActorDataLayersChanged().RemoveAll(this);
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
}


bool FDataLayerHierarchy::IsDataLayerPartOfSelection(const UDataLayerInstance* DataLayerInstance) const
{
	if (!bShowOnlySelectedActors)
	{
		return true;
	}

	if (UDataLayerEditorSubsystem::Get()->DoesDataLayerContainSelectedActors(DataLayerInstance))
	{
		return true;
	}

	bool bFoundSelected = false;
	DataLayerInstance->ForEachChild([this, &bFoundSelected](const UDataLayerInstance* Child)
	{
		bFoundSelected = IsDataLayerPartOfSelection(Child);
		return !bFoundSelected; // Continue iterating if not found
	});
	return bFoundSelected;
};

FSceneOutlinerTreeItemPtr FDataLayerHierarchy::CreateDataLayerTreeItem(UDataLayerInstance* InDataLayer, bool bInForce) const
{
	FSceneOutlinerTreeItemPtr Item = Mode->CreateItemFor<FDataLayerTreeItem>(InDataLayer, bInForce);
	if (FDataLayerTreeItem* DataLayerTreeItem = Item ? Item->CastTo<FDataLayerTreeItem>() : nullptr)
	{
		DataLayerTreeItem->SetIsHighlightedIfSelected(bHighlightSelectedDataLayers);
	}
	return Item;
}

UWorld* FDataLayerHierarchy::GetOwningWorld() const
{
	UWorld* World = RepresentingWorld.Get();
	return World ? World->PersistentLevel->GetWorld() : nullptr;
}

void FDataLayerHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	const UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetOwningWorld());

	auto IsDataLayerShown = [this](const UDataLayerInstance* DataLayerInstance)
	{
		const bool bIsRuntimeDataLayer = DataLayerInstance->IsRuntime();
		return ((bIsRuntimeDataLayer && bShowRuntimeDataLayers) || (!bIsRuntimeDataLayer && bShowEditorDataLayers)) && IsDataLayerPartOfSelection(DataLayerInstance);
	};

	auto CreateDataLayerTreeItems = [this, DataLayerSubsystem, &OutItems, IsDataLayerShown](const ULevel* OuterLevel)
	{
		DataLayerSubsystem->ForEachDataLayer([this, &OutItems, IsDataLayerShown](UDataLayerInstance* DataLayer)
		{
			if (IsDataLayerShown(DataLayer))
			{
				if (FSceneOutlinerTreeItemPtr DataLayerItem = CreateDataLayerTreeItem(DataLayer))
				{
					OutItems.Add(DataLayerItem);
				}
			}
			return true;
		}, OuterLevel);
	};

	CreateDataLayerTreeItems(GetOwningWorld()->PersistentLevel);

	// CurrentLevel represents the current level if different than the PersistentLevel
	ULevel* CurrentLevel = (GetOwningWorld()->GetCurrentLevel() && !GetOwningWorld()->GetCurrentLevel()->IsPersistentLevel()) ? GetOwningWorld()->GetCurrentLevel() : nullptr;
	if (CurrentLevel != nullptr)
	{
		CreateDataLayerTreeItems(CurrentLevel);
	}

	// Create Tree items for WorldDataLayers(WDL) which are part of the current world (not subworld) but are not the ULevel::AWorldDataLayers.
	// The only way to create data layer instances for in these WDL is to right-click on the WDL tree item and select create new.
	// We cannot rely on creating the WDL tree item only if it has DataLayerInstance (::FindOrCreateParentItem), as it prevents to initially create a new instance.
	DataLayerSubsystem->ForEachWorldDataLayer([this, &OutItems](AWorldDataLayers* WorldDataLayers)
	{
		if (!WorldDataLayers->IsSubWorldDataLayers() && !WorldDataLayers->IsTheMainWorldDataLayers())
		{
			FSceneOutlinerTreeItemPtr WorldDataLayerItem = Mode->CreateItemFor<FWorldDataLayersTreeItem>(WorldDataLayers, true);
			OutItems.Add(WorldDataLayerItem);
		}

		return true;
	});

	if (bShowDataLayerActors)
	{
		for (AActor* Actor : FActorRange(GetOwningWorld()))
		{
			// Only consider actors of current level (if there is one)
			if (!CurrentLevel || (Actor->GetLevel() == CurrentLevel))
			{
				for (const UDataLayerInstance* DataLayerInstance : Actor->GetDataLayerInstances())
				{
					if (IsDataLayerShown(DataLayerInstance))
					{
						if (FSceneOutlinerTreeItemPtr DataLayerActorItem = Mode->CreateItemFor<FDataLayerActorTreeItem>(FDataLayerActorTreeItemData(Actor, const_cast<UDataLayerInstance*>(DataLayerInstance))))
						{
							OutItems.Add(DataLayerActorItem);
						}
					}
				}
			}
		}

		if (bShowUnloadedActors)
		{
			ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(GetOwningWorld());
			if (UWorldPartitionSubsystem* WorldPartitionSubsystem = UWorld::GetSubsystem<UWorldPartitionSubsystem>(GetOwningWorld()))
			{
				WorldPartitionSubsystem->ForEachWorldPartition([this, CurrentLevel, LevelInstanceSubsystem, DataLayerSubsystem, IsDataLayerShown, &OutItems](UWorldPartition* WorldPartition)
				{
					// Skip WorldPartition if it's not the one of the current level (the editing level instance)
					// or if we hide the content of level instances
					UWorld* OuterWorld = WorldPartition->GetTypedOuter<UWorld>();
					ULevel* OuterLevel = OuterWorld ? OuterWorld->PersistentLevel : nullptr;
					if ((CurrentLevel && (CurrentLevel != OuterLevel)) ||
						(!bShowLevelInstanceContent && OuterLevel && LevelInstanceSubsystem && LevelInstanceSubsystem->GetOwningLevelInstance(OuterLevel)))
					{
						return true;
					}

					// Create an FDataLayerActorDescTreeItem for each unloaded actor of this WorldPartition
					FWorldPartitionHelpers::ForEachActorDesc(WorldPartition, [this, DataLayerSubsystem, IsDataLayerShown, &OutItems](const FWorldPartitionActorDesc* ActorDesc)
					{
						if (ActorDesc != nullptr && !ActorDesc->IsLoaded())
						{
							for (const FName& DataLayerInstanceName : ActorDesc->GetDataLayerInstanceNames())
							{
								if (const UDataLayerInstance* const DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName))
								{
									if (IsDataLayerShown(DataLayerInstance))
									{
										if (const FSceneOutlinerTreeItemPtr ActorDescItem = Mode->CreateItemFor<FDataLayerActorDescTreeItem>(FDataLayerActorDescTreeItemData(ActorDesc->GetGuid(), ActorDesc->GetContainer(), const_cast<UDataLayerInstance*>(DataLayerInstance))))
										{
											OutItems.Add(ActorDescItem);
										}
									}
								}
							}
						}
						return true;
					});
					return true;
				});
			}
		}
	}
}

FSceneOutlinerTreeItemPtr FDataLayerHierarchy::FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item, const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate)
{
	if (const FDataLayerTreeItem* DataLayerTreeItem = Item.CastTo<FDataLayerTreeItem>())
	{
		if (UDataLayerInstance* DataLayerInstance = DataLayerTreeItem->GetDataLayer())
		{
			if (UDataLayerInstance* ParentDataLayer = DataLayerInstance->GetParent())
			{
				if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(ParentDataLayer))
				{
					return *ParentItem;
				}
				else if (bCreate)
				{
					return CreateDataLayerTreeItem(ParentDataLayer, true);
				}
			}
			else
			{
				// Parent WorldDataLayers
				if (AWorldDataLayers* OuterWorldDataLayers = DataLayerInstance->GetOuterAWorldDataLayers())
				{
					if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(OuterWorldDataLayers))
					{
						return *ParentItem;
					}
					else if (bCreate)
					{
						return Mode->CreateItemFor<FWorldDataLayersTreeItem>(OuterWorldDataLayers, true);
					}
				}
			}
		}
	}
	else if (const FDataLayerActorTreeItem* DataLayerActorTreeItem = Item.CastTo<FDataLayerActorTreeItem>())
	{
		if (UDataLayerInstance* DataLayerInstance = DataLayerActorTreeItem->GetDataLayer())
		{
			if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(DataLayerInstance))
			{
				return *ParentItem;
			}
			else if (bCreate)
			{
				return CreateDataLayerTreeItem(DataLayerInstance, true);
			}
		}
	}
	else if (const FDataLayerActorDescTreeItem* DataLayerActorDescTreeItem = Item.CastTo<FDataLayerActorDescTreeItem>())
	{
		if (UDataLayerInstance* DataLayerInstance = DataLayerActorDescTreeItem->GetDataLayer())
		{
			if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(DataLayerInstance))
			{
				return *ParentItem;
			}
			else if (bCreate)
			{
				return CreateDataLayerTreeItem(DataLayerInstance, true);
			}
		}
	}
	return nullptr;
}

void FDataLayerHierarchy::OnWorldPartitionCreated(UWorld* InWorld)
{
	if (RepresentingWorld.Get() == InWorld)
	{
		FullRefreshEvent();
	}
}

void FDataLayerHierarchy::OnLevelActorsAdded(const TArray<AActor*>& InActors)
{
	if (!bShowDataLayerActors)
	{
		return;
	}

	if (UWorld* OwningWorld = GetOwningWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Added;

		for (AActor* Actor : InActors)
		{
			if (Actor && (Actor->GetWorld() == OwningWorld))
			{
				TArray<const UDataLayerInstance*> DataLayerInstances = Actor->GetDataLayerInstances();
				EventData.Items.Reserve(DataLayerInstances.Num());
				for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
				{
					EventData.Items.Add(Mode->CreateItemFor<FDataLayerActorTreeItem>(FDataLayerActorTreeItemData(Actor, const_cast<UDataLayerInstance*>(DataLayerInstance))));
				}
			}
		}

		if (!EventData.Items.IsEmpty())
		{
			HierarchyChangedEvent.Broadcast(EventData);
		}
	}
}

void FDataLayerHierarchy::OnLevelActorsRemoved(const TArray<AActor*>& InActors)
{
	if (UWorld* OwningWorld = GetOwningWorld())
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;

		for (AActor* Actor : InActors)
		{
			if (Actor != nullptr)
			{
				const TArray<const UDataLayerInstance*> DataLayerInstances = Actor->GetDataLayerInstances();
				for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
				{
					EventData.ItemIDs.Add(FDataLayerActorTreeItem::ComputeTreeItemID(Actor, DataLayerInstance));
				}
			}
		}

		if (!EventData.ItemIDs.IsEmpty())
		{
			HierarchyChangedEvent.Broadcast(EventData);
		}
	}
}

void FDataLayerHierarchy::OnLevelActorAdded(AActor* InActor)
{
	OnLevelActorsAdded({ InActor });
}

void FDataLayerHierarchy::OnActorDataLayersChanged(const TWeakObjectPtr<AActor>& InActor)
{
	AActor* Actor = InActor.Get();
	if (Actor && (GetOwningWorld() == Actor->GetWorld()))
	{
		FullRefreshEvent();
	}
}

void FDataLayerHierarchy::OnDataLayerChanged(const EDataLayerAction Action, const TWeakObjectPtr<const UDataLayerInstance>& ChangedDataLayer, const FName& ChangedProperty)
{
	const UDataLayerInstance* DataLayerInstance = ChangedDataLayer.Get();
	if (DataLayerInstance || (Action == EDataLayerAction::Delete) || (Action == EDataLayerAction::Reset))
	{
		FullRefreshEvent();
	}
}

void FDataLayerHierarchy::OnLevelActorDeleted(AActor* InActor)
{
	OnLevelActorsRemoved({ InActor });
}

void FDataLayerHierarchy::OnLevelActorListChanged()
{
	FullRefreshEvent();
}

void FDataLayerHierarchy::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	FullRefreshEvent();
}

void FDataLayerHierarchy::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	FullRefreshEvent();
}

void FDataLayerHierarchy::OnLevelAdded(ULevel* InLevel, UWorld* InWorld)
{
	if (!InWorld->IsGameWorld() && InLevel && (GetOwningWorld() == InWorld))
	{
		OnLevelActorsAdded(InLevel->Actors);
	}
}

void FDataLayerHierarchy::OnLevelRemoved(ULevel* InLevel, UWorld* InWorld)
{
	if (!InWorld->IsGameWorld() && InLevel && (GetOwningWorld() == InWorld))
	{
		OnLevelActorsRemoved(InLevel->Actors);
	}
}

void FDataLayerHierarchy::OnLoadedActorAdded(AActor& InActor)
{
	if (!bShowDataLayerActors)
	{
		return;
	}

	// Handle the actor being added to the level
	OnLevelActorAdded(&InActor);

	// Remove corresponding actor desc items
	if ((GetOwningWorld() == InActor.GetWorld()))
	{
		const TArray<const UDataLayerInstance*> DataLayerInstances = InActor.GetDataLayerInstances();
		if (DataLayerInstances.Num() > 0)
		{
			FSceneOutlinerHierarchyChangedData EventData;
			EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
			EventData.ItemIDs.Reserve(DataLayerInstances.Num());
			ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(GetOwningWorld());
			TArray<AActor*> ParentActors = LevelInstanceSubsystem->GetParentLevelInstanceActors(InActor.GetLevel());

			for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
			{
				EventData.ItemIDs.Add(FDataLayerActorDescTreeItem::ComputeTreeItemID(InActor.GetActorGuid(), DataLayerInstance, ParentActors));
			}
			HierarchyChangedEvent.Broadcast(EventData);
		}
	}
}

void FDataLayerHierarchy::OnLoadedActorRemoved(AActor& InActor)
{
	// Handle the actor being removed from the level
	OnLevelActorDeleted(&InActor);

	// Add any corresponding actor desc items for this actor
	UWorldPartition* const WorldPartition = RepresentingWorld->GetWorldPartition();
	if ((GetOwningWorld() == InActor.GetWorld()) && WorldPartition != nullptr)
	{
		const TArray<const UDataLayerInstance*> DataLayerInstances = InActor.GetDataLayerInstances();
		if (DataLayerInstances.Num() > 0)
		{
			FSceneOutlinerHierarchyChangedData EventData;
			EventData.Type = FSceneOutlinerHierarchyChangedData::Added;
			EventData.Items.Reserve(DataLayerInstances.Num());
			for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
			{
				EventData.Items.Add(Mode->CreateItemFor<FDataLayerActorDescTreeItem>(FDataLayerActorDescTreeItemData(InActor.GetActorGuid(), WorldPartition, const_cast<UDataLayerInstance*>(DataLayerInstance))));
			}
			HierarchyChangedEvent.Broadcast(EventData);
		}
    }
}

void FDataLayerHierarchy::OnActorDescAdded(FWorldPartitionActorDesc* InActorDesc)
{
	if (!bShowUnloadedActors || !InActorDesc || InActorDesc->IsLoaded(true))
	{
		return;
	}

	UWorldPartition* const WorldPartition = RepresentingWorld->GetWorldPartition();
	const UDataLayerSubsystem* const DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetOwningWorld());
	const TArray<FName>& DataLayerInstanceNames = InActorDesc->GetDataLayerInstanceNames();

	if (DataLayerSubsystem && DataLayerInstanceNames.Num() > 0)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Added;

		EventData.Items.Reserve(DataLayerInstanceNames.Num());
		for (const FName& DataLayerInstanceName : DataLayerInstanceNames)
		{
			const UDataLayerInstance* const DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName);
			EventData.Items.Add(Mode->CreateItemFor<FDataLayerActorDescTreeItem>(FDataLayerActorDescTreeItemData(InActorDesc->GetGuid(), WorldPartition, const_cast<UDataLayerInstance*>(DataLayerInstance))));
		}
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FDataLayerHierarchy::OnActorDescRemoved(FWorldPartitionActorDesc* InActorDesc)
{
	if (!bShowUnloadedActors || (InActorDesc == nullptr))
	{
		return;
	}

	const UDataLayerSubsystem* const DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetOwningWorld());
	const TArray<FName>& DataLayerInstanceNames = InActorDesc->GetDataLayerInstanceNames();
	
	if (DataLayerSubsystem && DataLayerInstanceNames.Num() > 0)
	{
		FSceneOutlinerHierarchyChangedData EventData;
		EventData.Type = FSceneOutlinerHierarchyChangedData::Removed;
		EventData.ItemIDs.Reserve(DataLayerInstanceNames.Num());
		TArray<AActor*> ParentActors = FDataLayerActorDescTreeItem::GetParentActors(InActorDesc->GetContainer());

		for (const FName& DataLayerInstanceName : DataLayerInstanceNames)
		{
			const UDataLayerInstance* const DataLayer = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName);
			EventData.ItemIDs.Add(FDataLayerActorDescTreeItem::ComputeTreeItemID(InActorDesc->GetGuid(), DataLayer, ParentActors));
		}
		HierarchyChangedEvent.Broadcast(EventData);
	}
}

void FDataLayerHierarchy::FullRefreshEvent()
{
	FSceneOutlinerHierarchyChangedData EventData;
	EventData.Type = FSceneOutlinerHierarchyChangedData::FullRefresh;
	HierarchyChangedEvent.Broadcast(EventData);
}
