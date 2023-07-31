// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"

#if WITH_EDITOR

#include "WorldPartition/RuntimeSpatialHash/RuntimeSpatialHashGridHelper.h"

#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartitionHelpers.h"

#include "UObject/GCObjectScopeGuard.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"

#include "Algo/ForEach.h"

#include "EngineUtils.h"

#include "StaticMeshCompiler.h"

#include "UObject/SavePackage.h"


DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionRuntimeSpatialHashHLOD, Log, All);


static void SavePackage(UPackage* Package, ISourceControlHelper* SourceControlHelper)
{
	if (SourceControlHelper)
	{
		SourceControlHelper->Save(Package);
	}
	else
	{
		Package->MarkAsFullyLoaded();

		FPackagePath PackagePath = FPackagePath::FromPackageNameChecked(Package->GetName());
		const FString PackageFileName = PackagePath.GetLocalFullPath();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		if (!UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
		{
			UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Error, TEXT("Error saving package %s."), *Package->GetName());
			check(0);
		}
	}
}

static void DeletePackage(const FString& PackageName, ISourceControlHelper* SourceControlHelper)
{
	FPackagePath PackagePath = FPackagePath::FromPackageNameChecked(PackageName);
	const FString PackageFileName = PackagePath.GetLocalFullPath();

	if (SourceControlHelper)
	{
		SourceControlHelper->Delete(PackageFileName);
	}
	else
	{
		FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PackageFileName);
	}
}

static void DeletePackage(UPackage* Package, ISourceControlHelper* SourceControlHelper)
{
	if (SourceControlHelper)
	{
		SourceControlHelper->Delete(Package);
	}
	else
	{
		DeletePackage(Package->GetName(), SourceControlHelper);
	}
}

static void DeletePackage(UWorldPartition* WorldPartition, FWorldPartitionActorDesc* ActorDesc, ISourceControlHelper* SourceControlHelper)
{
	if (ActorDesc->IsLoaded())
	{
		DeletePackage(ActorDesc->GetActor()->GetPackage(), SourceControlHelper);
		WorldPartition->GetActorDescContainer()->OnPackageDeleted(ActorDesc->GetActor()->GetPackage());
	}
	else
	{
		DeletePackage(ActorDesc->GetActorPackage().ToString(), SourceControlHelper);
		WorldPartition->RemoveActor(ActorDesc->GetGuid());
	}
}

static void GameTick(UWorld* InWorld)
{
	static int32 TickRendering = 0;
	static const int32 FlushRenderingFrequency = 256;

	// Perform a GC when memory usage exceeds a given threshold
	if (FWorldPartitionHelpers::HasExceededMaxMemory())
	{
		FWorldPartitionHelpers::DoCollectGarbage();
	}

	// When running with -AllowCommandletRendering we want to flush
	if (((++TickRendering % FlushRenderingFrequency) == 0) && IsAllowCommandletRendering())
	{
		FWorldPartitionHelpers::FakeEngineTick(InWorld);
	}
}

static TArray<FGuid> GenerateHLODsForGrid(UWorldPartition* WorldPartition, const IStreamingGenerationContext* StreamingGenerationContext, const FSpatialHashRuntimeGrid& RuntimeGrid, uint32 HLODLevel, FHLODCreationContext& Context, ISourceControlHelper* SourceControlHelper, bool bCreateActorsOnly, const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances)
{
	const IStreamingGenerationContext::FActorSetContainer* MainActorSetContainer = StreamingGenerationContext->GetMainWorldContainer();
	const FBox WorldBounds = StreamingGenerationContext->GetWorldBounds();

	const FSquare2DGridHelper PartitionedActors = GetPartitionedActors(WorldBounds, RuntimeGrid, ActorSetInstances);
	const FSquare2DGridHelper::FGridLevel::FGridCell& AlwaysLoadedCell = PartitionedActors.GetAlwaysLoadedCell();

	auto ShouldGenerateHLODs = [&AlwaysLoadedCell](const FSquare2DGridHelper::FGridLevel::FGridCell& GridCell, const FSquare2DGridHelper::FGridLevel::FGridCellDataChunk& GridCellDataChunk)
	{
		const bool bIsCellAlwaysLoaded = &GridCell == &AlwaysLoadedCell;
		const bool bChunkHasActors = !GridCellDataChunk.GetActorSetInstances().IsEmpty();

		const bool bShouldGenerateHLODs = !bIsCellAlwaysLoaded && bChunkHasActors;
		return bShouldGenerateHLODs;
	};

	// Quick pass to compute the number of cells we'll have to process, to provide a meaningful progress display
	int32 NbCellsToProcess = 0;
	PartitionedActors.ForEachCells([&](const FSquare2DGridHelper::FGridLevel::FGridCell& GridCell)
	{
		for (const FSquare2DGridHelper::FGridLevel::FGridCellDataChunk& GridCellDataChunk : GridCell.GetDataChunks())
		{
			const bool bShouldGenerateHLODs = ShouldGenerateHLODs(GridCell, GridCellDataChunk);
			if (bShouldGenerateHLODs)
			{
				NbCellsToProcess++;
			}
		}
	});

	UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Verbose, TEXT("Building HLODs for grid %s..."), *RuntimeGrid.GridName.ToString());

	FScopedSlowTask SlowTask(NbCellsToProcess, FText::FromString(FString::Printf(TEXT("Building HLODs for grid %s..."), *RuntimeGrid.GridName.ToString())));
	SlowTask.MakeDialog();

	TArray<FGuid> GridHLODActors;
	PartitionedActors.ForEachCells([&](const FSquare2DGridHelper::FGridLevel::FGridCell& GridCell)
	{
		const FGridCellCoord CellCoord = GridCell.GetCoords();

		FBox2D CellBounds2D;
		PartitionedActors.GetCellBounds(CellCoord, CellBounds2D);

		FGridCellCoord CellGlobalCoord;
		verify(PartitionedActors.GetCellGlobalCoords(CellCoord, CellGlobalCoord));

		for (const FSquare2DGridHelper::FGridLevel::FGridCellDataChunk& GridCellDataChunk : GridCell.GetDataChunks())
		{
			// Fake tick
			GameTick(WorldPartition->GetWorld());

			const bool bShouldGenerateHLODs = ShouldGenerateHLODs(GridCell, GridCellDataChunk);
			if (bShouldGenerateHLODs)
			{
				SlowTask.EnterProgressFrame(1);
				// todo_ow : Generating hlods is not yet supported for content bundles. Pass an invalid UID to specified the cell is not coming from ContentBundles.
				FGuid ContentBundleInvalidUID;
				FString CellName = UWorldPartitionRuntimeSpatialHash::GetCellNameString(RuntimeGrid.GridName, CellGlobalCoord, GridCellDataChunk.GetDataLayersID(), ContentBundleInvalidUID);

				UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Verbose, TEXT("Creating HLOD for cell %s at %s..."), *CellName, *CellCoord.ToString());

				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*CellName);

				UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Display, TEXT("[%d / %d] Processing cell %s..."), (int32)SlowTask.CompletedWork + 1, (int32)SlowTask.TotalAmountOfWork, *CellName);

				// We know the cell's 2D bound but must figure out the bounds in Z
				FBox CellBounds = FBox(FVector(CellBounds2D.Min,  std::numeric_limits<double>::max()), 
									   FVector(CellBounds2D.Max, -std::numeric_limits<double>::max()));

				TArray<IStreamingGenerationContext::FActorInstance> ActorInstances;
				for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : GridCellDataChunk.GetActorSetInstances())
				{
					for (const FGuid& ActorGuid : ActorSetInstance->ActorSet->Actors)
					{
						const IStreamingGenerationContext::FActorInstance& ActorInstance = ActorInstances.Emplace_GetRef(ActorGuid, ActorSetInstance);

						const double ActorMinZ = ActorInstance.GetActorDescView().GetBounds().Min.Z;
						const double ActorMaxZ = ActorInstance.GetActorDescView().GetBounds().Max.Z;

						CellBounds.Min.Z = FMath::Min(CellBounds.Min.Z, ActorMinZ);
						CellBounds.Max.Z = FMath::Max(CellBounds.Max.Z, ActorMaxZ);
					}
				}

				// Ensure the Z bounds are valid
				check(CellBounds.Min.Z <= CellBounds.Max.Z);

				FHLODCreationParams CreationParams;
				CreationParams.WorldPartition = WorldPartition;
				CreationParams.CellName = FName(CellName);
				CreationParams.CellBounds = CellBounds;
				CreationParams.HLODLevel = HLODLevel;
				CreationParams.MinVisibleDistance = RuntimeGrid.LoadingRange;

				IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();
				TArray<AWorldPartitionHLOD*> CellHLODActors = WPHLODUtilities->CreateHLODActors(Context, CreationParams, ActorInstances, GridCellDataChunk.GetDataLayers());
				if (!CellHLODActors.IsEmpty())
				{
					TArray<AWorldPartitionHLOD*> NewCellHLODActors;

					for (AWorldPartitionHLOD* CellHLODActor : CellHLODActors)
					{
						FGuid ActorGuid = CellHLODActor->GetActorGuid();
						GridHLODActors.Add(ActorGuid);

						UPackage* CellHLODActorPackage = CellHLODActor->GetPackage();
						if (CellHLODActorPackage->HasAnyPackageFlags(PKG_NewlyCreated))
						{
							NewCellHLODActors.Add(CellHLODActor);
						}
					}

					for (AWorldPartitionHLOD* CellHLODActor : CellHLODActors)
					{
						if (!bCreateActorsOnly)
						{
							CellHLODActor->BuildHLOD();
						}

						if (CellHLODActor->GetPackage()->IsDirty())
						{
							SavePackage(CellHLODActor->GetPackage(), SourceControlHelper);
						}
					}

					// Update newly created HLOD actors
					for (AWorldPartitionHLOD* CellHLODActor : NewCellHLODActors)
					{
						Context.ActorReferences.Emplace(WorldPartition, CellHLODActor->GetActorGuid());
					}
				}

				// Unload actors
				Context.ActorReferences.Empty();
			}
		}
	});

	// Ensure all async packages writes are completed before we start processing another HLOD level or grid
	UPackage::WaitForAsyncFileWrites();

	// Need to collect garbage here since some HLOD actors have been marked pending kill when destroying them
	// and they may be loaded when generating the next HLOD layer.
	FWorldPartitionHelpers::DoCollectGarbage();

	return GridHLODActors;
}

// Find all referenced HLODLayer assets
static TMap<UHLODLayer*, int32> GatherHLODLayers(UWorldPartition* WorldPartition)
{
	// Gather up all HLODLayers referenced by the actors, along with the HLOD level at which it was used
	TMap<UHLODLayer*, int32> HLODLayersLevel;

	for (FActorDescList::TIterator<> ActorDescIterator(WorldPartition->GetActorDescContainer()); ActorDescIterator; ++ActorDescIterator)
	{
		const FWorldPartitionActorDesc& ActorDesc = **ActorDescIterator;
		if (!ActorDesc.GetActorNativeClass()->IsChildOf<AWorldPartitionHLOD>())
		{
			if (ActorDesc.GetActorIsHLODRelevant())
			{
				UHLODLayer* HLODLayer = UHLODLayer::GetHLODLayer(ActorDesc, WorldPartition);

				// If layer was already encountered, no need to process it again
				if (!HLODLayersLevel.Contains(HLODLayer))
				{
					// Walk up the parent HLOD layers, keep track of HLOD level
					int32 CurrentHLODLevel = 0;
					while (HLODLayer != nullptr)
					{
						int32& HLODLevel = HLODLayersLevel.FindOrAdd(HLODLayer);
						HLODLevel = FMath::Max(HLODLevel, CurrentHLODLevel);

						HLODLayer = HLODLayer->GetParentLayer().LoadSynchronous();
						CurrentHLODLevel++;
					}
				}
			}
		}
	}

	return HLODLayersLevel;
}

static TMap<FName, FSpatialHashRuntimeGrid> CreateHLODGrids(TMap<UHLODLayer*, int32>& InHLODLayersLevel)
{
	TMap<FName, FSpatialHashRuntimeGrid> HLODGrids;

	// Create HLOD runtime grids
	for (const TPair<UHLODLayer*, int32>& HLODLayerLevel : InHLODLayersLevel)
	{
		UHLODLayer* HLODLayer = HLODLayerLevel.Key;

		// No need to create a runtime grid if the HLOD layer is set to be non spatially loaded
		if (HLODLayer->IsSpatiallyLoaded())
		{
			int32 HLODLevel = HLODLayerLevel.Value;

			FSpatialHashRuntimeGrid HLODGrid;
			HLODGrid.CellSize = HLODLayer->GetCellSize();
			HLODGrid.LoadingRange = HLODLayer->GetLoadingRange();
			HLODGrid.DebugColor = FLinearColor::Red;
			HLODGrid.GridName = HLODLayer->GetRuntimeGrid(HLODLevel);
			HLODGrid.bClientOnlyVisible = true;
			HLODGrid.HLODLayer = HLODLayer;

			HLODGrids.Emplace(HLODGrid.GridName, HLODGrid);
		}
	}

	HLODGrids.KeySort(FNameLexicalLess());

	return HLODGrids;
}

// Create/destroy HLOD grid actors
static void UpdateHLODGridsActors(UWorld* World, const TMap<FName, FSpatialHashRuntimeGrid>& HLODGrids, ISourceControlHelper* SourceControlHelper)
{
	static const FName HLODGridTag = TEXT("HLOD");
	static const uint32 HLODGridTagLen = HLODGridTag.ToString().Len();

	// Gather all existing HLOD grid actors, see if some are unused and needs to be deleted
	TMap<FName, ASpatialHashRuntimeGridInfo*> ExistingGridActors;
	for (TActorIterator<ASpatialHashRuntimeGridInfo> ItRuntimeGridActor(World); ItRuntimeGridActor; ++ItRuntimeGridActor)
	{
		ASpatialHashRuntimeGridInfo* GridActor = *ItRuntimeGridActor;
		if (GridActor->ActorHasTag(HLODGridTag))
		{
			const FSpatialHashRuntimeGrid* HLODGrid = HLODGrids.Find(GridActor->GridSettings.GridName);
			if (HLODGrid && GridActor->GridSettings.Priority && GridActor->GridSettings.HLODLayer)
			{
				ExistingGridActors.Emplace(GridActor->GridSettings.GridName, GridActor);
			}
			else
			{
				World->DestroyActor(GridActor);

				DeletePackage(GridActor->GetPackage(), SourceControlHelper);
			}
		}
	}

	// Create missing HLOD grid actors 
	for (const auto& HLODGridEntry : HLODGrids)
	{
		FName GridName = HLODGridEntry.Key;
		if (!ExistingGridActors.Contains(GridName))
		{
			const FSpatialHashRuntimeGrid& GridSettings = HLODGridEntry.Value;

			FActorSpawnParameters SpawnParams;
			SpawnParams.bCreateActorPackage = true;
			ASpatialHashRuntimeGridInfo* GridActor = World->SpawnActor<ASpatialHashRuntimeGridInfo>(SpawnParams);
			GridActor->Tags.Add(HLODGridTag);
			GridActor->SetActorLabel(GridSettings.GridName.ToString());
			GridActor->GridSettings = GridSettings;

			// Setup grid debug color to match HLODColorationColors
			const uint32 LastLODColorationColorIdx = GEngine->HLODColorationColors.Num() - 1;
			uint32 HLODLevel;
			if (!LexTryParseString(HLODLevel, *GridSettings.GridName.ToString().Mid(HLODGridTagLen, 1)))
			{
				HLODLevel = LastLODColorationColorIdx;
			}

			GridActor->GridSettings.Priority = 100 + HLODLevel;

			HLODLevel = FMath::Clamp(HLODLevel + 2, 0U, LastLODColorationColorIdx);
			GridActor->GridSettings.DebugColor = GEngine->HLODColorationColors[HLODLevel];

			SavePackage(GridActor->GetPackage(), SourceControlHelper);
		}
	}
}

bool UWorldPartitionRuntimeSpatialHash::GenerateHLOD(ISourceControlHelper* SourceControlHelper, const IStreamingGenerationContext* StreamingGenerationContext, bool bCreateActorsOnly)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionRuntimeSpatialHash::GenerateHLOD);

	if (!Grids.Num())
	{
		UE_LOG(LogWorldPartitionRuntimeSpatialHashHLOD, Error, TEXT("Invalid partition grids setup"));
		return false;
	}

	UWorldPartition* WorldPartition = GetOuterUWorldPartition();

	// Find all used HLOD layers
	TMap<UHLODLayer*, int32> HLODLayersLevels = GatherHLODLayers(WorldPartition);
	TArray<UHLODLayer*> HLODLayers;
	HLODLayersLevels.GetKeys(HLODLayers);

	// Keep references to HLODLayers or they may be GC'd
	TGCObjectsScopeGuard<UHLODLayer> KeepHLODLayersAlive(HLODLayers);

	TMap<FName, FSpatialHashRuntimeGrid> HLODGrids = CreateHLODGrids(HLODLayersLevels);
	
	TMap<FName, int32> GridsMapping;
	GridsMapping.Add(NAME_None, 0);
	for (int32 i = 0; i < Grids.Num(); i++)
	{
		const FSpatialHashRuntimeGrid& Grid = Grids[i];
		check(!GridsMapping.Contains(Grid.GridName));
		GridsMapping.Add(Grid.GridName, i);
	}

	// HLOD creation context
	FHLODCreationContext Context;
	for (FActorDescList::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition->GetActorDescContainer()); HLODIterator; ++HLODIterator)
	{
		FWorldPartitionHandle HLODActorHandle(WorldPartition, HLODIterator->GetGuid());
		Context.HLODActorDescs.Emplace(HLODIterator->GetActorName(), MoveTemp(HLODActorHandle));
	}

	IStreamingGenerationContext::FActorSetContainer* MainActorSetContainer = const_cast<IStreamingGenerationContext::FActorSetContainer*>(StreamingGenerationContext->GetMainWorldContainer());

	TArray<TArray<const IStreamingGenerationContext::FActorSetInstance*>> GridActorSetInstances;
	GridActorSetInstances.InsertDefaulted(0, Grids.Num());

	StreamingGenerationContext->ForEachActorSetInstance([&GridsMapping, &GridActorSetInstances](const IStreamingGenerationContext::FActorSetInstance& ActorSetInstance)
	{
		int32* FoundIndex = GridsMapping.Find(ActorSetInstance.RuntimeGrid);
		if (!FoundIndex)
		{
			//@todo_ow should this be done upstream?
			UE_LOG(LogWorldPartition, Error, TEXT("Invalid partition grid '%s' referenced by actor cluster"), *ActorSetInstance.RuntimeGrid.ToString());
		}

		int32 GridIndex = FoundIndex ? *FoundIndex : 0;
		GridActorSetInstances[GridIndex].Add(&ActorSetInstance);
	});

	// Keep track of all valid HLOD actors, along with which runtime grid they live in
	TMap<FName, TArray<FGuid>> GridsHLODActors;
	TArray<TUniquePtr<FWorldPartitionActorDescView>> ActorDescViews;

	auto GenerateHLODs = [&GridsHLODActors, &ActorDescViews, &MainActorSetContainer, StreamingGenerationContext, WorldPartition, &Context, SourceControlHelper, bCreateActorsOnly]
	(const FSpatialHashRuntimeGrid& RuntimeGrid, uint32 HLODLevel, const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances)
	{
		// Generate HLODs for this grid
		TArray<FGuid> HLODActors = GenerateHLODsForGrid(WorldPartition, StreamingGenerationContext, RuntimeGrid, HLODLevel, Context, SourceControlHelper, bCreateActorsOnly, ActorSetInstances);

		for (const FGuid& HLODActorGuid : HLODActors)
		{
			FWorldPartitionActorDesc* HLODActorDesc = WorldPartition->GetActorDesc(HLODActorGuid);
			check(HLODActorDesc);

			FWorldPartitionActorDescView* ActorDescView = ActorDescViews.Emplace_GetRef(MakeUnique<FWorldPartitionActorDescView>(HLODActorDesc)).Get();
			FActorDescViewMap* NonConstActorDescViewMap = const_cast<FActorDescViewMap*>(MainActorSetContainer->ActorDescViewMap);			
			NonConstActorDescViewMap->Emplace(HLODActorGuid, *ActorDescView);

			const FWorldPartitionActorDescView& HLODActorDescView = MainActorSetContainer->ActorDescViewMap->FindByGuidChecked(HLODActorGuid);
			GridsHLODActors.FindOrAdd(HLODActorDescView.GetRuntimeGrid()).Add(HLODActorGuid);
		}
	};

	// Generate HLODs for the standard runtime grids (HLOD 0)
	for (int32 GridIndex = 0; GridIndex < Grids.Num(); GridIndex++)
	{
		// Generate HLODs for this grid - retrieve actors from the world partition
		GenerateHLODs(Grids[GridIndex], 0, GridActorSetInstances[GridIndex]);
	}

	// Now, go on and create HLOD actors from HLOD grids (HLOD 1-N)
	for (auto It = HLODGrids.CreateIterator(); It; ++It)
	{
		const FName HLODGridName = It.Key();

		// No need to process empty grids
		if (!GridsHLODActors.Contains(HLODGridName))
		{
			It.RemoveCurrent(); // No need to keep this grid around, we have no actors in it
			continue;
		}

		// Create one actor cluster per HLOD actor
		TArray<IStreamingGenerationContext::FActorSetInstance> HLODActorSetInstances;
		TArray<const IStreamingGenerationContext::FActorSetInstance*> HLODActorSetInstancePtrs;
		HLODActorSetInstances.Reserve(GridsHLODActors[HLODGridName].Num());
		HLODActorSetInstancePtrs.Reserve(GridsHLODActors[HLODGridName].Num());

		for (const FGuid& HLODActorGuid : GridsHLODActors[HLODGridName])
		{
			const FWorldPartitionActorDescView& HLODActorDescView = MainActorSetContainer->ActorDescViewMap->FindByGuidChecked(HLODActorGuid);
			
			IStreamingGenerationContext::FActorSetInstance& NewHLODActorSetInstance = HLODActorSetInstances.Emplace_GetRef();
			
			NewHLODActorSetInstance.Bounds = HLODActorDescView.GetBounds();
			NewHLODActorSetInstance.RuntimeGrid = HLODActorDescView.GetRuntimeGrid();
			NewHLODActorSetInstance.bIsSpatiallyLoaded = HLODActorDescView.GetIsSpatiallyLoaded();
			NewHLODActorSetInstance.DataLayers = UDataLayerSubsystem::GetRuntimeDataLayerInstances(MainActorSetContainer->ActorDescContainer->GetWorld(), HLODActorDescView.GetRuntimeDataLayers());
			NewHLODActorSetInstance.ContainerInstance = MainActorSetContainer;
			NewHLODActorSetInstance.ActorSet = &MainActorSetContainer->ActorSets.AddDefaulted_GetRef();
			const_cast<IStreamingGenerationContext::FActorSet*>(NewHLODActorSetInstance.ActorSet)->Actors.Add(HLODActorDescView.GetGuid());

			HLODActorSetInstancePtrs.Add(&NewHLODActorSetInstance);
		};

		// Generate HLODs for this grid - retrieve actors from our ValidHLODActors map
		// We can't rely on actor descs for newly created HLOD actors
		GenerateHLODs(HLODGrids[HLODGridName], HLODLayersLevels[HLODGrids[HLODGridName].HLODLayer] + 1, HLODActorSetInstancePtrs);
	}

	auto DeleteHLODActor = [&SourceControlHelper, WorldPartition](FWorldPartitionHandle ActorHandle)
	{
		FWorldPartitionActorDesc* HLODActorDesc = ActorHandle.Get();
		check(HLODActorDesc);

		DeletePackage(WorldPartition, HLODActorDesc, SourceControlHelper);
	};

	// Destroy all unreferenced HLOD actors
	for (const auto& HLODActorPair : Context.HLODActorDescs)
	{
		DeleteHLODActor(HLODActorPair.Value);
	}

	// Create/destroy HLOD grid actors
	UpdateHLODGridsActors(GetWorld(), HLODGrids, SourceControlHelper);

	return true;
}

#endif // #if WITH_EDITOR