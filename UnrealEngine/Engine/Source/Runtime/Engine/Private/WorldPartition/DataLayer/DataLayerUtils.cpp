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

FDataLayerInstanceNames FDataLayerUtils::ResolveDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const FWorldPartitionActorDesc* InActorDesc, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs)
{
	const bool bIncludeExternalDataLayer = false;
	const FName ActorDescExternalDataLayer = InActorDesc->GetExternalDataLayer();
	const TArray<FName> ActorDescNonExternalDataLayers = InActorDesc->GetDataLayers(bIncludeExternalDataLayer);
	if (ActorDescNonExternalDataLayers.IsEmpty() && ActorDescExternalDataLayer.IsNone())
	{
		return FDataLayerInstanceNames();
	}
	
	// DataLayers not using DataLayer Assets represent DataLayerInstanceNames
	if (!InActorDesc->IsUsingDataLayerAsset())
	{
		if (InDataLayerManager && InDataLayerManager->CanResolveDataLayers())
		{
			TArray<FName> Result;
			for (const FName& DataLayerInstanceName : ActorDescNonExternalDataLayers)
			{
				if (const UDataLayerInstance* DataLayerInstance = InDataLayerManager->GetDataLayerInstance(DataLayerInstanceName))
				{
					Result.Add(DataLayerInstanceName);
				}
			}
			return FDataLayerInstanceNames(Result, false);
		}
		// Fallback on FWorldDataLayersActorDesc
		else if (!InWorldDataLayersActorDescs.IsEmpty() && AreWorldDataLayersActorDescsSane(InWorldDataLayersActorDescs))
		{
			TArray<FName> Result;
			for (const FName& DataLayerInstanceName : ActorDescNonExternalDataLayers)
			{
				if (GetDataLayerInstanceDescFromInstanceName(InWorldDataLayersActorDescs, DataLayerInstanceName) != nullptr)
				{
					Result.Add(DataLayerInstanceName);
				}
			}
			return FDataLayerInstanceNames(Result, false);
		}
	}
	// ActorDesc DataLayers represents DataLayer Asset Paths
	else
	{
		if (InDataLayerManager && InDataLayerManager->CanResolveDataLayers())
		{
			TArray<FName> ResolvedDLINames;
			ResolvedDLINames.Reserve(ActorDescNonExternalDataLayers.Num() + 1);
			const UDataLayerInstance* ExternalDataLayerInstance = InDataLayerManager->GetDataLayerInstanceFromAssetName(ActorDescExternalDataLayer);
			FName ExternalDataLayer = ExternalDataLayerInstance ? ExternalDataLayerInstance->GetDataLayerFName() : NAME_None;
			bool bIsFirstDataLayerExternal = !ExternalDataLayer.IsNone();
			if (bIsFirstDataLayerExternal)
			{
				ResolvedDLINames.Add(ExternalDataLayer);
			}
			for (const FName& DataLayerAssetPath : ActorDescNonExternalDataLayers)
			{
				if (const UDataLayerInstance* DataLayerInstance = InDataLayerManager->GetDataLayerInstanceFromAssetName(DataLayerAssetPath))
				{
					ResolvedDLINames.Add(DataLayerInstance->GetDataLayerFName());
				}
			}
			return FDataLayerInstanceNames(ResolvedDLINames, bIsFirstDataLayerExternal);
		}
		// Fallback on FWorldDataLayersActorDesc
		else if (!InWorldDataLayersActorDescs.IsEmpty() && AreWorldDataLayersActorDescsSane(InWorldDataLayersActorDescs))
		{
			TArray<FName> ResolvedDLINames;
			ResolvedDLINames.Reserve(ActorDescNonExternalDataLayers.Num() + 1);
			const FDataLayerInstanceDesc* ExternalDataLayerInstanceDesc = GetDataLayerInstanceDescFromAssetPath(InWorldDataLayersActorDescs, ActorDescExternalDataLayer);
			FName ExternalDataLayer = ExternalDataLayerInstanceDesc ? ExternalDataLayerInstanceDesc->GetName() : NAME_None;
			bool bIsFirstDataLayerExternal = !ExternalDataLayer.IsNone();
			if (bIsFirstDataLayerExternal)
			{
				ResolvedDLINames.Add(ExternalDataLayer);
			}
			for (const FName& DataLayerAssetPath : ActorDescNonExternalDataLayers)
			{
				if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = GetDataLayerInstanceDescFromAssetPath(InWorldDataLayersActorDescs, DataLayerAssetPath))
				{
					ResolvedDLINames.Add(DataLayerInstanceDesc->GetName());
				}
			}
			return FDataLayerInstanceNames(ResolvedDLINames, bIsFirstDataLayerExternal);
		}
	}
	return FDataLayerInstanceNames(ActorDescNonExternalDataLayers, ActorDescExternalDataLayer);
}

// For performance reasons, this function assumes that InActorDesc's DataLayerInstanceNames was already resolved.
bool FDataLayerUtils::ResolveRuntimeDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const IWorldPartitionActorDescInstanceView& InActorDescView, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, FDataLayerInstanceNames& OutRuntimeDataLayerInstanceNames)
{
	const FDataLayerInstanceNames& ActorDescViewDataLayerInstanceNames = InActorDescView.GetDataLayerInstanceNames();
	const FName EDLInstanceName = ActorDescViewDataLayerInstanceNames.GetExternalDataLayer();
	const TArrayView<const FName> NonEDLInstanceNames = ActorDescViewDataLayerInstanceNames.GetNonExternalDataLayers();

	if (InDataLayerManager && InDataLayerManager->CanResolveDataLayers())
	{
		TArray<FName> ResolvedRuntimeDLINames;
		ResolvedRuntimeDLINames.Reserve(NonEDLInstanceNames.Num() + 1);
		const UDataLayerInstance* ExternalDataLayerInstance = InDataLayerManager->GetDataLayerInstanceFromName(EDLInstanceName);
		const bool bIsFirstDataLayerExternal = (ExternalDataLayerInstance && ExternalDataLayerInstance->IsRuntime());
		if (bIsFirstDataLayerExternal)
		{
			ResolvedRuntimeDLINames.Add(EDLInstanceName);
		}
		for (FName DataLayerInstanceName : NonEDLInstanceNames)
		{
			const UDataLayerInstance* DataLayerInstance = InDataLayerManager->GetDataLayerInstanceFromName(DataLayerInstanceName);
			if (DataLayerInstance && DataLayerInstance->IsRuntime())
			{
				ResolvedRuntimeDLINames.Add(DataLayerInstanceName);
			}
		}
		OutRuntimeDataLayerInstanceNames = FDataLayerInstanceNames(ResolvedRuntimeDLINames, bIsFirstDataLayerExternal);
		return true;
	}

	// Fallback on FWorldDataLayersActorDesc
	const TArray<const FWorldDataLayersActorDesc*>& WorldDataLayersActorDescs = InWorldDataLayersActorDescs;
	if (WorldDataLayersActorDescs.Num())
	{
		check(AreWorldDataLayersActorDescsSane(WorldDataLayersActorDescs));

		TArray<FName> ResolvedRuntimeDLINames;
		ResolvedRuntimeDLINames.Reserve(NonEDLInstanceNames.Num() + 1);
		const FDataLayerInstanceDesc* ExternalDataLayerInstanceDesc = GetDataLayerInstanceDescFromInstanceName(WorldDataLayersActorDescs, EDLInstanceName);
		const bool bIsFirstDataLayerExternal = (ExternalDataLayerInstanceDesc && (ExternalDataLayerInstanceDesc->GetDataLayerType() == EDataLayerType::Runtime));
		if (bIsFirstDataLayerExternal)
		{
			ResolvedRuntimeDLINames.Add(EDLInstanceName);
		}

		for (FName DataLayerInstanceName : NonEDLInstanceNames)
		{
			const FDataLayerInstanceDesc* DataLayerInstanceDesc = GetDataLayerInstanceDescFromInstanceName(WorldDataLayersActorDescs, DataLayerInstanceName);
			if (DataLayerInstanceDesc && (DataLayerInstanceDesc->GetDataLayerType() == EDataLayerType::Runtime))
			{
				ResolvedRuntimeDLINames.Add(DataLayerInstanceName);
			}
		}
		OutRuntimeDataLayerInstanceNames = FDataLayerInstanceNames(ResolvedRuntimeDLINames, bIsFirstDataLayerExternal);
		return true;
	}

	return false;
}

const FDataLayerInstanceDesc* FDataLayerUtils::GetDataLayerInstanceDescFromInstanceName(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, const FName& InDataLayerInstanceName)
{
	for (const FWorldDataLayersActorDesc* WorldDataLayerActorDesc : InWorldDataLayersActorDescs)
	{
		if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = WorldDataLayerActorDesc->GetDataLayerInstanceFromInstanceName(InDataLayerInstanceName))
		{
			return DataLayerInstanceDesc;
		}
	}

	return nullptr;
}

const FDataLayerInstanceDesc* FDataLayerUtils::GetDataLayerInstanceDescFromAssetPath(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, const FName& InDataLayerAssetPath)
{
	for (const FWorldDataLayersActorDesc* WorldDataLayerActorDesc : InWorldDataLayersActorDescs)
	{
		if (const FDataLayerInstanceDesc* DataLayerInstanceDesc = WorldDataLayerActorDesc->GetDataLayerInstanceFromAssetPath(InDataLayerAssetPath))
		{
			return DataLayerInstanceDesc;
		}
	}

	return nullptr;
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