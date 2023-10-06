// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Misc/ArchiveMD5.h"

/**
 *	TODOs:
 *		- Deprecate WorldPartitionStreamingSourceComponent.TargetGrids and make it more generic to handle all sorts of filtering.
 *		- Deprecate Actor.RuntimeGrid and introduce TargetPartition + HLOD setup index.
 *		- Refactor Source.ForEachShape to extract target grid and maybe loading range.
 */

FAutoConsoleCommand WorldPartitionRuntimeHashSetEnable(
	TEXT("wp.Editor.WorldPartitionRuntimeHashSet.Enable"),
	TEXT("Enable experimental runtime hash set class."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (UClass* WorldPartitionRuntimeHashSetClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.WorldPartitionRuntimeHashSet")))
		{
			WorldPartitionRuntimeHashSetClass->ClassFlags &= ~CLASS_HideDropDown;
		}
	})
);

#if WITH_EDITOR
void FRuntimePartitionDesc::UpdateHLODPartitionLayers()
{
	if (!Class || !MainLayer || !HLODLayer)
	{
		HLODSetups.Empty();
	}
	else
	{
		TSet<const UHLODLayer*> VisitedHLODLayers;

		const UHLODLayer* CurHLODLayer = HLODLayer;
		while (CurHLODLayer)
		{
			const int32 HLODSetupIndex = VisitedHLODLayers.Num();

			bool bHLODLayerWasAlreadyInSet;
			VisitedHLODLayers.Add(CurHLODLayer, &bHLODLayerWasAlreadyInSet);
			if (bHLODLayerWasAlreadyInSet)
			{
				break;
			}

			if (!HLODSetups.IsValidIndex(HLODSetupIndex))
			{
				HLODSetups.AddDefaulted();
			}

			FRuntimePartitionHLODSetup& HLODSetup = HLODSetups[HLODSetupIndex];

			const bool bHLODLayerMatches = HLODSetup.HLODLayer == CurHLODLayer;
			const UClass* ExpectedHLODPartitionClass = CurHLODLayer->IsSpatiallyLoaded() ? MainLayer->GetClass() : URuntimePartitionPersistent::StaticClass();
			const bool bHasValidPartitionLayer = HLODSetup.PartitionLayer && (HLODSetup.PartitionLayer->GetClass() == ExpectedHLODPartitionClass);
			
			if (!bHLODLayerMatches || !bHasValidPartitionLayer)
			{
				HLODSetup.HLODLayer = CurHLODLayer;
				HLODSetup.PartitionLayer = CurHLODLayer->IsSpatiallyLoaded() ? DuplicateObject<URuntimePartition>(MainLayer, MainLayer->GetOuter()) : NewObject<URuntimePartition>(MainLayer->GetOuter(), ExpectedHLODPartitionClass);
				HLODSetup.PartitionLayer->Name = CurHLODLayer->GetFName();
				HLODSetup.PartitionLayer->bIsHLODSetup = true;
			}

			CurHLODLayer = CurHLODLayer->GetParentLayer();
		}

		HLODSetups.SetNum(VisitedHLODLayers.Num());
	}
}
#endif

UWorldPartitionRuntimeHashSet::UWorldPartitionRuntimeHashSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UWorldPartitionRuntimeHashSet::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		bool bSpatialIndexValid = SpatialIndex.IsValid();
		Ar << bSpatialIndexValid;

		if (bSpatialIndexValid)
		{
			if (Ar.IsLoading())
			{
				check(!SpatialIndex);
				SpatialIndex = MakeUnique<FStaticSpatialIndexType>();
			}

			SpatialIndex->Serialize(Ar);
		}
	}
#endif
}

void UWorldPartitionRuntimeHashSet::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!GetTypedOuter<UWorld>()->IsGameWorld())
	{
		UpdateHLODPartitionLayers();
	}
#endif
}

void UWorldPartitionRuntimeHashSet::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
#if WITH_EDITOR
	UWorldPartitionRuntimeHashSet* This = CastChecked<UWorldPartitionRuntimeHashSet>(InThis);
	if (This->SpatialIndex)
	{
		This->SpatialIndex->AddReferencedObjects(Collector);
	}
	Collector.AddStableReferenceSet(&This->InjectedExternalStreamingObjects);
#endif

	Super::AddReferencedObjects(InThis, Collector);
}

#if WITH_EDITOR
void UWorldPartitionRuntimeHashSet::SetDefaultValues()
{
}

bool UWorldPartitionRuntimeHashSet::SupportsHLODs() const
{
	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		if (RuntimePartitionDesc.MainLayer)
		{
			if (RuntimePartitionDesc.MainLayer->SupportsHLODs())
			{
				return true;
			}
		}
	}

	return false;
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
	PersistentPartitionDesc.MainLayer->Name = TEXT("MainPartition");

	//
	// Split actor sets into their corresponding runtime partition implementation
	//
	TMap<FName, const FRuntimePartitionDesc*> NameToRuntimePartitionDescMap;

	NameToRuntimePartitionDescMap.Add(NAME_None, &RuntimePartitions[0]);				// Actors with RuntimeGrid=None will be assigned to the default partition
	NameToRuntimePartitionDescMap.Add(NAME_PersistentLevel, &PersistentPartitionDesc);	// Non-spatially loaded actors will be assigned to the persistent partition

	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		NameToRuntimePartitionDescMap.Add(RuntimePartitionDesc.Name, &RuntimePartitionDesc);
	}

	TMap<URuntimePartition*, TArray<const IStreamingGenerationContext::FActorSetInstance*>> RuntimePartitionsToActorSetMap;
	StreamingGenerationContext->ForEachActorSetInstance([this, &NameToRuntimePartitionDescMap, &RuntimePartitionsToActorSetMap](const IStreamingGenerationContext::FActorSetInstance& ActorSetInstance)
	{
		const TArray<FName> ActorSetRuntimeGrid = ActorSetInstance.bIsSpatiallyLoaded ? ParseGridName(ActorSetInstance.RuntimeGrid) : TArray<FName>({ NAME_PersistentLevel });

		if (const FRuntimePartitionDesc** RuntimePartitionDesc = NameToRuntimePartitionDescMap.Find(ActorSetRuntimeGrid[0]))
		{
			RuntimePartitionsToActorSetMap.FindOrAdd((*RuntimePartitionDesc)->MainLayer).Add(&ActorSetInstance);
		}
	});

	// Generate per data layer instance of partition cells
	struct FCellDescInstance : public URuntimePartition::FCellDesc
	{
		FBox Bounds;
		TArray<const UDataLayerInstance*> DataLayers;
	};

	TArray<FCellDescInstance> RuntimeCellDescsDataLayers;
	{
		TArray<URuntimePartition::FCellDesc> RuntimeCellDescs;
		for (auto [RuntimePartition, ActorSetInstances] : RuntimePartitionsToActorSetMap)
		{
			if (!RuntimePartition->GenerateStreaming(ActorSetInstances, RuntimeCellDescs))
			{
				return false;
			}
		}

		// Split cell descs into data layers	
		for (const URuntimePartition::FCellDesc& RuntimeCellDesc : RuntimeCellDescs)
		{
			TMap<FDataLayersID, FCellDescInstance> RuntimeCellDescsDataLayersSet;

			for (const IStreamingGenerationContext::FActorInstance& ActorInstance : RuntimeCellDesc.ActorInstances)
			{
				const FDataLayersID DataLayersID(ActorInstance.ActorSetInstance->DataLayers);

				FCellDescInstance* DataLayerCellDesc = RuntimeCellDescsDataLayersSet.Find(DataLayersID);
			
				if (!DataLayerCellDesc)
				{
					DataLayerCellDesc = &RuntimeCellDescsDataLayersSet.Emplace(DataLayersID);

					DataLayerCellDesc->Name = RuntimeCellDesc.Name;
					DataLayerCellDesc->bIsSpatiallyLoaded = RuntimeCellDesc.bIsSpatiallyLoaded;
					DataLayerCellDesc->ContentBundleID = RuntimeCellDesc.ContentBundleID;
					DataLayerCellDesc->bBlockOnSlowStreaming = RuntimeCellDesc.bBlockOnSlowStreaming;
					DataLayerCellDesc->bClientOnlyVisible = RuntimeCellDesc.bClientOnlyVisible;
					DataLayerCellDesc->Priority = RuntimeCellDesc.Priority;
					DataLayerCellDesc->Bounds = RuntimeCellDesc.Bounds;
					DataLayerCellDesc->DataLayers = ActorInstance.ActorSetInstance->DataLayers;
				}

				DataLayerCellDesc->ActorInstances.Add(ActorInstance);
			}

			Algo::Transform(RuntimeCellDescsDataLayersSet, RuntimeCellDescsDataLayers, [](const auto& Value) { return Value.Value; });
		}
	}

	// Generate runtime cells
	auto CreateRuntimeCellFromCellDesc = [this](const URuntimePartition::FCellDesc& CellDesc, const TArray<const UDataLayerInstance*>& DataLayers, TSubclassOf<UWorldPartitionRuntimeCell> CellClass, TSubclassOf<UWorldPartitionRuntimeCellData> CellDataClass)
	{
		FString CellObjectName;
		FGuid CellGuid;
		{
			UWorld* OuterWorld = GetTypedOuter<UWorld>();
			check(OuterWorld);

			FString WorldName = FPackageName::GetShortName(OuterWorld->GetPackage());

			CellObjectName = FString::Printf(TEXT("%s_%s"), *WorldName, *CellDesc.Name.ToString());

			const FDataLayersID DataLayersID(DataLayers);
			if (DataLayersID.GetHash())
			{
				CellObjectName += FString::Printf(TEXT("_d%X"), DataLayersID.GetHash());
			}

			if (CellDesc.ContentBundleID.IsValid())
			{
				CellObjectName += FString::Printf(TEXT("_c%s"), *UContentBundleDescriptor::GetContentBundleCompactString(CellDesc.ContentBundleID));
			}

			if (!IsRunningCookCommandlet() && OuterWorld->IsGameWorld())
			{
				FString SourceWorldPath;
				FString InstancedWorldPath;
				if (OuterWorld->GetSoftObjectPathMapping(SourceWorldPath, InstancedWorldPath))
				{
					const FTopLevelAssetPath SourceAssetPath(SourceWorldPath);
					WorldName = FPackageName::GetShortName(SourceAssetPath.GetPackageName());
						
					InstancedWorldPath = UWorld::RemovePIEPrefix(InstancedWorldPath);

					const FString SourcePackageName = SourceAssetPath.GetPackageName().ToString();
					const FTopLevelAssetPath InstanceAssetPath(InstancedWorldPath);
					const FString InstancePackageName = InstanceAssetPath.GetPackageName().ToString();

					if (int32 Index = InstancePackageName.Find(SourcePackageName); Index != INDEX_NONE)
					{
						CellObjectName += FString::Printf(TEXT("_i%s"), *InstancePackageName.Mid(Index + SourcePackageName.Len()));
					}
				}
			}

			FArchiveMD5 ArMD5;
			ArMD5 << CellObjectName;
			CellGuid = ArMD5.GetGuidFromHash();
			check(CellGuid.IsValid());
		}

		UWorldPartitionRuntimeCell* RuntimeCell = Super::CreateRuntimeCell(CellClass, CellDataClass, CellObjectName, TEXT(""), this);

		RuntimeCell->SetIsAlwaysLoaded(!CellDesc.bIsSpatiallyLoaded);
		RuntimeCell->SetDataLayers(DataLayers);
		RuntimeCell->SetContentBundleUID(CellDesc.ContentBundleID);
		RuntimeCell->SetPriority(CellDesc.Priority);
		RuntimeCell->SetClientOnlyVisible(CellDesc.bClientOnlyVisible);
		RuntimeCell->SetBlockOnSlowLoading(CellDesc.bBlockOnSlowStreaming);
		RuntimeCell->SetIsHLOD(false);
		RuntimeCell->SetGuid(CellGuid);
		RuntimeCell->RuntimeCellData->DebugName = CellObjectName;

		return RuntimeCell;
	};

	TArray<UWorldPartitionRuntimeCell*> RuntimeCells;	
	for (const FCellDescInstance& CellDescInstance : RuntimeCellDescsDataLayers)
	{
		UWorldPartitionRuntimeCell* RuntimeCell = RuntimeCells.Emplace_GetRef(CreateRuntimeCellFromCellDesc(CellDescInstance, CellDescInstance.DataLayers, StreamingPolicy->GetRuntimeCellClass(), UWorldPartitionRuntimeCellData::StaticClass()));
		PopulateRuntimeCell(RuntimeCell, CellDescInstance.ActorInstances, nullptr);

		// Override the cell bounds if the runtime partition provided one
		if (CellDescInstance.Bounds.IsValid)
		{
			RuntimeCell->RuntimeCellData->ContentBounds = CellDescInstance.Bounds;
		}
	}

	if (OutPackagesToGenerate)
	{
		for (UWorldPartitionRuntimeCell* RuntimeCell : RuntimeCells)
		{
			// Always loaded cell actors are transfered to World's Persistent Level (see UWorldPartitionRuntimeSpatialHash::PopulateGeneratorPackageForCook)
			if (RuntimeCell->GetActorCount() && !RuntimeCell->IsAlwaysLoaded())
			{
				const FString PackageRelativePath = RuntimeCell->GetPackageNameToCreate();
				check(!PackageRelativePath.IsEmpty());

				OutPackagesToGenerate->Add(PackageRelativePath);

				// Map relative package to StreamingCell for PopulateGeneratedPackageForCook/PopulateGeneratorPackageForCook/GetCellForPackage
				PackagesToGenerateForCook.Add(PackageRelativePath, RuntimeCell);
			}
		}
	}

	// Init spatial index
	check(!SpatialIndex);
	SpatialIndex = MakeUnique<FStaticSpatialIndexType>();

	TSet<FGuid> RuntimeCellsGuids;
	RuntimeCellsGuids.Reserve(RuntimeCells.Num());

	TArray<TPair<FBox, UWorldPartitionRuntimeCell*>> Elements;
	Algo::ForEach(RuntimeCells, [this, &Elements, &RuntimeCellsGuids](UWorldPartitionRuntimeCell* RuntimeCell)
	{
		bool bCellWasAlreadyInSet;
		RuntimeCellsGuids.Add(RuntimeCell->GetGuid(), &bCellWasAlreadyInSet);
		check(!bCellWasAlreadyInSet);

		if (RuntimeCell->IsAlwaysLoaded())
		{
			NonSpatiallyLoadedRuntimeCells.Add(RuntimeCell);
		}
		else
		{
			Elements.Add(TPair<FBox, UWorldPartitionRuntimeCell*>(RuntimeCell->GetContentBounds(), RuntimeCell));
		}
	});

	SpatialIndex->Init(Elements);

	return true;
}

void UWorldPartitionRuntimeHashSet::FlushStreaming()
{
	Super::FlushStreaming();
	
	check(PersistentPartitionDesc.Class);
	PersistentPartitionDesc.Class = nullptr;
	PersistentPartitionDesc.Name = NAME_None;
	PersistentPartitionDesc.MainLayer = nullptr;

	NonSpatiallyLoadedRuntimeCells.Empty();	
	SpatialIndex.Reset();
}

bool UWorldPartitionRuntimeHashSet::IsValidGrid(FName GridName) const
{
	// The None grid name will always map to the first runtime partition in the list
	if (GridName.IsNone())
	{
		return true;
	}

	// Parse the potentially dot separated grid name to identiy the associated runtime partition
	const TArray<FName> GridNameList = ParseGridName(GridName);
	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		if (RuntimePartitionDesc.Name == GridNameList[0])
		{
			if (RuntimePartitionDesc.MainLayer)
			{
				return RuntimePartitionDesc.MainLayer->IsValidGrid(GridName);
			}
		}
	}

	return false;
}

TArray<UWorldPartitionRuntimeCell*> UWorldPartitionRuntimeHashSet::GetAlwaysLoadedCells() const
{
	return NonSpatiallyLoadedRuntimeCells;
}

void UWorldPartitionRuntimeHashSet::DumpStateLog(FHierarchicalLogArchive& Ar) const
{
	Super::DumpStateLog(Ar);

	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("%s - Runtime Hash Set"), *GetWorld()->GetName());
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));

	TArray<const UWorldPartitionRuntimeCell*> StreamingCells;
	ForEachStreamingCells([&StreamingCells](const UWorldPartitionRuntimeCell* StreamingCell) { StreamingCells.Add(StreamingCell); return true; });
				
	StreamingCells.Sort([this](const UWorldPartitionRuntimeCell& A, const UWorldPartitionRuntimeCell& B) { return A.GetFName().LexicalLess(B.GetFName()); });

	for (const UWorldPartitionRuntimeCell* StreamingCell : StreamingCells)
	{
		FHierarchicalLogArchive::FIndentScope CellIndentScope = Ar.PrintfIndent(TEXT("Content of Cell %s (%s)"), *StreamingCell->GetDebugName(), *StreamingCell->GetName());
		StreamingCell->DumpStateLog(Ar);
	}

	Ar.Printf(TEXT(""));
}

TArray<FName> UWorldPartitionRuntimeHashSet::ParseGridName(FName GridName)
{
	TArray<FString> GridNameList;
	const FString GridNameStr = GridName.ToString();
	if (GridNameStr.ParseIntoArray(GridNameList, TEXT(".")))
	{
		TArray<FName> Result;
		Algo::Transform(GridNameList, Result, [](const FString& GridName) { return *GridName; });
		return MoveTemp(Result);
	}
	return { GridName };
}

URuntimeHashExternalStreamingObjectBase* UWorldPartitionRuntimeHashSet::StoreToExternalStreamingObject(UObject* StreamingObjectOuter, FName StreamingObjectName)
{
	URuntimeHashSetExternalStreamingObject* StreamingObject = CreateExternalStreamingObject<URuntimeHashSetExternalStreamingObject>(StreamingObjectOuter, StreamingObjectName);
	StreamingObject->NonSpatiallyLoadedRuntimeCells = NonSpatiallyLoadedRuntimeCells;
	SpatialIndex->ForEachElement([StreamingObject](UWorldPartitionRuntimeCell* RuntimeCell) { StreamingObject->SpatiallyLoadedRuntimeCells.Add(RuntimeCell); });
	return StreamingObject;
}
#endif

bool UWorldPartitionRuntimeHashSet::InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	URuntimeHashSetExternalStreamingObject* HashSetExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(ExternalStreamingObject);

	bool bWasAlreadyInSet;
	InjectedExternalStreamingObjects.Add(HashSetExternalStreamingObject, &bWasAlreadyInSet);
	check(!bWasAlreadyInSet);

	check(!HashSetExternalStreamingObject->SpatialIndex);
	HashSetExternalStreamingObject->SpatialIndex = MakeUnique<FStaticSpatialIndexType>();

	TArray<TPair<FBox, UWorldPartitionRuntimeCell*>> Elements;
	Algo::Transform(HashSetExternalStreamingObject->SpatiallyLoadedRuntimeCells, Elements, 
		[](UWorldPartitionRuntimeCell* RuntimeCell) { return TPair<FBox, UWorldPartitionRuntimeCell*>(RuntimeCell->GetContentBounds(), RuntimeCell); }
	);	
	HashSetExternalStreamingObject->SpatialIndex->Init(Elements);

	return true;
}

bool UWorldPartitionRuntimeHashSet::RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	URuntimeHashSetExternalStreamingObject* HashSetExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(ExternalStreamingObject);

	check(HashSetExternalStreamingObject->SpatialIndex);
	HashSetExternalStreamingObject->SpatialIndex.Reset();

	verify(InjectedExternalStreamingObjects.Remove(HashSetExternalStreamingObject));

	return true;
}

// Streaming interface
void UWorldPartitionRuntimeHashSet::ForEachStreamingCells(TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const
{
	auto ForEachStreamingCells = [this, &Func](FStaticSpatialIndexType* InSpatialIndex)
	{
		if (InSpatialIndex)
		{
			InSpatialIndex->ForEachElement([this, &Func](const UWorldPartitionRuntimeCell* RuntimeCell)
			{
				if (IsCellRelevantFor(RuntimeCell->GetClientOnlyVisible()))
				{
					Func(RuntimeCell);
				}
			});
		}
	};

	auto ForEachNonStreamingCells = [this, &Func](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonSpatiallyLoadedRuntimeCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonSpatiallyLoadedRuntimeCells)
		{
			if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
			{
				Func(Cell);
			}
		}
	};

	ForEachStreamingCells(SpatialIndex.Get());
	ForEachNonStreamingCells(NonSpatiallyLoadedRuntimeCells);

	for (URuntimeHashSetExternalStreamingObject* InjectedExternalStreamingObject : InjectedExternalStreamingObjects)
	{
		ForEachStreamingCells(InjectedExternalStreamingObject->SpatialIndex.Get());
		ForEachNonStreamingCells(InjectedExternalStreamingObject->NonSpatiallyLoadedRuntimeCells);
	}
}

void UWorldPartitionRuntimeHashSet::ForEachStreamingCellsQuery(const FWorldPartitionStreamingQuerySource& QuerySource, TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func, FWorldPartitionQueryCache* QueryCache) const
{
	auto ShouldAddCell = [this](const UWorldPartitionRuntimeCell* Cell, const FWorldPartitionStreamingQuerySource& QuerySource)
	{
		if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
		{
			if (Cell->HasDataLayers())
			{
				if (Cell->GetDataLayers().FindByPredicate([&](const FName& DataLayerName) { return QuerySource.DataLayers.Contains(DataLayerName); }))
				{
					return true;
				}
			}
			else if (!QuerySource.bDataLayersOnly)
			{
				return true;
			}
		}

		return false;
	};

	auto ForEachStreamingCells = [&ShouldAddCell, &QuerySource, &Func](FStaticSpatialIndexType* InSpatialIndex)
	{
		if (InSpatialIndex)
		{
			InSpatialIndex->ForEachElement([&ShouldAddCell, &QuerySource, &Func](UWorldPartitionRuntimeCell* RuntimeCell)
			{
				if (ShouldAddCell(RuntimeCell, QuerySource))
				{
					Func(RuntimeCell);
				}
			});
		}
	};

	auto ForEachNonStreamingCells = [&ShouldAddCell, &QuerySource, &Func](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonSpatiallyLoadedRuntimeCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonSpatiallyLoadedRuntimeCells)
		{
			if (ShouldAddCell(Cell, QuerySource))
			{
				Func(Cell);
			}
		}
	};

	ForEachStreamingCells(SpatialIndex.Get());
	ForEachNonStreamingCells(NonSpatiallyLoadedRuntimeCells);

	for (URuntimeHashSetExternalStreamingObject* InjectedExternalStreamingObject : InjectedExternalStreamingObjects)
	{
		ForEachStreamingCells(InjectedExternalStreamingObject->SpatialIndex.Get());
		ForEachNonStreamingCells(InjectedExternalStreamingObject->NonSpatiallyLoadedRuntimeCells);
	}
}

void UWorldPartitionRuntimeHashSet::ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const
{
	UWorldPartitionRuntimeHash::FStreamingSourceCells ActivateStreamingSourceCells;
	UWorldPartitionRuntimeHash::FStreamingSourceCells LoadStreamingSourceCells;

	auto ForEachStreamingCells = [this, &Sources, &Func, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](FStaticSpatialIndexType* InSpatialIndex)
	{
		if (InSpatialIndex)
		{
			for (const FWorldPartitionStreamingSource& Source : Sources)
			{
				// @todo_jfd
				const FName GridName;
				const FSoftObjectPath HLODLayer;

				Source.ForEachShape(/*LoadingRange*/25600, GridName, HLODLayer, false, [this, &Source, InSpatialIndex, &Func, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](const FSphericalSector& Shape)
				{
					const FSphere ShapeSphere(Shape.GetCenter(), Shape.GetRadius());

					InSpatialIndex->ForEachIntersectingElement(ShapeSphere, [this, &Source, &Shape, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](UWorldPartitionRuntimeCell* Cell)
					{
						if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
						{
							if (!Cell->HasDataLayers() || Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Activated))
							{
								if (Source.TargetState == EStreamingSourceTargetState::Loaded)
								{
									LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
								}
								else
								{
									ActivateStreamingSourceCells.AddCell(Cell, Source, Shape);
								}
							}
							else if (Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Loaded))
							{
								LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
							}
						}
					});
				});
			}
		}
	};

	auto ForEachNonStreamingCells = [this, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonSpatiallyLoadedRuntimeCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonSpatiallyLoadedRuntimeCells)
		{
			if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
			{
				if (!Cell->HasDataLayers() || Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Activated))
				{
					ActivateStreamingSourceCells.GetCells().Add(Cell);
				}
				else if (Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Loaded))
				{
					LoadStreamingSourceCells.GetCells().Add(Cell);
				}
			}
		}
	};

	ForEachStreamingCells(SpatialIndex.Get());
	ForEachNonStreamingCells(NonSpatiallyLoadedRuntimeCells);

	for (URuntimeHashSetExternalStreamingObject* InjectedExternalStreamingObject : InjectedExternalStreamingObjects)
	{
		ForEachStreamingCells(InjectedExternalStreamingObject->SpatialIndex.Get());
		ForEachNonStreamingCells(InjectedExternalStreamingObject->NonSpatiallyLoadedRuntimeCells);
	}

	auto ExecuteFuncOnCells = [Func](const TSet<const UWorldPartitionRuntimeCell*>& Cells, EStreamingSourceTargetState TargetState)
	{
		for (const UWorldPartitionRuntimeCell* Cell : Cells)
		{
			Func(Cell, TargetState);
		}
	};

	ExecuteFuncOnCells(ActivateStreamingSourceCells.GetCells(), EStreamingSourceTargetState::Activated);
	ExecuteFuncOnCells(LoadStreamingSourceCells.GetCells(), EStreamingSourceTargetState::Loaded);
}

#if WITH_EDITOR
void UWorldPartitionRuntimeHashSet::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	static FName NAME_RuntimePartitions(TEXT("RuntimePartitions"));
	static FName NAME_HLODSetups_Key(TEXT("HLODSetups_Key"));
	static FName NAME_HLODLayer(TEXT("HLODLayer"));

	FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionDesc, Class))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];
		
		RuntimePartitionDesc.MainLayer = nullptr;

		if (RuntimePartitionDesc.Class)
		{
			RuntimePartitionDesc.Name = RuntimePartitionDesc.Class->GetFName();
			RuntimePartitionDesc.MainLayer = NewObject<URuntimePartition>(this, RuntimePartitionDesc.Class, NAME_None);
			RuntimePartitionDesc.MainLayer->Name = TEXT("MainPartition");

			RuntimePartitionDesc.UpdateHLODPartitionLayers();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionDesc, Name))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];

		if (RuntimePartitionDesc.Name == NAME_PersistentLevel)
		{
			RuntimePartitionDesc.Name = RuntimePartitionDesc.Class->GetFName();
		}
		else
		{
			for (int32 CurRuntimePartitionIndex = 0; CurRuntimePartitionIndex < RuntimePartitions.Num(); CurRuntimePartitionIndex++)
			{
				if (CurRuntimePartitionIndex != RuntimePartitionIndex)
				{
					if (RuntimePartitionDesc.Name == RuntimePartitions[CurRuntimePartitionIndex].Name)
					{
						RuntimePartitionDesc.Name = RuntimePartitionDesc.Class->GetFName();
						break;
					}
				}
			}
		}
	}
	else if (PropertyName == NAME_HLODLayer)
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		RuntimePartitions[RuntimePartitionIndex].UpdateHLODPartitionLayers();
	}
}

void UWorldPartitionRuntimeHashSet::UpdateHLODPartitionLayers()
{
	for (FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		RuntimePartitionDesc.UpdateHLODPartitionLayers();
	}
}
#endif