// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * WorldPartitionStreamingPolicy Implementation
 */

#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "Algo/Find.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionReplay.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/HLOD/HLODRuntimeSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "Engine/Level.h"
#include "Engine/NetConnection.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "Misc/HashBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionStreamingPolicy)

#define LOCTEXT_NAMESPACE "WorldPartitionStreamingPolicy"

int32 GBlockOnSlowStreaming = 1;
static FAutoConsoleVariableRef CVarBlockOnSlowStreaming(
	TEXT("wp.Runtime.BlockOnSlowStreaming"),
	GBlockOnSlowStreaming,
	TEXT("Set if streaming needs to block when to slow to catchup."));

static FString GServerDisallowStreamingOutDataLayersString;
static FAutoConsoleVariableRef CVarServerDisallowStreamingOutDataLayers(
	TEXT("wp.Runtime.ServerDisallowStreamingOutDataLayers"),
	GServerDisallowStreamingOutDataLayersString,
	TEXT("Comma separated list of data layer names that aren't allowed to be unloaded or deactivated on the server"),
	ECVF_ReadOnly);

bool UWorldPartitionStreamingPolicy::IsUpdateOptimEnabled = true;
FAutoConsoleVariableRef UWorldPartitionStreamingPolicy::CVarUpdateOptimEnabled(
	TEXT("wp.Runtime.UpdateStreaming.EnableOptimization"),
	UWorldPartitionStreamingPolicy::IsUpdateOptimEnabled,
	TEXT("Set to 1 to enable an optimization that skips world partition streaming update\n")
	TEXT("if nothing relevant changed since last update."),
	ECVF_Default);

int32 UWorldPartitionStreamingPolicy::ForceUpdateFrameCount = 0;
FAutoConsoleVariableRef UWorldPartitionStreamingPolicy::CVarForceUpdateFrameCount(
	TEXT("wp.Runtime.UpdateStreaming.ForceUpdateFrameCount"),
	UWorldPartitionStreamingPolicy::ForceUpdateFrameCount,
	TEXT("Frequency (in frames) at which world partition streaming update will be executed regardless if no changes are detected."),
	ECVF_Default);

static void SortStreamingCellsByImportance(TArray<const UWorldPartitionRuntimeCell*>& InOutCells)
{
	if (InOutCells.Num() > 1)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SortStreamingCellsByImportance);
		Algo::Sort(InOutCells, [](const UWorldPartitionRuntimeCell* CellA, const UWorldPartitionRuntimeCell* CellB) { return CellA->SortCompare(CellB) < 0; });
	}
}

UWorldPartitionStreamingPolicy::UWorldPartitionStreamingPolicy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) 
	, WorldPartition(nullptr)
	, ProcessedToActivateCells(0)
	, ProcessedToLoadCells(0)
	, bCriticalPerformanceRequestedBlockTillOnWorld(false)
	, bShouldMergeStreamingSourceInfo(false)
	, CriticalPerformanceBlockTillLevelStreamingCompletedEpoch(0)
	, ServerStreamingStateEpoch(INT_MIN)
	, ServerStreamingEnabledEpoch(INT_MIN)
	, UpdateStreamingHash(0)
	, UpdateStreamingSourcesHash(0)
	, UpdateStreamingStateCalls(0)
	, StreamingPerformance(EWorldPartitionStreamingPerformance::Good)
#if !UE_BUILD_SHIPPING
	, OnScreenMessageStartTime(0.0)
	, OnScreenMessageStreamingPerformance(EWorldPartitionStreamingPerformance::Good)
#endif
{
	if (!IsTemplate())
	{
		WorldPartition = GetOuterUWorldPartition();
		check(WorldPartition);
	}
}

void UWorldPartitionStreamingPolicy::UpdateStreamingSources(bool bCanOptimizeUpdate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingSources);
	if (!WorldPartition->CanStream())
	{
		StreamingSources.Reset();
		return;
	}

	const UWorldPartitionSubsystem* WorldPartitionSubsystem = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
	const uint32 NewUpdateStreamingSourcesHash = WorldPartitionSubsystem->GetStreamingSourcesHash();
	if (bCanOptimizeUpdate && (UpdateStreamingSourcesHash == NewUpdateStreamingSourcesHash))
	{
		TArray<FWorldPartitionStreamingSource> LocalStreamingSources;
		WorldPartitionSubsystem->GetStreamingSources(WorldPartition, LocalStreamingSources);
		check(LocalStreamingSources.Num() == StreamingSources.Num());
		const FTransform WorldToLocal = WorldPartition->GetInstanceTransform().Inverse();
		for (int32 i=0; i<LocalStreamingSources.Num(); i++)
		{
			check(StreamingSources[i].Name == LocalStreamingSources[i].Name);
			StreamingSources[i].Velocity = WorldToLocal.TransformVector(LocalStreamingSources[i].Velocity);
		}
		return;
	}

	StreamingSources.Reset();
	WorldPartitionSubsystem->GetStreamingSources(WorldPartition, StreamingSources);
	UpdateStreamingSourcesHash = NewUpdateStreamingSourcesHash;
}

bool UWorldPartitionStreamingPolicy::IsInBlockTillLevelStreamingCompleted(bool bIsCausedByBadStreamingPerformance /* = false*/) const
{
	const UWorld* World = GetWorld();
	const bool bIsInBlockTillLevelStreamingCompleted = World->GetIsInBlockTillLevelStreamingCompleted();
	if (bIsCausedByBadStreamingPerformance)
	{
		return bIsInBlockTillLevelStreamingCompleted &&
				(StreamingPerformance != EWorldPartitionStreamingPerformance::Good) &&
				(CriticalPerformanceBlockTillLevelStreamingCompletedEpoch == World->GetBlockTillLevelStreamingCompletedEpoch());
	}
	return bIsInBlockTillLevelStreamingCompleted;
}

int32 UWorldPartitionStreamingPolicy::ComputeServerStreamingEnabledEpoch() const
{
	return WorldPartition->IsServer() ? (WorldPartition->IsServerStreamingEnabled() ? 1 : 0) : INT_MIN;
}

bool UWorldPartitionStreamingPolicy::IsUpdateStreamingOptimEnabled()
{
	return UWorldPartitionStreamingPolicy::IsUpdateOptimEnabled &&
		((FWorldPartitionStreamingSource::GetLocationQuantization() > 0) ||
		 (FWorldPartitionStreamingSource::GetRotationQuantization() > 0));
};

uint32 UWorldPartitionStreamingPolicy::ComputeUpdateStreamingHash(bool bCanOptimizeUpdate) const
{
	if (bCanOptimizeUpdate)
	{
		const bool bIsStreaming3D = WorldPartition->RuntimeHash && WorldPartition->RuntimeHash->IsStreaming3D();

		// Build hash that will be used to detect relevant changes
		FHashBuilder HashBuilder;
		if (WorldPartition->RuntimeHash)
		{
			HashBuilder << WorldPartition->RuntimeHash->ComputeUpdateStreamingHash();
		}
		HashBuilder << ComputeServerStreamingEnabledEpoch();
		HashBuilder << WorldPartition->GetStreamingStateEpoch();
		HashBuilder << bIsStreaming3D;
		for (const FWorldPartitionStreamingSource& Source : StreamingSources)
		{
			HashBuilder << Source.GetHash(bIsStreaming3D);
		}

		if (WorldPartition->IsServer())
		{
			HashBuilder << GetWorld()->GetSubsystem<UWorldPartitionSubsystem>()->GetServerClientsVisibleLevelsHash();
		}

		return HashBuilder.GetHash();
	}

	return 0;
};

bool UWorldPartitionStreamingPolicy::GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells) const
{
	if (!WorldPartition || !WorldPartition->RuntimeHash)
	{
		return false;
	}

	FWorldPartitionQueryCache QueryCache;
	TSet<const UWorldPartitionRuntimeCell*> Cells;
	for (const FWorldPartitionStreamingQuerySource& Source : InSources)
	{
		WorldPartition->RuntimeHash->ForEachStreamingCellsQuery(Source, [&Cells](const UWorldPartitionRuntimeCell* Cell)
		{
			Cells.Add(Cell);
			return true;
		}, &QueryCache);
	}

	TArray<const UWorldPartitionRuntimeCell*> SortedCells = Cells.Array();
	Algo::Sort(SortedCells, [&QueryCache](const UWorldPartitionRuntimeCell* CellA, const UWorldPartitionRuntimeCell* CellB)
	{
		int32 SortCompare = CellA->SortCompare(CellB);
		if (SortCompare == 0)
		{
			// Closest distance (lower value is higher prio)
			const double Diff = QueryCache.GetCellMinSquareDist(CellA) - QueryCache.GetCellMinSquareDist(CellB);
			if (FMath::IsNearlyZero(Diff))
			{
				return CellA->GetLevelPackageName().LexicalLess(CellB->GetLevelPackageName());
			}

			return Diff < 0;
		}
		return SortCompare < 0;
	});

	OutCells.Reserve(SortedCells.Num());
	for (const UWorldPartitionRuntimeCell* Cell : SortedCells)
	{
		OutCells.Add(Cell);
	}
	return true;
}

const TSet<FName>& UWorldPartitionStreamingPolicy::GetServerDisallowedStreamingOutDataLayers()
{
	if (!CachedServerDisallowStreamingOutDataLayers.IsSet())
	{
		TSet<FName> ServerDisallowStreamingOutDataLayers;

		if (!GServerDisallowStreamingOutDataLayersString.IsEmpty())
		{
			TArray<FString> AllDLAssetsStrings;
			GServerDisallowStreamingOutDataLayersString.ParseIntoArray(AllDLAssetsStrings, TEXT(","));

			if (const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager())
			{
				for (const FString& DataLayerAssetName : AllDLAssetsStrings)
				{
					if (const UDataLayerInstance* DataLayerInstance = DataLayerManager->GetDataLayerInstanceFromAssetName(FName(DataLayerAssetName)))
					{
						ServerDisallowStreamingOutDataLayers.Add(DataLayerInstance->GetDataLayerFName());
					}
				}
			}
		}

		CachedServerDisallowStreamingOutDataLayers = ServerDisallowStreamingOutDataLayers;
	}
	
	return CachedServerDisallowStreamingOutDataLayers.GetValue();
}

void UWorldPartitionStreamingPolicy::UpdateStreamingState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState);

	++UpdateStreamingStateCalls;

	UWorld* World = GetWorld();
	check(World);
	check(World->IsGameWorld());

	const bool bLastUpdateCompletedLoadingAndActivation = ((ProcessedToActivateCells + ProcessedToLoadCells) == (ToActivateCells.Num() + ToLoadCells.Num()));
	ProcessedToActivateCells = 0;
	ProcessedToLoadCells = 0;
	ToActivateCells.Reset();
	ToLoadCells.Reset();

	// Dermine if the World's BlockTillLevelStreamingCompleted was triggered by WorldPartitionStreamingPolicy
	if (bCriticalPerformanceRequestedBlockTillOnWorld && IsInBlockTillLevelStreamingCompleted())
	{
		bCriticalPerformanceRequestedBlockTillOnWorld = false;
		CriticalPerformanceBlockTillLevelStreamingCompletedEpoch = World->GetBlockTillLevelStreamingCompletedEpoch();
	}

	const bool bIsServer = WorldPartition->IsServer();
	const bool bCanStream = WorldPartition->CanStream();
	const bool bForceFrameUpdate = (UWorldPartitionStreamingPolicy::ForceUpdateFrameCount > 0) ? ((UpdateStreamingStateCalls % UWorldPartitionStreamingPolicy::ForceUpdateFrameCount) == 0) : false;
	const bool bCanOptimizeUpdate =
		WorldPartition->RuntimeHash &&
		bCanStream &&
		!bForceFrameUpdate &&										// We garantee to update every N frame to force some internal updates like UpdateStreamingPerformance
		IsUpdateStreamingOptimEnabled() &&							// Check CVars to see if optimization is enabled
		bLastUpdateCompletedLoadingAndActivation &&					// Don't optimize if last frame didn't process all cells to load/activate
		!IsInBlockTillLevelStreamingCompleted() &&					// Don't optimize when inside UWorld::BlockTillLevelStreamingCompleted
		ActivatedCells.GetPendingAddToWorldCells().IsEmpty(); 		// Don't optimize when remaining cells to add to world
	
	// Update streaming sources
	UpdateStreamingSources(bCanOptimizeUpdate);

	// Detect if nothing relevant changed and early out
	const uint32 NewUpdateStreamingHash = ComputeUpdateStreamingHash(bCanOptimizeUpdate);
	const bool bShouldSkipUpdate = NewUpdateStreamingHash && (UpdateStreamingHash == NewUpdateStreamingHash);
	if (bShouldSkipUpdate)
	{
		return;
	}

	// Update new streaming sources hash
	UpdateStreamingHash = NewUpdateStreamingHash;

	bool bUpdateServerEpoch = false;

	check(FrameActivateCells.IsEmpty());
	check(FrameLoadCells.IsEmpty());

	ON_SCOPE_EXIT
	{
		// Reset frame StreamingSourceCells (optimization to avoid reallocation at every call to UpdateStreamingState)
		FrameActivateCells.Reset();
		FrameLoadCells.Reset();
	};

	const bool bIsServerStreamingEnabled = WorldPartition->IsServerStreamingEnabled();
	const int32 NewServerStreamingEnabledEpoch = ComputeServerStreamingEnabledEpoch();

	const bool bIsStreamingInEnabled = WorldPartition->IsStreamingInEnabled();

	if (bCanStream)
	{
		if (!bIsServer || bIsServerStreamingEnabled || AWorldPartitionReplay::IsPlaybackEnabled(World))
		{
			// When world partition can't stream, all cells must be unloaded
			if (WorldPartition->RuntimeHash)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ForEachStreamingCellsSources);

				UWorldPartitionRuntimeCellData::DirtyStreamingSourceCacheEpoch();

				WorldPartition->RuntimeHash->ForEachStreamingCellsSources(StreamingSources, [this](const UWorldPartitionRuntimeCell* Cell, EStreamingSourceTargetState TargetState)
				{
					switch (TargetState)
					{
					case EStreamingSourceTargetState::Loaded:
						FrameLoadCells.Add(Cell);
						break;
					case EStreamingSourceTargetState::Activated:
						FrameActivateCells.Add(Cell);
						break;
					default:
						check(0);
					}

					return true;
				});
			}
		}

		if (bIsServer)
		{
			const bool bCanServerDeactivateOrUnloadCells = WorldPartition->IsServerStreamingOutEnabled();
			const TSet<FName>& ServerDisallowStreamingOutDataLayers = GetServerDisallowedStreamingOutDataLayers();

			TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ServerUpdate);

			// Server will activate all non data layer cells at first and then load/activate/unload data layer cells only when the data layer states change
			if (!bIsServerStreamingEnabled && 
				bLastUpdateCompletedLoadingAndActivation && 
				(ServerStreamingEnabledEpoch == NewServerStreamingEnabledEpoch) &&
				(ServerStreamingStateEpoch == WorldPartition->GetStreamingStateEpoch()))
			{
				// Server as nothing to do early out
				return; 
			}

			bUpdateServerEpoch = true;

			auto CanServerDeactivateOrUnloadDataLayerCell = [&ServerDisallowStreamingOutDataLayers](const UWorldPartitionRuntimeCell* Cell)
			{
				return !Cell->HasDataLayers() || !Cell->HasAnyDataLayer(ServerDisallowStreamingOutDataLayers);
			};

			auto AddServerFrameCell = [this, CanServerDeactivateOrUnloadDataLayerCell](const UWorldPartitionRuntimeCell* Cell)
			{
				// Keep Data Layer cells in their current state if server cannot deactivate/unload data layer cells
				if (!CanServerDeactivateOrUnloadDataLayerCell(Cell))
				{
					// If cell was activated, keep it activated
					if (ActivatedCells.Contains(Cell))
					{
						FrameActivateCells.Add(Cell);
						return;
					}
					else
					{
						// If cell was loaded, keep it loaded except if it should become activated.
						// In the second case, let the standard code path process it and add it to FrameActivateCells.
						const bool bIsAnActivatedDataLayerCell = Cell->HasDataLayers() && (Cell->GetCellEffectiveWantedState() == EDataLayerRuntimeState::Activated); 
						if (LoadedCells.Contains(Cell) && !bIsAnActivatedDataLayerCell)
						{
							FrameLoadCells.Add(Cell);
							return;
						}
					}
				}
				
				switch (Cell->GetCellEffectiveWantedState())
				{
				case EDataLayerRuntimeState::Loaded:
					FrameLoadCells.Add(Cell);
					break;
				case EDataLayerRuntimeState::Activated:
					FrameActivateCells.Add(Cell);
					break;
				case EDataLayerRuntimeState::Unloaded:
					break;
				default:
					checkNoEntry();
				}
			};

			if (!bIsServerStreamingEnabled)
			{
				if (WorldPartition->RuntimeHash)
				{
					WorldPartition->RuntimeHash->ForEachStreamingCells([this, &AddServerFrameCell](const UWorldPartitionRuntimeCell* Cell)
					{
						AddServerFrameCell(Cell);
						return true;
					});
				}
			}
			else if (!bCanServerDeactivateOrUnloadCells)
			{
				// When server streaming-out is disabled, revisit existing loaded/activated cells and add them in the proper FrameLoadCells/FrameActivateCells
				for (const UWorldPartitionRuntimeCell* Cell : ActivatedCells.GetCells())
				{
					AddServerFrameCell(Cell);
				}
				for (const UWorldPartitionRuntimeCell* Cell : LoadedCells)
				{
					AddServerFrameCell(Cell);
				}
			}
		}
	}

	const TSet<FName>& ServerClientsVisibleLevelNames = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>()->ServerClientsVisibleLevelNames;
	auto ShouldWaitForClientVisibility = [bIsServer, this, &bUpdateServerEpoch, &ServerClientsVisibleLevelNames](const UWorldPartitionRuntimeCell* Cell)
	{
		check(bIsServer);
		if (Cell->ShouldServerWaitForClientLevelVisibility())
		{
			if (ULevel* Level = Cell->GetLevel())
			{
				if (ServerClientsVisibleLevelNames.Contains(Level->GetPackage()->GetFName()))
				{
					UE_CLOG(bUpdateServerEpoch, LogWorldPartition, Verbose, TEXT("Server epoch update delayed by client visibility"));
					bUpdateServerEpoch = false;
					return true;
				}
			}
		}
		return false;
	};

	auto ShouldSkipDisabledHLODCell = [](const UWorldPartitionRuntimeCell* Cell)
	{
		return Cell->GetIsHLOD() && !UWorldPartitionHLODRuntimeSubsystem::IsHLODEnabled();
	};

	// Activation superseeds Loading
	FrameLoadCells = FrameLoadCells.Difference(FrameActivateCells);

	// Determine cells to activate
	if (bCanStream)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ToActivateCells);
		for (const UWorldPartitionRuntimeCell* Cell : FrameActivateCells)
		{
			if (ActivatedCells.Contains(Cell))
			{
				// Update streaming source info for pending add to world cells
				if (bShouldMergeStreamingSourceInfo && ActivatedCells.GetPendingAddToWorldCells().Contains(Cell))
				{
					Cell->MergeStreamingSourceInfo();
				}
			}
			else if (!ShouldSkipCellForPerformance(Cell) && !ShouldSkipDisabledHLODCell(Cell))
			{
				if (bShouldMergeStreamingSourceInfo)
				{
					Cell->MergeStreamingSourceInfo();
				}
				ToActivateCells.Add(Cell);
			}
		}
	}

	// Determine cells to load and server cells to deactivate
	TArray<const UWorldPartitionRuntimeCell*> ServerToDeactivateCells;
	if (bCanStream)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ToLoadCells);
		for (const UWorldPartitionRuntimeCell* Cell : FrameLoadCells)
		{
			if (LoadedCells.Contains(Cell))
			{
				// Update streaming source info for pending load cells
				if (bShouldMergeStreamingSourceInfo && !Cell->GetLevel())
				{
					Cell->MergeStreamingSourceInfo();
				}
			}
			else
			{
				if (!ShouldSkipCellForPerformance(Cell) && !ShouldSkipDisabledHLODCell(Cell))
				{
					// Server deactivated cells are processed right away (see below for details)
					if (const bool bIsServerCellToDeactivate = bIsServer && ActivatedCells.Contains(Cell))
					{
						// Only deactivated server cells need to call ShouldWaitForClientVisibility (those part of ActivatedCells)
						if (!ShouldWaitForClientVisibility(Cell))
						{
							ServerToDeactivateCells.Add(Cell);
						}
					}
					else
					{
						if (bShouldMergeStreamingSourceInfo)
						{
							Cell->MergeStreamingSourceInfo();
						}
						ToLoadCells.Add(Cell);
					}
				}
			}
		}
	}

	// Determine cells to unload
	TArray<const UWorldPartitionRuntimeCell*> ToUnloadCells;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ToUnloadCells);
		auto BuildCellsToUnload = [this, &ToUnloadCells, bCanStream, bIsServer, ShouldWaitForClientVisibility](const TSet<TObjectPtr<const UWorldPartitionRuntimeCell>>& InCells)
		{
			for (const UWorldPartitionRuntimeCell* Cell : InCells)
			{
				if (!FrameActivateCells.Contains(Cell) && !FrameLoadCells.Contains(Cell))
				{
					if (!bCanStream || !bIsServer || !ShouldWaitForClientVisibility(Cell))
					{
						ToUnloadCells.Add(Cell);
					}
				}
			}
		};

		BuildCellsToUnload(ActivatedCells.GetCells());
		BuildCellsToUnload(LoadedCells);
	}

	UE_SUPPRESS(LogWorldPartition, Verbose,
		if ((bIsStreamingInEnabled && (ToActivateCells.Num() > 0 || ToLoadCells.Num() > 0)) || ToUnloadCells.Num() > 0)
		{
			UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy: CellsToActivate(%d), CellsToLoad(%d), CellsToUnload(%d)"), ToActivateCells.Num(), ToLoadCells.Num(), ToUnloadCells.Num());
			FTransform LocalToWorld = WorldPartition->GetInstanceTransform();
			for (int i = 0; i < StreamingSources.Num(); ++i)
			{
				FVector ViewLocation = LocalToWorld.TransformPosition(StreamingSources[i].Location);
				FRotator ViewRotation = LocalToWorld.TransformRotation(StreamingSources[i].Rotation.Quaternion()).Rotator();
				UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy: Sources[%d] = %s,%s"), i, *ViewLocation.ToString(), *ViewRotation.ToString());
			}
		});

#if !UE_BUILD_SHIPPING
	UpdateDebugCellsStreamingPriority(FrameActivateCells, FrameLoadCells);
#endif

	// Unloaded cells
	if (ToUnloadCells.Num() > 0)
	{
		SetCellsStateToUnloaded(ToUnloadCells);
	}
	
	// Server deactivated cells (activated -> loaded)
	// 
	// Server deactivation is handle right away to ensure that even if WorldPartitionSubsystem::UpdateStreamingState
	// is running in incremental mode, server deactivated cells will make their streaming level ShouldBeVisible() 
	// return false. This way, UNetConnection::UpdateLevelVisibilityInternal will not allow clients to make their 
	// streaming level visible (see LevelVisibility.bTryMakeVisible).
	for (const UWorldPartitionRuntimeCell* ServerCellToDeactivate : ServerToDeactivateCells)
	{
		int32 DummyMaxCellToLoad = 0; // Deactivating is not concerned by MaxCellsToLoad
		SetCellStateToLoaded(ServerCellToDeactivate, DummyMaxCellToLoad);
	}

	// Evaluate streaming performance based on cells that should be activated
	UpdateStreamingPerformance(FrameActivateCells);
	
	// Update Epoch if we aren't waiting for clients anymore
	if (bIsServer)
	{
		if (bUpdateServerEpoch)
		{
			ServerStreamingStateEpoch = WorldPartition->GetStreamingStateEpoch();
			ServerStreamingEnabledEpoch = NewServerStreamingEnabledEpoch;
			UE_LOG(LogWorldPartition, Verbose, TEXT("Server epoch updated"));
		}
		else
		{
			// Invalidate UpdateStreamingHash as it was built with latest epochs 
			// and is now also used to optimize server's update streaming.
			UpdateStreamingHash = 0;
		}
	}
}

#if !UE_BUILD_SHIPPING
void UWorldPartitionStreamingPolicy::UpdateDebugCellsStreamingPriority(const TSet<const UWorldPartitionRuntimeCell*>& ActivateStreamingCells, const TSet<const UWorldPartitionRuntimeCell*>& LoadStreamingCells)
{
	if (FWorldPartitionDebugHelper::IsRuntimeSpatialHashCellStreamingPriorityShown())
	{
		TArray<const UWorldPartitionRuntimeCell*> Cells = ActivateStreamingCells.Array();
		Cells.Append(LoadStreamingCells.Array());

		if (bShouldMergeStreamingSourceInfo)
		{
			for (const UWorldPartitionRuntimeCell* Cell : Cells)
			{
				Cell->MergeStreamingSourceInfo();
			}
		}

		SortStreamingCellsByImportance(Cells);

		const int32 CellCount = Cells.Num();
		int32 CellPrio = 0;
		for (const UWorldPartitionRuntimeCell* SortedCell : Cells)
		{
			const_cast<UWorldPartitionRuntimeCell*>(SortedCell)->SetDebugStreamingPriority(float(CellPrio++) / CellCount);
		}
	}
}
#endif

void UWorldPartitionStreamingPolicy::UpdateStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& CellsToActivate)
{		
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingPerformance);
	UWorld* World = GetWorld();
	// If we are currently in a blocked loading just reset the on screen message time and return
	if (StreamingPerformance == EWorldPartitionStreamingPerformance::Critical && IsInBlockTillLevelStreamingCompleted())
	{
#if !UE_BUILD_SHIPPING
		OnScreenMessageStartTime = FPlatformTime::Seconds();
#endif
		return;
	}

	if (WorldPartition->RuntimeHash)
	{
		EWorldPartitionStreamingPerformance NewStreamingPerformance = WorldPartition->RuntimeHash->GetStreamingPerformance(CellsToActivate);

		if (StreamingPerformance != NewStreamingPerformance)
		{
			UE_LOG(LogWorldPartition, Log, TEXT("Streaming performance changed: %s -> %s"),
				*StaticEnum<EWorldPartitionStreamingPerformance>()->GetDisplayNameTextByValue((int64)StreamingPerformance).ToString(),
				*StaticEnum<EWorldPartitionStreamingPerformance>()->GetDisplayNameTextByValue((int64)NewStreamingPerformance).ToString());

			StreamingPerformance = NewStreamingPerformance;
		}
	}

#if !UE_BUILD_SHIPPING
	if (StreamingPerformance != EWorldPartitionStreamingPerformance::Good)
	{
		// performance still bad keep message alive
		OnScreenMessageStartTime = FPlatformTime::Seconds();
		OnScreenMessageStreamingPerformance = StreamingPerformance;
	}
#endif
	
	if (StreamingPerformance == EWorldPartitionStreamingPerformance::Critical)
	{
		const bool bIsServer = WorldPartition->IsServer();
		const bool bIsServerStreamingEnabled = WorldPartition->IsServerStreamingEnabled();
		const bool bCanBlockOnSlowStreaming = GBlockOnSlowStreaming && (!bIsServer || bIsServerStreamingEnabled);

		// This is a very simple implementation of handling of critical streaming conditions.
		if (bCanBlockOnSlowStreaming && !IsInBlockTillLevelStreamingCompleted())
		{
			World->bRequestedBlockOnAsyncLoading = true;
			bCriticalPerformanceRequestedBlockTillOnWorld = true;
		}
	}
}

#if !UE_BUILD_SHIPPING
void UWorldPartitionStreamingPolicy::GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages)
{
	// Keep displaying for 2 seconds (or more if health stays bad)
	double DisplayTime = FPlatformTime::Seconds() - OnScreenMessageStartTime;
	if (DisplayTime < 2.0)
	{
		switch (OnScreenMessageStreamingPerformance)
		{
		case EWorldPartitionStreamingPerformance::Critical:
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Error, LOCTEXT("WPStreamingCritical", "[Critical] WorldPartition Streaming Performance"));
			break;
		case EWorldPartitionStreamingPerformance::Slow:
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, LOCTEXT("WPStreamingWarning", "[Slow] WorldPartition Streaming Performance"));
			break;
		default:
			break;
		}
	}
	else
	{
		OnScreenMessageStreamingPerformance = EWorldPartitionStreamingPerformance::Good;
	}
}
#endif

bool UWorldPartitionStreamingPolicy::ShouldSkipCellForPerformance(const UWorldPartitionRuntimeCell* Cell) const
{
	// When performance is degrading start skipping non blocking cells
	if (!Cell->GetBlockOnSlowLoading())
	{
		if (!WorldPartition->IsServer())
		{
			return IsInBlockTillLevelStreamingCompleted(/*bIsCausedByBadStreamingPerformance*/true);
		}
	}
	return false;
}

void UWorldPartitionStreamingPolicy::GetCellsToUpdate(TArray<const UWorldPartitionRuntimeCell*>& OutToLoadCells, TArray<const UWorldPartitionRuntimeCell*>& OutToActivateCells)
{
	OutToLoadCells.Append(ToLoadCells);
	OutToActivateCells.Append(ToActivateCells);
}

void UWorldPartitionStreamingPolicy::GetCellsToReprioritize(TArray<const UWorldPartitionRuntimeCell*>& OutToReprioritizeLoadCells, TArray<const UWorldPartitionRuntimeCell*>& OutToReprioritizeActivateCells)
{
	for (const UWorldPartitionRuntimeCell* Cell : LoadedCells)
	{
		if (!Cell->GetLevel())
		{
			OutToReprioritizeLoadCells.Add(Cell);
		}
	}

	for (const UWorldPartitionRuntimeCell* Cell : ActivatedCells.GetPendingAddToWorldCells())
	{
		OutToReprioritizeActivateCells.Add(Cell);
	}
}

void UWorldPartitionStreamingPolicy::SetCellStateToLoaded(const UWorldPartitionRuntimeCell* InCell, int32& InOutMaxCellsToLoad)
{
	bool bLoadCell = false;
	if (ActivatedCells.Contains(InCell))
	{
		InCell->Deactivate();
		ActivatedCells.Remove(InCell);
		bLoadCell = true;
		
	}
	else if (WorldPartition->IsStreamingInEnabled())
	{
		if (InOutMaxCellsToLoad > 0)
		{
			InCell->Load();
			bLoadCell = true;
			if (!InCell->IsAlwaysLoaded())
			{
				--InOutMaxCellsToLoad;
			}
		}
	}

	if (bLoadCell)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::SetCellStateToLoaded %s"), *InCell->GetName());
		LoadedCells.Add(InCell);
		++ProcessedToLoadCells;
	}
}

void UWorldPartitionStreamingPolicy::SetCellStateToActivated(const UWorldPartitionRuntimeCell* InCell, int32& InOutMaxCellsToLoad)
{
	if (!WorldPartition->IsStreamingInEnabled())
	{
		return;
	}

	bool bActivateCell = false;
	if (LoadedCells.Contains(InCell))
	{
		LoadedCells.Remove(InCell);
		bActivateCell = true;
	}
	else if (InOutMaxCellsToLoad > 0)
	{
		if (!InCell->IsAlwaysLoaded())
		{
			--InOutMaxCellsToLoad;
		}
		bActivateCell = true;
	}

	if (bActivateCell)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::SetCellStateToActivated %s"), *InCell->GetName());
		ActivatedCells.Add(InCell);
		InCell->Activate();
		++ProcessedToActivateCells;
	}
}

void UWorldPartitionStreamingPolicy::SetCellsStateToUnloaded(const TArray<const UWorldPartitionRuntimeCell*>& InToUnloadCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SetCellsStateToUnloaded);

	for (const UWorldPartitionRuntimeCell* Cell : InToUnloadCells)
	{
		if (Cell->CanUnload())
		{
			UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::UnloadCells %s"), *Cell->GetName());
			Cell->Unload();
			ActivatedCells.Remove(Cell);
			LoadedCells.Remove(Cell);
		}
	}
}

bool UWorldPartitionStreamingPolicy::CanAddCellToWorld(const UWorldPartitionRuntimeCell* InCell) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::CanAddCellToWorld);

	check(InCell);
	check(WorldPartition->IsInitialized());

	// Always allow AddToWorld in Dedicated server and Listen server
	if (WorldPartition->IsServer())
	{
		return true;
	}

	// Always allow AddToWorld when not inside UWorld::BlockTillLevelStreamingCompleted that was not triggered by bad streaming performance
	if (!IsInBlockTillLevelStreamingCompleted(/*bIsCausedByBadStreamingPerformance*/true))
	{
		return true;
	}

	return !ShouldSkipCellForPerformance(InCell);
}

bool UWorldPartitionStreamingPolicy::IsStreamingCompleted(const TArray<FWorldPartitionStreamingSource>* InStreamingSources) const
{
	const UWorld* World = GetWorld();
	check(World);
	check(World->IsGameWorld());
	const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager();
	const bool bTestProvidedStreamingSource = !!InStreamingSources;

	// Always test non-spatial cells
	{
		// Test non-data layer and activated data layers
		TArray<FWorldPartitionStreamingQuerySource> QuerySources;
		FWorldPartitionStreamingQuerySource& QuerySource = QuerySources.Emplace_GetRef();
		QuerySource.bSpatialQuery = false;
		QuerySource.bDataLayersOnly = false;
		QuerySource.DataLayers = DataLayerManager->GetEffectiveActiveDataLayerNames().Array();
		if (!IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, QuerySources, true))
		{
			return false;
		}

		// Test only loaded data layers
		if (!DataLayerManager->GetEffectiveLoadedDataLayerNames().IsEmpty())
		{
			QuerySource.bDataLayersOnly = true;
			QuerySource.DataLayers = DataLayerManager->GetEffectiveLoadedDataLayerNames().Array();
			if (!IsStreamingCompleted(EWorldPartitionRuntimeCellState::Loaded, QuerySources, true))
			{
				return false;
			}
		}
	}

	// Test spatially loaded cells using streaming sources (or provided streaming source)
	TArrayView<const FWorldPartitionStreamingSource> QueriedStreamingSources = bTestProvidedStreamingSource ? *InStreamingSources : StreamingSources;
	for (const FWorldPartitionStreamingSource& StreamingSource : QueriedStreamingSources)
	{
		// Build a query source from a Streaming Source
		TArray<FWorldPartitionStreamingQuerySource> QuerySources;
		FWorldPartitionStreamingQuerySource& QuerySource = QuerySources.Emplace_GetRef();
		QuerySource.bSpatialQuery = true;
		QuerySource.Location = StreamingSource.Location;
		QuerySource.Rotation = StreamingSource.Rotation;
		QuerySource.TargetBehavior = StreamingSource.TargetBehavior;
		QuerySource.TargetGrids = StreamingSource.TargetGrids;
		QuerySource.Shapes = StreamingSource.Shapes;
		QuerySource.bUseGridLoadingRange = true;
		QuerySource.Radius = 0.f;
		QuerySource.bDataLayersOnly = false;
		QuerySource.DataLayers = (StreamingSource.TargetState == EStreamingSourceTargetState::Loaded) ? DataLayerManager->GetEffectiveLoadedDataLayerNames().Array() : DataLayerManager->GetEffectiveActiveDataLayerNames().Array();

		// Execute query
		const EWorldPartitionRuntimeCellState QueryState = (StreamingSource.TargetState == EStreamingSourceTargetState::Loaded) ? EWorldPartitionRuntimeCellState::Loaded : EWorldPartitionRuntimeCellState::Activated;
		if (!IsStreamingCompleted(QueryState, QuerySources, true))
		{
			return false;
		}
	}

	return true;
}

bool UWorldPartitionStreamingPolicy::IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState) const
{
	const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager();
	const bool bIsHLODEnabled = UWorldPartitionHLODRuntimeSubsystem::IsHLODEnabled();

	bool bResult = true;
	for (const FWorldPartitionStreamingQuerySource& QuerySource : QuerySources)
	{
		WorldPartition->RuntimeHash->ForEachStreamingCellsQuery(QuerySource, [QuerySource, QueryState, bExactState, bIsHLODEnabled, DataLayerManager, &bResult](const UWorldPartitionRuntimeCell* Cell)
		{
			EWorldPartitionRuntimeCellState CellState = Cell->GetCurrentState();
			if (CellState != QueryState)
			{
				bool bSkipCell = false;

				// Don't consider HLOD cells if HLODs are disabled.
				if (!bIsHLODEnabled)
				{
					bSkipCell = Cell->GetIsHLOD();
				}

				// If we are querying for Unloaded/Loaded but a Cell is part of a data layer outside of the query that is activated do not consider it
				if (!bSkipCell && QueryState < CellState)
				{
					for (const FName& CellDataLayer : Cell->GetDataLayers())
					{
						if (!QuerySource.DataLayers.Contains(CellDataLayer) && DataLayerManager->GetDataLayerInstanceEffectiveRuntimeState(DataLayerManager->GetDataLayerInstanceFromName(CellDataLayer)) > EDataLayerRuntimeState::Unloaded)
						{
							bSkipCell = true;
							break;
						}
					}
				}
								
				if (!bSkipCell && (bExactState || CellState < QueryState))
				{
					bResult = false;
					return false;
				}
			}

			return true;
		});
	}
	return bResult;
}

bool UWorldPartitionStreamingPolicy::DrawRuntimeHash2D(FWorldPartitionDraw2DContext& DrawContext)
{
	if (StreamingSources.Num() > 0 && WorldPartition->RuntimeHash)
	{
		return WorldPartition->RuntimeHash->Draw2D(DrawContext);
	}
	return false;
}

void UWorldPartitionStreamingPolicy::DrawRuntimeHash3D()
{
	if (WorldPartition->IsInitialized() && WorldPartition->RuntimeHash)
	{
		WorldPartition->RuntimeHash->Draw3D(StreamingSources);
	}
}

void UWorldPartitionStreamingPolicy::OnCellShown(const UWorldPartitionRuntimeCell* InCell)
{
	ActivatedCells.OnAddedToWorld(InCell);
}

void UWorldPartitionStreamingPolicy::OnCellHidden(const UWorldPartitionRuntimeCell* InCell)
{
	ActivatedCells.OnRemovedFromWorld(InCell);
}

void FActivatedCells::Add(const UWorldPartitionRuntimeCell* InCell)
{
	Cells.Add(InCell);
	if (!InCell->IsAlwaysLoaded())
	{
		PendingAddToWorldCells.Add(InCell);
	}
}

void FActivatedCells::Remove(const UWorldPartitionRuntimeCell* InCell)
{
	Cells.Remove(InCell);
	PendingAddToWorldCells.Remove(InCell);
}

void FActivatedCells::OnAddedToWorld(const UWorldPartitionRuntimeCell* InCell)
{
	PendingAddToWorldCells.Remove(InCell);
}

void FActivatedCells::OnRemovedFromWorld(const UWorldPartitionRuntimeCell* InCell)
{
	if (Cells.Contains(InCell))
	{
		if (!InCell->IsAlwaysLoaded())
		{
			PendingAddToWorldCells.Add(InCell);
		}
	}
}

#undef LOCTEXT_NAMESPACE
