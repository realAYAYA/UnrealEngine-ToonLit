// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerUtils.h"

#if WITH_EDITOR
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "Algo/AllOf.h"

TArray<FName> FDataLayerUtils::ResolvedDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const FWorldPartitionActorDesc* InActorDesc, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs)
{
	const TArray<FName>& ActorDescDataLayers = InActorDesc->GetDataLayers();

	if (!ActorDescDataLayers.IsEmpty())
	{
		// DataLayers not using DataLayer Assets represent DataLayerInstanceNames
		if (!InActorDesc->IsUsingDataLayerAsset())
		{
			if (InDataLayerManager && InDataLayerManager->CanResolveDataLayers())
			{
				TArray<FName> Result;
				for (const FName& DataLayerInstanceName : ActorDescDataLayers)
				{
					if (const UDataLayerInstance* DataLayerInstance = InDataLayerManager->GetDataLayerInstance(DataLayerInstanceName))
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
				for (const FName& DataLayerInstanceName : ActorDescDataLayers)
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
			if (InDataLayerManager && InDataLayerManager->CanResolveDataLayers())
			{
				TArray<FName> Result;
				for (const FName& DataLayerAssetPath : ActorDescDataLayers)
				{
					if (const UDataLayerInstance* DataLayerInstance = InDataLayerManager->GetDataLayerInstanceFromAssetName(DataLayerAssetPath))
					{
						Result.Add(DataLayerInstance->GetDataLayerFName());
					}
				}
				return Result;
			}
			// Fallback on FWorldDataLayersActorDesc
			else if (!InWorldDataLayersActorDescs.IsEmpty() && AreWorldDataLayersActorDescsSane(InWorldDataLayersActorDescs))
			{
				TArray<FName> Result;
				for (const FName& DataLayerAssetPath : ActorDescDataLayers)
				{
					if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = GetDataLayerInstanceDescFromAssetPath(InWorldDataLayersActorDescs, DataLayerAssetPath))
					{
						Result.Add(DataLayerInstanceDesc->GetName());
					}
				}
				return Result;
			}
		}
	}
	return InActorDesc->GetDataLayers();
}

// For performance reasons, this function assumes that InActorDesc's DataLayerInstanceNames was already resolved.
bool FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const FWorldPartitionActorDescView& InActorDescView, const FActorDescViewMap& ActorDescViewMap, TArray<FName>& OutRuntimeDataLayerInstanceNames)
{	
	if (InDataLayerManager && InDataLayerManager->CanResolveDataLayers())
	{
		for (FName DataLayerInstanceName : InActorDescView.GetDataLayerInstanceNames())
		{
			const UDataLayerInstance* DataLayerInstance = InDataLayerManager->GetDataLayerInstanceFromName(DataLayerInstanceName);
			if (DataLayerInstance && DataLayerInstance->IsRuntime())
			{
				OutRuntimeDataLayerInstanceNames.Add(DataLayerInstanceName);
			}
		}

		return true;
	}
	else
	{
		TArray<const FWorldDataLayersActorDesc*> WorldDataLayersActorDescs = FindWorldDataLayerActorDescs(ActorDescViewMap);

		// Fallback on FWorldDataLayersActorDesc
		if (WorldDataLayersActorDescs.Num())
		{
			check(AreWorldDataLayersActorDescsSane(WorldDataLayersActorDescs));
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

TArray<const FWorldDataLayersActorDesc*> FDataLayerUtils::FindWorldDataLayerActorDescs(const FActorDescViewMap& ActorDescViewMap)
{
	TArray<const FWorldDataLayersActorDesc*> WorldDataLayersActorDescs;
	TArray<const FWorldPartitionActorDescView*> WorldDataLayerViews = ActorDescViewMap.FindByExactNativeClass<AWorldDataLayers>();
	Algo::TransformIf(WorldDataLayerViews, WorldDataLayersActorDescs, 
		[](const FWorldPartitionActorDescView* WorldDataLayersActorDescView) { return ((FWorldDataLayersActorDesc*)WorldDataLayersActorDescView->GetActorDesc())->IsValid(); },
		[](const FWorldPartitionActorDescView* WorldDataLayersActorDescView) { return (FWorldDataLayersActorDesc*)WorldDataLayersActorDescView->GetActorDesc(); });
	return WorldDataLayersActorDescs;
}

bool FDataLayerUtils::AreWorldDataLayersActorDescsSane(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs)
{
	// Deprecation handling: Case where pre 5.1 map, without WorldDataLayersActorDescs, are resolving their data layer outside of their world (changelist validation). They need proper Actor descs to resolve.
	return Algo::AllOf(InWorldDataLayersActorDescs, [](const FWorldDataLayersActorDesc* WorldDataLayerActorDescs) { return WorldDataLayerActorDescs->IsValid(); });
}

bool FDataLayerUtils::SetDataLayerShortName(UDataLayerInstance* InDataLayerInstance, const FString& InNewShortName)
{
	check(InDataLayerInstance->CanEditDataLayerShortName());
	FString UniqueShortName = FDataLayerUtils::GenerateUniqueDataLayerShortName(UDataLayerManager::GetDataLayerManager(InDataLayerInstance), InNewShortName);
	if (InDataLayerInstance->GetDataLayerShortName() != UniqueShortName)
	{
		InDataLayerInstance->Modify();
		InDataLayerInstance->PerformSetDataLayerShortName(UniqueShortName);
		return true;
	}

	return false;
}

bool FDataLayerUtils::FindDataLayerByShortName(const UDataLayerManager* InDataLayerManager, const FString& InShortName, TSet<UDataLayerInstance*>&  OutDataLayersWithShortName)
{
	OutDataLayersWithShortName.Empty();
	InDataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance->GetDataLayerShortName() == InShortName)
		{
			OutDataLayersWithShortName.Add(DataLayerInstance);
		}
		return true;
	});
	return OutDataLayersWithShortName.Num() > 0;
}

FString FDataLayerUtils::GenerateUniqueDataLayerShortName(const UDataLayerManager* InDataLayerManager, const FString& InNewShortName)
{
	int32 DataLayerIndex = 0;
	const FString DataLayerShortNameSanitized = FDataLayerUtils::GetSanitizedDataLayerShortName(InNewShortName);
	FString UniqueNewDataLayerShortName = DataLayerShortNameSanitized;
		
	TSet<UDataLayerInstance*> OutDataLayersWithShortName;
	while (FindDataLayerByShortName(InDataLayerManager, UniqueNewDataLayerShortName, OutDataLayersWithShortName))
	{
		UniqueNewDataLayerShortName = FString::Printf(TEXT("%s%d"), *DataLayerShortNameSanitized, ++DataLayerIndex);
	}
		
	return UniqueNewDataLayerShortName;
}

#endif
