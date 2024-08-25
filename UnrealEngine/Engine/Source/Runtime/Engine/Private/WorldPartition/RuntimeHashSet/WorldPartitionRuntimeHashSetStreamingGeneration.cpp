// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/HLOD/HLODLayer.h"

#if WITH_EDITOR
bool UWorldPartitionRuntimeHashSet::GenerateRuntimePartitionsStreamingDescs(const IStreamingGenerationContext* StreamingGenerationContext, TMap<URuntimePartition*, TArray<URuntimePartition::FCellDescInstance>>& OutRuntimeCellDescs) const
{
	UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	//
	// Split actor sets into their corresponding runtime partition implementation
	//
	TMap<FName, const FRuntimePartitionDesc*> NameToRuntimePartitionDescMap;

	// Actors with RuntimeGrid set to None will be assigned to the default partition
	NameToRuntimePartitionDescMap.Add(NAME_None, &RuntimePartitions[0]);

	// Non-spatially loaded actors will be assigned to the persistent partition
	if (PersistentPartitionDesc.Class)
	{
		NameToRuntimePartitionDescMap.Add(NAME_PersistentLevel, &PersistentPartitionDesc);
	}

	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		NameToRuntimePartitionDescMap.Add(RuntimePartitionDesc.Name, &RuntimePartitionDesc);
	}

	TMap<URuntimePartition*, TArray<const IStreamingGenerationContext::FActorSetInstance*>> RuntimePartitionsToActorSetMap;
	StreamingGenerationContext->ForEachActorSetInstance([this, &NameToRuntimePartitionDescMap, &RuntimePartitionsToActorSetMap](const IStreamingGenerationContext::FActorSetInstance& ActorSetInstance)
	{
		TArray<FName> MainPartitionTokens;
		TArray<FName> HLODPartitionTokens;

		if (!ActorSetInstance.bIsSpatiallyLoaded)
		{
			MainPartitionTokens.Add(NAME_PersistentLevel);
		}
		else
		{
			verify(ParseGridName(ActorSetInstance.RuntimeGrid, MainPartitionTokens, HLODPartitionTokens));
		}

		check(!MainPartitionTokens.IsEmpty());
		if (const FRuntimePartitionDesc** RuntimePartitionDesc = NameToRuntimePartitionDescMap.Find(MainPartitionTokens[0]))
		{
			if (!HLODPartitionTokens.IsEmpty())
			{
				bool bFoundHLODPartitionLayer = false;

				for (const FRuntimePartitionHLODSetup& HLODSetup : (*RuntimePartitionDesc)->HLODSetups)
				{
					for (const UHLODLayer* HLODPartitionLayer : HLODSetup.HLODLayers)
					{
						if (HLODPartitionLayer->GetName() == HLODPartitionTokens[0])
						{
							RuntimePartitionsToActorSetMap.FindOrAdd(HLODSetup.PartitionLayer).Add(&ActorSetInstance);
							bFoundHLODPartitionLayer = true;
							break;
						}
					}

					if (bFoundHLODPartitionLayer)
					{
						break;
					}
				}
			}
			else
			{
				RuntimePartitionsToActorSetMap.FindOrAdd((*RuntimePartitionDesc)->MainLayer).Add(&ActorSetInstance);
			}
		}
	});

	//
	// Generate runtime partitions streaming data
	//
	TMap<URuntimePartition*, TArray<URuntimePartition::FCellDesc>> RuntimePartitionsStreamingDescs;
	for (auto [RuntimePartition, ActorSetInstances] : RuntimePartitionsToActorSetMap)
	{
		// Gather runtime partition cell descs
		URuntimePartition::FGenerateStreamingParams GenerateStreamingParams;
		GenerateStreamingParams.ActorSetInstances = &ActorSetInstances;

		URuntimePartition::FGenerateStreamingResult GenerateStreamingResult;

		if (!RuntimePartition->GenerateStreaming(GenerateStreamingParams, GenerateStreamingResult))
		{
			return false;
		}

		RuntimePartitionsStreamingDescs.Add(RuntimePartition, GenerateStreamingResult.RuntimeCellDescs);
	}

	//
	// Generate runtime partitions streaming data
	//
	TSet<FName> CellDescsNames;
	for (auto& [RuntimePartition, RuntimeCellDescs] : RuntimePartitionsStreamingDescs)
	{
		for (const URuntimePartition::FCellDesc& RuntimeCellDesc : RuntimeCellDescs)
		{
			TMap<FDataLayersID, URuntimePartition::FCellDescInstance> RuntimeCellDescsInstancesSet;

			bool bCellNameExists;
			CellDescsNames.Add(RuntimeCellDesc.Name, &bCellNameExists);
			check(!bCellNameExists);

			// Split cell descs into data layers
			for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : RuntimeCellDesc.ActorSetInstances)
			{
				const FDataLayersID DataLayersID(ActorSetInstance->DataLayers);
				URuntimePartition::FCellDescInstance* CellDescInstance = RuntimeCellDescsInstancesSet.Find(DataLayersID);

				if (!CellDescInstance)
				{
					CellDescInstance = &RuntimeCellDescsInstancesSet.Emplace(DataLayersID, URuntimePartition::FCellDescInstance(RuntimeCellDesc, RuntimePartition, ActorSetInstance->DataLayers, ActorSetInstance->ContentBundleID));
					CellDescInstance->ActorSetInstances.Empty();
				}

				CellDescInstance->ActorSetInstances.Add(ActorSetInstance);
			}

			for (auto& [DataLayersID, CellDescInstance] : RuntimeCellDescsInstancesSet)
			{
				OutRuntimeCellDescs.FindOrAdd(RuntimePartition).Add(CellDescInstance);
			}
		}
	}

	return true;
}

bool UWorldPartitionRuntimeHashSet::GenerateStreaming(UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate)
{
	verify(Super::GenerateStreaming(StreamingPolicy, StreamingGenerationContext, OutPackagesToGenerate));

	UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	check(!PersistentPartitionDesc.Class);
	PersistentPartitionDesc.Class = URuntimePartitionPersistent::StaticClass();
	PersistentPartitionDesc.Name = NAME_PersistentLevel;
	PersistentPartitionDesc.MainLayer = NewObject<URuntimePartition>(this, URuntimePartitionPersistent::StaticClass(), NAME_None);
	PersistentPartitionDesc.MainLayer->Name = NAME_PersistentLevel;
	PersistentPartitionDesc.MainLayer->LoadingRange = 0;

	ON_SCOPE_EXIT
	{
		check(PersistentPartitionDesc.Class);
		PersistentPartitionDesc.Class = nullptr;
		PersistentPartitionDesc.Name = NAME_None;
		PersistentPartitionDesc.MainLayer = nullptr;
	};

	//
	// Generate runtime partitions streaming cell desccriptors
	//
	TMap<URuntimePartition*, TArray<URuntimePartition::FCellDescInstance>> RuntimePartitionsStreamingDescs;
	GenerateRuntimePartitionsStreamingDescs(StreamingGenerationContext, RuntimePartitionsStreamingDescs);

	//
	// Create and populate streaming object
	//

	// Generate runtime cells
	auto CreateRuntimeCellFromCellDesc = [this](const URuntimePartition::FCellDescInstance& CellDescInstance, TSubclassOf<UWorldPartitionRuntimeCell> CellClass, TSubclassOf<UWorldPartitionRuntimeCellData> CellDataClass)
	{
		const FCellUniqueId CellUniqueId = GetCellUniqueId(CellDescInstance);

		UWorldPartitionRuntimeCell* RuntimeCell = Super::CreateRuntimeCell(CellClass, CellDataClass, CellUniqueId.Name, TEXT(""));

		RuntimeCell->SetDataLayers(CellDescInstance.DataLayerInstances);
		RuntimeCell->SetContentBundleUID(CellDescInstance.ContentBundleID);
		RuntimeCell->SetClientOnlyVisible(CellDescInstance.bClientOnlyVisible);
		RuntimeCell->SetBlockOnSlowLoading(CellDescInstance.bBlockOnSlowStreaming);
		RuntimeCell->SetIsHLOD(CellDescInstance.SourcePartition->HLODIndex != INDEX_NONE);
		RuntimeCell->SetGuid(CellUniqueId.Guid);
		RuntimeCell->SetCellDebugColor(CellDescInstance.SourcePartition->DebugColor);

		UWorldPartitionRuntimeCellData* RuntimeCellData = RuntimeCell->RuntimeCellData;
		RuntimeCellData->DebugName = CellUniqueId.Name;
		RuntimeCellData->CellBounds = CellDescInstance.CellBounds;
		RuntimeCellData->HierarchicalLevel = CellDescInstance.Level;
		RuntimeCellData->Priority = CellDescInstance.Priority;
		RuntimeCellData->GridName = CellDescInstance.SourcePartition->Name;

		return RuntimeCell;
	};

	TArray<UWorldPartitionRuntimeCell*> RuntimeCells;
	TMap<URuntimePartition*, FRuntimePartitionStreamingData> RuntimePartitionsStreamingData;
	for (auto& [RuntimePartition, CellDescInstances] : RuntimePartitionsStreamingDescs)
	{
		for (URuntimePartition::FCellDescInstance& CellDescInstance : CellDescInstances)
		{
			const bool bIsCellAlwaysLoaded = !CellDescInstance.bIsSpatiallyLoaded && !CellDescInstance.DataLayerInstances.Num() && !CellDescInstance.ContentBundleID.IsValid();

			TArray<IStreamingGenerationContext::FActorInstance> CellActorInstances;
			if (PopulateCellActorInstances(CellDescInstance.ActorSetInstances, bIsMainWorldPartition, bIsCellAlwaysLoaded, CellActorInstances))
			{
				UWorldPartitionRuntimeCell* RuntimeCell = RuntimeCells.Emplace_GetRef(CreateRuntimeCellFromCellDesc(CellDescInstance, StreamingPolicy->GetRuntimeCellClass(), UWorldPartitionRuntimeCellData::StaticClass()));
				RuntimeCell->SetIsAlwaysLoaded(bIsCellAlwaysLoaded);
				PopulateRuntimeCell(RuntimeCell, CellActorInstances, OutPackagesToGenerate);

				// Override the cell bounds if the runtime partition provided one
				if (CellDescInstance.Bounds.IsValid)
				{
					RuntimeCell->RuntimeCellData->ContentBounds = CellDescInstance.Bounds;
				}

				// Create partition streaming data
				FRuntimePartitionStreamingData& StreamingData = RuntimePartitionsStreamingData.FindOrAdd(CellDescInstance.SourcePartition);

				StreamingData.Name = CellDescInstance.SourcePartition->Name;
				StreamingData.LoadingRange = CellDescInstance.SourcePartition->LoadingRange;

				if (CellDescInstance.bIsSpatiallyLoaded)
				{
					StreamingData.StreamingCells.Add(RuntimeCell);
				}
				else
				{
					StreamingData.NonStreamingCells.Add(RuntimeCell);
				}
			}
		}
	}

	//
	// Finalize streaming object
	//
	check(RuntimeStreamingData.IsEmpty());
	for (auto& [Partition, StreamingData] : RuntimePartitionsStreamingData)
	{
		StreamingData.CreatePartitionsSpatialIndex();
		RuntimeStreamingData.Emplace(MoveTemp(StreamingData));
	}

	return true;
}

void UWorldPartitionRuntimeHashSet::DumpStateLog(FHierarchicalLogArchive& Ar) const
{
	Super::DumpStateLog(Ar);

	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("%s - Runtime Hash Set"), *GetWorld()->GetName());
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));

	TArray<const UWorldPartitionRuntimeCell*> StreamingCells;
	ForEachStreamingCells([&StreamingCells](const UWorldPartitionRuntimeCell* StreamingCell) { if (!StreamingCell->IsAlwaysLoaded()) { StreamingCells.Add(StreamingCell); } return true; });
				
	StreamingCells.Sort([this](const UWorldPartitionRuntimeCell& A, const UWorldPartitionRuntimeCell& B) { return A.GetFName().LexicalLess(B.GetFName()); });

	for (const UWorldPartitionRuntimeCell* StreamingCell : StreamingCells)
	{
		FHierarchicalLogArchive::FIndentScope CellIndentScope = Ar.PrintfIndent(TEXT("Content of Cell %s (%s)"), *StreamingCell->GetDebugName(), *StreamingCell->GetName());
		StreamingCell->DumpStateLog(Ar);
	}

	Ar.Printf(TEXT(""));
}
#endif
