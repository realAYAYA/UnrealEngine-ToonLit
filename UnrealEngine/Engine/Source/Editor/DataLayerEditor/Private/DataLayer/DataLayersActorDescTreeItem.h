// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneOutlinerFwd.h"
#include "ActorDescTreeItem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "LevelInstance/LevelInstanceSubsystem.h"

//////////////////////////////////////////////////////////////////////////
// FDataLayerActorTreeItemData

class FWorldPartitionActorDesc;

struct FDataLayerActorDescTreeItemData
{
	FDataLayerActorDescTreeItemData(const FGuid& InActorGuid, UActorDescContainer* InContainer, UDataLayerInstance* InDataLayer)
		: ActorGuid(InActorGuid)
		, Container(InContainer)
		, DataLayer(InDataLayer)
	{}

	FDataLayerActorDescTreeItemData(const FGuid& InActorGuid, FActorDescContainerCollection* InContainerCollection, UDataLayerInstance* InDataLayer)
		: ActorGuid(InActorGuid)
		, Container(InContainerCollection->GetActorDescContainer(InActorGuid))
		, DataLayer(InDataLayer)
	{}
	
	const FGuid& ActorGuid;
	UActorDescContainer* const Container;
	TWeakObjectPtr<UDataLayerInstance> DataLayer;
};

//////////////////////////////////////////////////////////////////////////
// FDataLayerActorTreeItem

struct FDataLayerActorDescTreeItem : public FActorDescTreeItem
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FFilterPredicate, const FWorldPartitionActorDesc* ActorDesc, const UDataLayerInstance* DataLayer);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FInteractivePredicate, const FWorldPartitionActorDesc* ActorDesc, const UDataLayerInstance* DataLayer);

	FDataLayerActorDescTreeItem(const FDataLayerActorDescTreeItemData& InData)
		: FActorDescTreeItem(InData.ActorGuid, InData.Container)
		, DataLayer(InData.DataLayer)
		, IDDataLayerActorDesc(FDataLayerActorDescTreeItem::ComputeTreeItemID(InData.ActorGuid, DataLayer.Get(), FDataLayerActorDescTreeItem::GetParentActors(InData.Container)))
	{
		if (ActorDescHandle.IsValid())
		{
			UActorDescContainer* Container = ActorDescHandle->GetContainer();
			UWorld* OwningWorld = Container->GetWorld();
			ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(OwningWorld);
			ULevel* Level = Container->GetTypedOuter<UWorld>()->PersistentLevel;
			if (LevelInstanceSubsystem && (Level != OwningWorld->GetCurrentLevel()))
			{
				DisplayString = LevelInstanceSubsystem->PrefixWithParentLevelInstanceActorLabels(DisplayString, Level);
			}
		}
	}

	UDataLayerInstance* GetDataLayer() const { return DataLayer.Get(); }
	
	static uint32 ComputeTreeItemID(const FGuid& InActorGuid, const UDataLayerInstance* InDataLayer, const TArray<AActor*>& InParentActors)
	{
		uint32 ID = HashCombine(GetTypeHash(InActorGuid), GetTypeHash(FObjectKey(InDataLayer)));
		for (AActor* ParentActor : InParentActors)
		{
			ID = HashCombine(ID, GetTypeHash(ParentActor->GetActorGuid()));
		}
		return ID;
	}

	static TArray<AActor*> GetParentActors(UActorDescContainer* InContainer)
	{
		if (InContainer)
		{
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(InContainer->GetWorld()))
			{
				ULevel* Level = InContainer->GetTypedOuter<UWorld>()->PersistentLevel;
				return LevelInstanceSubsystem->GetParentLevelInstanceActors(Level);
			}
		}
		return TArray<AActor*>();
	}

	bool Filter(FFilterPredicate Pred) const
	{
		return Pred.Execute(ActorDescHandle.Get(), DataLayer.Get());
	}

	bool GetInteractiveState(FInteractivePredicate Pred) const
	{
		return Pred.Execute(ActorDescHandle.Get(), DataLayer.Get());
	}

	/* Begin ISceneOutlinerTreeItem Implementation */
	virtual bool IsValid() const override { return ActorDescHandle.IsValid() && DataLayer.IsValid(); }
	virtual FSceneOutlinerTreeItemID GetID() const override { return IDDataLayerActorDesc; }
	virtual bool ShouldShowVisibilityState() const { return false; }
	virtual bool HasVisibilityInfo() const override { return false; }
	virtual void OnVisibilityChanged(const bool bNewVisibility) override {}
	virtual bool GetVisibility() const override { return false; }
	/* End ISceneOutlinerTreeItem Implementation */

private:
	TWeakObjectPtr<UDataLayerInstance> DataLayer;
	const uint32 IDDataLayerActorDesc;
};