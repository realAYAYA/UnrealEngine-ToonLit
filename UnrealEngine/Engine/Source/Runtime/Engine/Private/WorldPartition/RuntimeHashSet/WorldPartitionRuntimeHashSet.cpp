// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "Misc/ArchiveMD5.h"

void FRuntimePartitionStreamingData::CreatePartitionsSpatialIndex() const
{
	if (!SpatialIndex)
	{
		SpatialIndex = MakeUnique<FStaticSpatialIndexType>();

		TArray<TPair<FBox, TObjectPtr<UWorldPartitionRuntimeCell>>> PartitionsElements;
		Algo::Transform(StreamingCells, PartitionsElements, [](UWorldPartitionRuntimeCell* Cell)
		{
			return TPair<FBox, TObjectPtr<UWorldPartitionRuntimeCell>>(Cell->GetContentBounds(), Cell);
		});
		SpatialIndex->Init(PartitionsElements);
	}
}

void FRuntimePartitionStreamingData::DestroyPartitionsSpatialIndex() const
{
	SpatialIndex.Reset();
}

void URuntimeHashSetExternalStreamingObject::CreatePartitionsSpatialIndex() const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		StreamingData.CreatePartitionsSpatialIndex();
	}
}

void URuntimeHashSetExternalStreamingObject::DestroyPartitionsSpatialIndex() const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		StreamingData.DestroyPartitionsSpatialIndex();
	}
}

void URuntimeHashSetExternalStreamingObject::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
#if WITH_EDITOR
	URuntimeHashSetExternalStreamingObject* This = CastChecked<URuntimeHashSetExternalStreamingObject>(InThis);
	for (const FRuntimePartitionStreamingData& StreamingData : This->RuntimeStreamingData)
	{
		if (StreamingData.SpatialIndex.IsValid())
		{
			StreamingData.SpatialIndex->AddReferencedObjects(Collector);
		}
	}
#endif

	Super::AddReferencedObjects(InThis, Collector);
}

UWorldPartitionRuntimeHashSet::UWorldPartitionRuntimeHashSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UClass* RuntimeSpatialHashClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.WorldPartitionRuntimeSpatialHash")))
		{
			RegisterWorldPartitionRuntimeHashConverter(RuntimeSpatialHashClass, GetClass(), [](const UWorldPartitionRuntimeHash* SrcHash) -> UWorldPartitionRuntimeHash*
			{
				return CreateFrom(SrcHash);
			});
		}
	}
#endif
}

void UWorldPartitionRuntimeHashSet::PostLoad()
{
	Super::PostLoad();

	if (GetTypedOuter<UWorld>()->IsGameWorld())
	{
		ForEachStreamingData([](const FRuntimePartitionStreamingData& StreamingData)
		{
			StreamingData.CreatePartitionsSpatialIndex();
			return true;
		});
	}
}

#if WITH_EDITOR
void UWorldPartitionRuntimeHashSet::SetDefaultValues()
{
	check(RuntimePartitions.IsEmpty());

	FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions.AddDefaulted_GetRef();
	RuntimePartitionDesc.Class = URuntimePartitionLHGrid::StaticClass();
	RuntimePartitionDesc.Name = TEXT("MainPartition");

	RuntimePartitionDesc.MainLayer = NewObject<URuntimePartitionLHGrid>(this, NAME_None);
	RuntimePartitionDesc.MainLayer->Name = RuntimePartitionDesc.Name;
	RuntimePartitionDesc.MainLayer->SetDefaultValues();

	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	check(WorldPartition);

	if (const UHLODLayer* HLODLayer = WorldPartition->GetDefaultHLODLayer())
	{
		uint32 HLODIndex = 0;
		while (HLODLayer)
		{
			FRuntimePartitionHLODSetup& HLODSetup = RuntimePartitionDesc.HLODSetups.AddDefaulted_GetRef();

			HLODSetup.Name = HLODLayer->GetFName();
			HLODSetup.bIsSpatiallyLoaded = HLODLayer->IsSpatiallyLoaded();
			HLODSetup.HLODLayers = { HLODLayer };

			if (HLODSetup.bIsSpatiallyLoaded)
			{
				URuntimePartitionLHGrid* HLODLHGrid = NewObject<URuntimePartitionLHGrid>(this, NAME_None);
				HLODLHGrid->CellSize = CastChecked<URuntimePartitionLHGrid>(RuntimePartitionDesc.MainLayer)->CellSize * (2 << HLODIndex);
				HLODLHGrid->LoadingRange = RuntimePartitionDesc.MainLayer->LoadingRange * (2 << HLODIndex);
				HLODSetup.PartitionLayer = HLODLHGrid;
			}
			else
			{
				HLODSetup.PartitionLayer = NewObject<URuntimePartitionPersistent>(this, NAME_None);
				HLODSetup.PartitionLayer->LoadingRange = 0;
			}

			HLODSetup.PartitionLayer->Name = HLODSetup.Name;
			HLODSetup.PartitionLayer->bBlockOnSlowStreaming = false;
			HLODSetup.PartitionLayer->bClientOnlyVisible = true;
			HLODSetup.PartitionLayer->Priority = 0;
			HLODSetup.PartitionLayer->HLODIndex = HLODIndex;

			HLODLayer = HLODLayer->GetParentLayer();
			HLODIndex++;
		}
	}
}

void UWorldPartitionRuntimeHashSet::FlushStreamingContent()
{
	Super::FlushStreamingContent();
	check(!PersistentPartitionDesc.Class);
	RuntimeStreamingData.Empty();
}

bool UWorldPartitionRuntimeHashSet::IsValidGrid(FName GridName, const UClass* ActorClass) const
{
	TArray<FName> MainPartitionTokens;
	TArray<FName> HLODPartitionTokens;

	// Parse the potentially dot separated grid name to identiy the associated runtime partition
	if (ParseGridName(GridName, MainPartitionTokens, HLODPartitionTokens))
	{
		// The None grid name will always map to the first runtime partition in the list
		if (MainPartitionTokens[0].IsNone())
		{
			return true;
		}

		for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
		{
			if (RuntimePartitionDesc.Name == MainPartitionTokens[0])
			{
				if (!RuntimePartitionDesc.MainLayer)
				{
					return false;
				}

				if (!RuntimePartitionDesc.MainLayer->IsValidPartitionTokens(MainPartitionTokens))
				{
					return false;
				}

				return true;
			}
		}
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::IsValidHLODLayer(FName GridName, const FSoftObjectPath& HLODLayerPath) const
{
	if (!RuntimePartitions.Num())
	{
		return false;
	}

	if (const UHLODLayer* HLODLayer = Cast<UHLODLayer>(HLODLayerPath.ResolveObject()))
	{
		// The None grid name will always map to the first runtime partition in the list
		int32 RuntimePartitionIndex = GridName.IsNone() ? 0 : INDEX_NONE;
		
		if (RuntimePartitionIndex == INDEX_NONE)
		{
			TArray<FName> PartitionTokens;
			TArray<FName> HLODPartitionTokens;

			// Parse the potentially dot separated grid name to identiy the associated runtime partition
			if (ParseGridName(GridName, PartitionTokens, HLODPartitionTokens))
			{
				int32 RuntimePartitionIndexLookup = 0;
				for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
				{
					if (RuntimePartitionDesc.Name == PartitionTokens[0])
					{
						RuntimePartitionIndex = RuntimePartitionIndexLookup;
						break;
					}

					RuntimePartitionIndexLookup++;
				}
			}
		}

		if (RuntimePartitionIndex == INDEX_NONE)
		{
			return false;
		}

		for (const FRuntimePartitionHLODSetup& HLODSetup : RuntimePartitions[RuntimePartitionIndex].HLODSetups)
		{
			if (HLODSetup.HLODLayers.Contains(HLODLayer))
			{
				return true;
			}
		}
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::ParseGridName(FName GridName, TArray<FName>& MainPartitionTokens, TArray<FName>& HLODPartitionTokens)
{
	// If the grid name is none, it directly maps to the main partition
	if (GridName.IsNone())
	{
		MainPartitionTokens.Add(NAME_None);
		return true;
	}

	// Split grid name into its partition and HLOD parts
	TArray<FString> GridNameTokens;
	if (!GridName.ToString().ParseIntoArray(GridNameTokens, TEXT(":")))
	{
		GridNameTokens.Add(GridName.ToString());
	}

	// Parsed grid names token should be either "RuntimeHash" or "RuntimeHash:HLODLayer"
	if (GridNameTokens.Num() > 2)
	{
		return false;
	}

	// Parse the target main partition
	TArray<FString> MainPartitionTokensStr;
	if (GridNameTokens[0].ParseIntoArray(MainPartitionTokensStr, TEXT(".")))
	{
		Algo::Transform(MainPartitionTokensStr, MainPartitionTokens, [](const FString& GridName) { return *GridName; });
	}

	// Parse the target HLOD partition
	if (GridNameTokens.IsValidIndex(1))
	{
		HLODPartitionTokens.Add(*GridNameTokens[1]);
	}

	return true;
}

bool UWorldPartitionRuntimeHashSet::HasStreamingContent() const
{
	return !RuntimeStreamingData.IsEmpty();
}

void UWorldPartitionRuntimeHashSet::StoreStreamingContentToExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* OutExternalStreamingObject)
{
	check(!RuntimeStreamingData.IsEmpty());

	Super::StoreStreamingContentToExternalStreamingObject(OutExternalStreamingObject);

	URuntimeHashSetExternalStreamingObject* StreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(OutExternalStreamingObject);
	StreamingObject->RuntimeStreamingData = MoveTemp(RuntimeStreamingData);

	for (FRuntimePartitionStreamingData& StreamingData : StreamingObject->RuntimeStreamingData)
	{
		for (UWorldPartitionRuntimeCell* Cell : StreamingData.StreamingCells)
		{
			Cell->Rename(nullptr, StreamingObject,  REN_DoNotDirty | REN_ForceNoResetLoaders);
		}

		for (UWorldPartitionRuntimeCell* Cell : StreamingData.NonStreamingCells)
		{
			Cell->Rename(nullptr, StreamingObject,  REN_DoNotDirty | REN_ForceNoResetLoaders);
		}
	}
}
#endif

bool UWorldPartitionRuntimeHashSet::InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	if (Super::InjectExternalStreamingObject(ExternalStreamingObject))
	{
		URuntimeHashSetExternalStreamingObject* HashSetExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(ExternalStreamingObject);
		HashSetExternalStreamingObject->CreatePartitionsSpatialIndex();
		return true;
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	if (Super::RemoveExternalStreamingObject(ExternalStreamingObject))
	{
		URuntimeHashSetExternalStreamingObject* HashSetExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(ExternalStreamingObject);
		HashSetExternalStreamingObject->DestroyPartitionsSpatialIndex();
		return true;
	}

	return false;
}

// Streaming interface
void UWorldPartitionRuntimeHashSet::ForEachStreamingCells(TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const
{
	auto ForEachCells = [this, &Func](const TArray<TObjectPtr<UWorldPartitionRuntimeCell>>& InCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InCells)
		{
			if (!Func(Cell))
			{
				return false;
			}
		}
		return true;
	};

	ForEachStreamingData([&ForEachCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		return ForEachCells(StreamingData.StreamingCells) && ForEachCells(StreamingData.NonStreamingCells);
	});
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

	auto ForEachStreamingCells = [&ShouldAddCell, &QuerySource, &Func](FStaticSpatialIndexType* InSpatialIndex, int32 InLoadingRange, FName InGridName)
	{
		if (InSpatialIndex)
		{
			QuerySource.ForEachShape(InLoadingRange, InGridName, false, [InSpatialIndex, &ShouldAddCell, &QuerySource, &Func](const FSphericalSector& Shape)
			{
				const FSphere ShapeSphere(Shape.GetCenter(), Shape.GetRadius());

				InSpatialIndex->ForEachIntersectingElement(ShapeSphere, [&ShouldAddCell, &QuerySource, &Func](UWorldPartitionRuntimeCell* RuntimeCell)
				{
					return !ShouldAddCell(RuntimeCell, QuerySource) || Func(RuntimeCell);
				});
			});
		}

		return true;
	};

	auto ForEachNonStreamingCells = [&ShouldAddCell, &QuerySource, &Func](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonStreamingCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonStreamingCells)
		{
			if (ShouldAddCell(Cell, QuerySource))
			{
				if (!Func(Cell))
				{
					return false;
				}
			}
		}
		return true;
	};

	ForEachStreamingData([&QuerySource, &ForEachStreamingCells, &ForEachNonStreamingCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		return ForEachStreamingCells(StreamingData.SpatialIndex.Get(), StreamingData.LoadingRange, StreamingData.Name) && ForEachNonStreamingCells(StreamingData.NonStreamingCells);
	});
}

void UWorldPartitionRuntimeHashSet::ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const
{
	UWorldPartitionRuntimeHash::FStreamingSourceCells ActivateStreamingSourceCells;
	UWorldPartitionRuntimeHash::FStreamingSourceCells LoadStreamingSourceCells;

	auto ForEachStreamingCells = [this, &Sources, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](FStaticSpatialIndexType* InSpatialIndex, int32 InLoadingRange, FName InGridName)
	{
		if (InSpatialIndex)
		{
			for (const FWorldPartitionStreamingSource& Source : Sources)
			{
				Source.ForEachShape(InLoadingRange, InGridName, false, [this, &Source, InSpatialIndex, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](const FSphericalSector& Shape)
				{
					const FSphere ShapeSphere(Shape.GetCenter(), Shape.GetRadius());

					InSpatialIndex->ForEachIntersectingElement(ShapeSphere, [this, &Source, &Shape, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](UWorldPartitionRuntimeCell* Cell)
					{
						if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
						{
							switch (Cell->GetCellEffectiveWantedState())
							{
							case EDataLayerRuntimeState::Loaded:
								LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
								break;
							case EDataLayerRuntimeState::Activated:
								switch (Source.TargetState)
								{
								case EStreamingSourceTargetState::Loaded:
									LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
									break;
								case EStreamingSourceTargetState::Activated:
									ActivateStreamingSourceCells.AddCell(Cell, Source, Shape);
									break;
								default:
									checkNoEntry();
								}
								break;
							case EDataLayerRuntimeState::Unloaded:
								break;
							default:
								checkNoEntry();
							}
						}
					});
				});
			}
		}
		return true;
	};

	auto ForEachNonStreamingCells = [this, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonStreamingCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonStreamingCells)
		{
			if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
			{
				switch (Cell->GetCellEffectiveWantedState())
				{
				case EDataLayerRuntimeState::Loaded:
					LoadStreamingSourceCells.GetCells().Add(Cell);
					break;
				case EDataLayerRuntimeState::Activated:
					ActivateStreamingSourceCells.GetCells().Add(Cell);
					break;
				case EDataLayerRuntimeState::Unloaded:
					break;
				default:
					checkNoEntry();
				}
			}
		}
		return true;
	};

	ForEachStreamingData([&ForEachStreamingCells, &ForEachNonStreamingCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		return ForEachStreamingCells(StreamingData.SpatialIndex.Get(), StreamingData.LoadingRange, StreamingData.Name) && ForEachNonStreamingCells(StreamingData.NonStreamingCells);
	});

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
	static FName NAME_HLODSetups(TEXT("HLODSetups"));
	static FName NAME_HLODLayers(TEXT("HLODLayers"));

	FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionDesc, Class))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];
		
		RuntimePartitionDesc.MainLayer = nullptr;

		if (RuntimePartitionDesc.Class)
		{
			RuntimePartitionDesc.Name = *FString::Printf(TEXT("%s_%d"), *RuntimePartitionDesc.Class->GetName(), RuntimePartitionIndex);
			RuntimePartitionDesc.MainLayer = NewObject<URuntimePartition>(this, RuntimePartitionDesc.Class, NAME_None);
			RuntimePartitionDesc.MainLayer->SetDefaultValues();
			RuntimePartitionDesc.MainLayer->Name = RuntimePartitionDesc.Name;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionDesc, Name))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];

		int32 HLODSetupsIndex = PropertyChangedEvent.GetArrayIndex(NAME_HLODSetups.ToString());
		if (RuntimePartitionDesc.HLODSetups.IsValidIndex(HLODSetupsIndex))
		{
			FRuntimePartitionHLODSetup& RuntimePartitionHLODSetup = RuntimePartitionDesc.HLODSetups[HLODSetupsIndex];

			if (RuntimePartitionDesc.Name == NAME_PersistentLevel)
			{
				RuntimePartitionHLODSetup.Name = *FString::Printf(TEXT("HLOD_%d"), RuntimePartitionHLODSetup.PartitionLayer->HLODIndex);
			}
			else
			{
				for (int32 CurHLODSetupsIndex = 0; CurHLODSetupsIndex < RuntimePartitionDesc.HLODSetups.Num(); CurHLODSetupsIndex++)
				{
					if (CurHLODSetupsIndex != HLODSetupsIndex)
					{
						if (RuntimePartitionHLODSetup.Name == RuntimePartitionDesc.HLODSetups[CurHLODSetupsIndex].Name)
						{
							RuntimePartitionHLODSetup.Name = *FString::Printf(TEXT("HLOD_%d"), RuntimePartitionHLODSetup.PartitionLayer->HLODIndex);
							break;
						}
					}
				}
			}

			RuntimePartitionHLODSetup.PartitionLayer->Name = RuntimePartitionHLODSetup.Name;
		}
		else
		{
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

			RuntimePartitionDesc.MainLayer->Name = RuntimePartitionDesc.Name;
		}
	}
	else if (PropertyName == NAME_HLODSetups)
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		if (RuntimePartitions.IsValidIndex(RuntimePartitionIndex))
		{
			FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];

			int32 HLODSetupsIndex = PropertyChangedEvent.GetArrayIndex(NAME_HLODSetups.ToString());
			if (RuntimePartitionDesc.HLODSetups.IsValidIndex(HLODSetupsIndex))
			{
				FRuntimePartitionHLODSetup& RuntimePartitionHLODSetup = RuntimePartitionDesc.HLODSetups[HLODSetupsIndex];
				URuntimePartition* ParentRuntimePartition = HLODSetupsIndex ? RuntimePartitionDesc.HLODSetups[HLODSetupsIndex - 1].PartitionLayer : RuntimePartitionDesc.MainLayer;
				RuntimePartitionHLODSetup.Name = *FString::Printf(TEXT("HLOD_%d"), HLODSetupsIndex);
				RuntimePartitionHLODSetup.bIsSpatiallyLoaded = true;
				RuntimePartitionHLODSetup.PartitionLayer = ParentRuntimePartition->CreateHLODRuntimePartition(HLODSetupsIndex);
				RuntimePartitionHLODSetup.PartitionLayer->Name = RuntimePartitionHLODSetup.Name;
			}
		}
	}
	else if (PropertyName == NAME_HLODLayers)
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		if (RuntimePartitions.IsValidIndex(RuntimePartitionIndex))
		{
			FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];

			int32 HLODSetupsIndex = PropertyChangedEvent.GetArrayIndex(NAME_HLODSetups.ToString());
			if (RuntimePartitionDesc.HLODSetups.IsValidIndex(HLODSetupsIndex))
			{
				FRuntimePartitionHLODSetup& RuntimePartitionHLODSetup = RuntimePartitionDesc.HLODSetups[HLODSetupsIndex];

				int32 HLODLayersIndex = PropertyChangedEvent.GetArrayIndex(NAME_HLODLayers.ToString());
				if (RuntimePartitionHLODSetup.HLODLayers.IsValidIndex(HLODLayersIndex))
				{
					const UHLODLayer* HLODLayer = RuntimePartitionHLODSetup.HLODLayers[HLODLayersIndex];

					// Remove duplicated entries
					for (int32 CurrentHLODSetupsIndex = 0; CurrentHLODSetupsIndex < RuntimePartitionDesc.HLODSetups.Num(); CurrentHLODSetupsIndex++)
					{
						FRuntimePartitionHLODSetup& CurrentRuntimePartitionHLODSetup = RuntimePartitionDesc.HLODSetups[CurrentHLODSetupsIndex];
						for (int32 CurrentHLODLayerIndex = 0; CurrentHLODLayerIndex < CurrentRuntimePartitionHLODSetup.HLODLayers.Num(); CurrentHLODLayerIndex++)
						{
							const UHLODLayer* CurrentHLODLayer = CurrentRuntimePartitionHLODSetup.HLODLayers[CurrentHLODLayerIndex];
							if (((CurrentHLODSetupsIndex != HLODSetupsIndex) || (CurrentHLODLayerIndex != HLODLayersIndex)) && (CurrentHLODLayer == HLODLayer))
							{
								CurrentRuntimePartitionHLODSetup.HLODLayers.RemoveAt(CurrentHLODLayerIndex--);
								break;
							}
						}
					}
				}
			}
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionHLODSetup, bIsSpatiallyLoaded))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		if (RuntimePartitions.IsValidIndex(RuntimePartitionIndex))
		{
			FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];

			int32 HLODSetupsIndex = PropertyChangedEvent.GetArrayIndex(NAME_HLODSetups.ToString());
			if (RuntimePartitionDesc.HLODSetups.IsValidIndex(HLODSetupsIndex))
			{
				FRuntimePartitionHLODSetup& RuntimePartitionHLODSetup = RuntimePartitionDesc.HLODSetups[HLODSetupsIndex];

				if (RuntimePartitionHLODSetup.bIsSpatiallyLoaded)
				{
					URuntimePartition* ParentRuntimePartition = HLODSetupsIndex ? RuntimePartitionDesc.HLODSetups[HLODSetupsIndex - 1].PartitionLayer : RuntimePartitionDesc.MainLayer;
					RuntimePartitionHLODSetup.PartitionLayer = ParentRuntimePartition->CreateHLODRuntimePartition(HLODSetupsIndex);
					RuntimePartitionHLODSetup.PartitionLayer->Name = RuntimePartitionHLODSetup.Name;
				}
				else
				{
					RuntimePartitionHLODSetup.PartitionLayer = NewObject<URuntimePartitionPersistent>(this, NAME_None);
					RuntimePartitionHLODSetup.PartitionLayer->Name = RuntimePartitionHLODSetup.Name;
				}
			}
		}
	}
}

UWorldPartitionRuntimeHashSet::FCellUniqueId UWorldPartitionRuntimeHashSet::GetCellUniqueId(const URuntimePartition::FCellDescInstance& InCellDescInstance) const
{
	FCellUniqueId CellUniqueId;
	FName CellNameID = InCellDescInstance.Name;
	FDataLayersID DataLayersID(InCellDescInstance.DataLayerInstances);
	FGuid ContentBundleID(InCellDescInstance.ContentBundleID);

	// Build cell unique name
	{
		UWorld* OuterWorld = GetTypedOuter<UWorld>();
		check(OuterWorld);

		FString InstanceSuffix;
		FString WorldName = FPackageName::GetShortName(OuterWorld->GetPackage());

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
					InstanceSuffix = InstancePackageName.Mid(Index + SourcePackageName.Len());
				}
			}
		}

		TStringBuilder<128> CellNameBuilder;
		CellNameBuilder.Appendf(TEXT("%s_%s"), *WorldName, *CellNameID.ToString());

		if (DataLayersID.GetHash())
		{
			CellNameBuilder.Appendf(TEXT("_d%X"), DataLayersID.GetHash());
		}

		if (ContentBundleID.IsValid())
		{
			CellNameBuilder.Appendf(TEXT("_c%s"), *UContentBundleDescriptor::GetContentBundleCompactString(ContentBundleID));
		}

		if (!InstanceSuffix.IsEmpty())
		{
			CellNameBuilder.Appendf(TEXT("_i%s"), *InstanceSuffix);
		}
	
		CellUniqueId.Name = CellNameBuilder.ToString();
	}

	// Build cell guid
	{
		FArchiveMD5 ArMD5;
		ArMD5 << CellNameID << DataLayersID << ContentBundleID;
		InCellDescInstance.SourcePartition->AppendCellGuid(ArMD5);
		CellUniqueId.Guid = ArMD5.GetGuidFromHash();
		check(CellUniqueId.Guid.IsValid());
	}

	return CellUniqueId;
}
#endif

void UWorldPartitionRuntimeHashSet::ForEachStreamingData(TFunctionRef<bool(const FRuntimePartitionStreamingData&)> Func) const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		if (!Func(StreamingData))
		{
			return;
		}
	}

	for (const TWeakObjectPtr<URuntimeHashExternalStreamingObjectBase>& InjectedExternalStreamingObject : InjectedExternalStreamingObjects)
	{
		if (InjectedExternalStreamingObject.IsValid())
		{
			URuntimeHashSetExternalStreamingObject* ExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(InjectedExternalStreamingObject.Get());
			
			for (const FRuntimePartitionStreamingData& StreamingData : ExternalStreamingObject->RuntimeStreamingData)
			{
				if (!Func(StreamingData))
				{
					return;
				}
			}
		}
	}
}
