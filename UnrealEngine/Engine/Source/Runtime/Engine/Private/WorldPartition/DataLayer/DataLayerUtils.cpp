// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerUtils.h"

#if WITH_EDITOR
#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerType.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "Algo/Transform.h"
#include "Algo/AllOf.h"

TArray<FName> FDataLayerUtils::ResolvedDataLayerInstanceNames(const FWorldPartitionActorDesc* InActorDesc, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, UWorld* InWorld, bool* bOutIsResultValid)
{
	bool bLocalIsSuccess = true;
	bool& bIsSuccess = bOutIsResultValid ? *bOutIsResultValid : bLocalIsSuccess;
	bIsSuccess = true;

	// Prioritize in-memory AWorldDataLayers
	UWorld* World = InWorld;
	if (!World)
	{
		World = InActorDesc->GetContainer() ? InActorDesc->GetContainer()->GetWorld() : nullptr;
	}

	const UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(World);

	// DataLayers not using DataLayer Assets represent DataLayerInstanceNames
	if (!InActorDesc->IsUsingDataLayerAsset())
	{
		if (DataLayerSubsystem && DataLayerSubsystem->CanResolveDataLayers())
		{
			TArray<FName> Result;
			for (const FName& DataLayerInstanceName : InActorDesc->GetDataLayers())
			{
				if (const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName))
				{
					Result.Add(DataLayerInstanceName);
				}
			}
			return Result;
		}
		// Fallback on FWorldDataLayersActorDesc
		else if (!InWorldDataLayersActorDescs.IsEmpty() && AreWorldDataLayersActorDescsSane(InWorldDataLayersActorDescs))
		{
			TArray<FName> Result;
			for (const FName& DataLayerInstanceName : InActorDesc->GetDataLayers())
			{
				if (GetDataLayerInstanceDescFromInstanceName(InWorldDataLayersActorDescs, DataLayerInstanceName) != nullptr)
				{
					Result.Add(DataLayerInstanceName);
				}
			}
			return Result;
		}
	}
	// ActorDesc DataLayers represents DataLayer Asset Paths
	else
	{
		if (DataLayerSubsystem && DataLayerSubsystem->CanResolveDataLayers())
		{
			TArray<FName> Result;
			for (const FName& DataLayerAssetPath : InActorDesc->GetDataLayers())
			{
				DataLayerSubsystem->ForEachDataLayer([DataLayerAssetPath, &Result](UDataLayerInstance* DataLayerInstance)
				{
					const UDataLayerInstanceWithAsset* DataLayerInstanceWithAsset = Cast<UDataLayerInstanceWithAsset>(DataLayerInstance);
					const UDataLayerAsset* DataLayerAsset = DataLayerInstanceWithAsset ? DataLayerInstanceWithAsset->GetAsset() : nullptr;
					if (DataLayerAsset && (FName(DataLayerAsset->GetPathName()) == DataLayerAssetPath))
					{
						Result.Add(DataLayerInstance->GetDataLayerFName());
						return false;
					}
					return true;
				});
			}
			return Result;
		}
		// Fallback on FWorldDataLayersActorDesc
		else if (!InWorldDataLayersActorDescs.IsEmpty() && AreWorldDataLayersActorDescsSane(InWorldDataLayersActorDescs))
		{
			TArray<FName> Result;
			for (const FName& DataLayerAssetPath : InActorDesc->GetDataLayers())
			{
				if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = GetDataLayerInstanceDescFromAssetPath(InWorldDataLayersActorDescs, DataLayerAssetPath))
				{
					Result.Add(DataLayerInstanceDesc->GetName());
				}
			}
			return Result;
		}
	}

	bIsSuccess = false;
	return InActorDesc->GetDataLayers();
}

// For performance reasons, this function assumes that InActorDesc's DataLayerInstanceNames was already resolved.
bool FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(const FWorldPartitionActorDescView& InActorDescView, const FActorDescViewMap& ActorDescViewMap, TArray<FName>& OutRuntimeDataLayerInstanceNames)
{
	UWorld* World = InActorDescView.GetActorDesc()->GetContainer()->GetWorld();
	const UDataLayerSubsystem* DataLayerSubsystem = World ? World->GetSubsystem<UDataLayerSubsystem>() : nullptr;

	if (DataLayerSubsystem && DataLayerSubsystem->CanResolveDataLayers())
	{
		for (FName DataLayerInstanceName : InActorDescView.GetDataLayerInstanceNames())
		{
			const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName);
			if (DataLayerInstance && DataLayerInstance->IsRuntime())
			{
				OutRuntimeDataLayerInstanceNames.Add(DataLayerInstanceName);
			}
		}

		return true;
	}
	else
	{
		TArray<const FWorldDataLayersActorDesc*> WorldDataLayersActorDescs;
		FindWorldDataLayerActorDescs(ActorDescViewMap, WorldDataLayersActorDescs);

		// Fallback on FWorldDataLayersActorDesc
		if (WorldDataLayersActorDescs.Num() && AreWorldDataLayersActorDescsSane(WorldDataLayersActorDescs))
		{
			for (FName DataLayerInstanceName : InActorDescView.GetDataLayerInstanceNames())
			{
				if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = GetDataLayerInstanceDescFromInstanceName(WorldDataLayersActorDescs, DataLayerInstanceName))
				{
					if (DataLayerInstanceDesc->GetDataLayerType() == EDataLayerType::Runtime)
					{
						OutRuntimeDataLayerInstanceNames.Add(DataLayerInstanceName);
					}
				}
			}

			return true;
		}
	}

	return false;
}

const FDataLayerInstanceDesc* FDataLayerUtils::GetDataLayerInstanceDescFromInstanceName(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, const FName& DataLayerInstanceName)
{
	for (const FWorldDataLayersActorDesc* WorldDataLayerActorDesc : InWorldDataLayersActorDescs)
	{
		if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = WorldDataLayerActorDesc->GetDataLayerInstanceFromInstanceName(DataLayerInstanceName))
		{
			return DataLayerInstanceDesc;
		}
	}

	return nullptr;
}

const FDataLayerInstanceDesc* FDataLayerUtils::GetDataLayerInstanceDescFromAssetPath(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, const FName& DataLayerAssetPath)
{
	for (const FWorldDataLayersActorDesc* WorldDataLayerActorDesc : InWorldDataLayersActorDescs)
	{
		if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = WorldDataLayerActorDesc->GetDataLayerInstanceFromAssetPath(DataLayerAssetPath))
		{
			return DataLayerInstanceDesc;
		}
	}

	return nullptr;
}

bool FDataLayerUtils::FindWorldDataLayerActorDescs(const FActorDescViewMap& ActorDescViewMap, TArray<const FWorldDataLayersActorDesc*>& OutWorldDataLayersActorDescs)
{
	TArray<const FWorldPartitionActorDescView*> WorldDataLayerViews = ActorDescViewMap.FindByExactNativeClass<AWorldDataLayers>();
	Algo::Transform(WorldDataLayerViews, OutWorldDataLayersActorDescs, [](const FWorldPartitionActorDescView* WorldDataLayersActorDescView) { return  (FWorldDataLayersActorDesc*)WorldDataLayersActorDescView->GetActorDesc(); });
	return !OutWorldDataLayersActorDescs.IsEmpty();
}

bool FDataLayerUtils::AreWorldDataLayersActorDescsSane(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs)
{
	// Deprecation handling: Case where pre 5.1 map, without WorldDataLayersActorDescs, are resolving their data layer outside of their world (changelist validation). They need proper Actor descs to resolve.
	return Algo::AllOf(InWorldDataLayersActorDescs, [](const FWorldDataLayersActorDesc* WorldDataLayerActorDescs) { return WorldDataLayerActorDescs->IsValid(); });
}

#endif