// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "SceneOutlinerFwd.h"
#include "ActorDescTreeItem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/WorldPartition.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Misc/ArchiveMD5.h"
#include "Misc/SecureHash.h"
#include "UObject/ObjectKey.h"


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
		, IDDataLayerActorDesc(FDataLayerActorDescTreeItem::ComputeTreeItemID(InData.ActorGuid, InData.Container, InData.DataLayer.Get()))
	{
		if (ActorDescHandle.IsValid())
		{
			UActorDescContainer* Container = ActorDescHandle->GetContainer();
			UWorld* OwningWorld = Container->GetWorldPartition()->GetWorld();
			ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(OwningWorld);
			ULevel* Level = Container->GetTypedOuter<UWorld>()->PersistentLevel;
			if (LevelInstanceSubsystem && Level && (Level != OwningWorld->GetCurrentLevel()))
			{
				DisplayString = LevelInstanceSubsystem->PrefixWithParentLevelInstanceActorLabels(DisplayString, Level);
			}
		}
	}

	UDataLayerInstance* GetDataLayer() const { return DataLayer.Get(); }
	
	static FSceneOutlinerTreeItemID ComputeTreeItemID(FGuid InActorGuid, UActorDescContainer* InContainer, const UDataLayerInstance* InDataLayer)
	{
		FObjectKey ContainerKey(InContainer);
		FObjectKey DataLayerInstanceKey(InDataLayer);

		FArchiveMD5 Ar;
		Ar << InActorGuid << ContainerKey << DataLayerInstanceKey;

		return FSceneOutlinerTreeItemID(Ar.GetGuidFromHash());
	}

	static TArray<AActor*> GetParentActors(UActorDescContainer* InContainer)
	{
		if (InContainer)
		{
			UWorld* OwningWorld = InContainer->GetWorldPartition()->GetWorld();
			if (ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(OwningWorld))
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
	const FSceneOutlinerTreeItemID IDDataLayerActorDesc;
};