// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * WorldPartitionStreamingPolicy Implementation
 */

#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionReplay.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "DrawDebugHelpers.h"
#include "DisplayDebugHelpers.h"
#include "RenderUtils.h"
#include "Algo/RemoveIf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionStreamingPolicy)

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

#define LOCTEXT_NAMESPACE "WorldPartitionStreamingPolicy"

static int32 GUpdateStreamingSources = 1;
static FAutoConsoleVariableRef CVarUpdateStreamingSources(
	TEXT("wp.Runtime.UpdateStreamingSources"),
	GUpdateStreamingSources,
	TEXT("Set to 0 to stop updating (freeze) world partition streaming sources."));

static int32 GMaxLoadingStreamingCells = 4;
static FAutoConsoleVariableRef CMaxLoadingStreamingCells(
	TEXT("wp.Runtime.MaxLoadingStreamingCells"),
	GMaxLoadingStreamingCells,
	TEXT("Used to limit the number of concurrent loading world partition streaming cells."));

int32 GBlockOnSlowStreaming = 1;
static FAutoConsoleVariableRef CVarBlockOnSlowStreaming(
	TEXT("wp.Runtime.BlockOnSlowStreaming"),
	GBlockOnSlowStreaming,
	TEXT("Set if streaming needs to block when to slow to catchup."));

static void SortStreamingCellsByImportance(const TSet<const UWorldPartitionRuntimeCell*>& InCells, TArray<const UWorldPartitionRuntimeCell*>& OutSortedCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SortStreamingCellsByImportance);
	OutSortedCells = InCells.Array();
	Algo::Sort(OutSortedCells, [](const UWorldPartitionRuntimeCell* CellA, const UWorldPartitionRuntimeCell* CellB) { return CellA->SortCompare(CellB) < 0; });
}

UWorldPartitionStreamingPolicy::UWorldPartitionStreamingPolicy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) 
	, bCriticalPerformanceRequestedBlockTillOnWorld(false)
	, CriticalPerformanceBlockTillLevelStreamingCompletedEpoch(0)
	, DataLayersStatesServerEpoch(INT_MIN)
	, ContentBundleServerEpoch(INT_MIN)
	, ServerStreamingEnabledEpoch(INT_MIN)
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

void UWorldPartitionStreamingPolicy::UpdateStreamingSources()
{
	if (!GUpdateStreamingSources)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingSources);

	StreamingSources.Reset();

	if (!WorldPartition->CanStream())
	{
		return;
	}

	const FTransform WorldToLocal = WorldPartition->GetInstanceTransform().Inverse();
	UWorld* World = GetWorld();

	const bool bIsServer = WorldPartition->IsServer();
	const bool bIsServerStreamingEnabled = WorldPartition->IsServerStreamingEnabled();

	if (!AWorldPartitionReplay::IsPlaybackEnabled(World) || !WorldPartition->Replay->GetReplayStreamingSources(StreamingSources))
	{
#if WITH_EDITOR
		if (UWorldPartition::IsSimulating())
		{
			// We are in the SIE
			const FVector ViewLocation = WorldToLocal.TransformPosition(GCurrentLevelEditingViewportClient->GetViewLocation());
			const FRotator ViewRotation = WorldToLocal.TransformRotation(GCurrentLevelEditingViewportClient->GetViewRotation().Quaternion()).Rotator();
			static const FName NAME_SIE(TEXT("SIE"));
			StreamingSources.Add(FWorldPartitionStreamingSource(NAME_SIE, ViewLocation, ViewRotation, EStreamingSourceTargetState::Activated, /*bBlockOnSlowLoading=*/false, EStreamingSourcePriority::Default, false));
		}
		else
#endif
		if (!bIsServer || bIsServerStreamingEnabled || AWorldPartitionReplay::IsRecordingEnabled(World))
		{
			UWorldPartitionSubsystem* WorldPartitionSubsystem = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
			check(WorldPartitionSubsystem);

			for (IWorldPartitionStreamingSourceProvider* StreamingSourceProvider : WorldPartitionSubsystem->GetStreamingSourceProviders())
			{
				FWorldPartitionStreamingSource StreamingSource;
				// Default Streaming Source provider's priority to be less than those based on player controllers
				StreamingSource.Priority = EStreamingSourcePriority::Low;
				if (StreamingSourceProvider->GetStreamingSource(StreamingSource))
				{
					// Transform to Local
					StreamingSource.Location = WorldToLocal.TransformPosition(StreamingSource.Location);
					StreamingSource.Rotation = WorldToLocal.TransformRotation(StreamingSource.Rotation.Quaternion()).Rotator();
					StreamingSources.Add(StreamingSource);
				}
			}
		}
	}
		
	// Update streaming sources velocity
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	TSet<FName, DefaultKeyFuncs<FName>, TInlineSetAllocator<8>> ValidStreamingSources;
	for (FWorldPartitionStreamingSource& StreamingSource : StreamingSources)
	{
		if (!StreamingSource.Name.IsNone())
		{
			ValidStreamingSources.Add(StreamingSource.Name);
			FStreamingSourceVelocity& SourceVelocity = StreamingSourcesVelocity.FindOrAdd(StreamingSource.Name, FStreamingSourceVelocity(StreamingSource.Name));
			StreamingSource.Velocity = SourceVelocity.GetAverageVelocity(StreamingSource.Location, CurrentTime);
		}
	}

	// Cleanup StreamingSourcesVelocity
	for (auto It(StreamingSourcesVelocity.CreateIterator()); It; ++It)
	{
		if (!ValidStreamingSources.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

#define WORLDPARTITION_LOG_UPDATESTREAMINGSTATE(Verbosity)\
UE_SUPPRESS(LogWorldPartition, Verbosity, \
{ \
	if (ToActivateCells.Num() > 0 || ToLoadCells.Num() > 0 || ToUnloadCells.Num() > 0) \
	{ \
		UE_LOG(LogWorldPartition, Verbosity, TEXT("UWorldPartitionStreamingPolicy: CellsToActivate(%d), CellsToLoad(%d), CellsToUnload(%d)"), ToActivateCells.Num(), ToLoadCells.Num(), ToUnloadCells.Num()); \
		FTransform LocalToWorld = WorldPartition->GetInstanceTransform(); \
		for (int i = 0; i < StreamingSources.Num(); ++i) \
		{ \
			FVector ViewLocation = LocalToWorld.TransformPosition(StreamingSources[i].Location); \
			FRotator ViewRotation = LocalToWorld.TransformRotation(StreamingSources[i].Rotation.Quaternion()).Rotator(); \
			UE_LOG(LogWorldPartition, Verbosity, TEXT("UWorldPartitionStreamingPolicy: Sources[%d] = %s,%s"), i, *ViewLocation.ToString(), *ViewRotation.ToString()); \
		} \
	} \
}) \

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

void UWorldPartitionStreamingPolicy::UpdateStreamingState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState);

	UWorld* World = GetWorld();
	check(World);
	check(World->IsGameWorld());

	// Dermine if the World's BlockTillLevelStreamingCompleted was triggered by WorldPartitionStreamingPolicy
	if (bCriticalPerformanceRequestedBlockTillOnWorld && IsInBlockTillLevelStreamingCompleted())
	{
		bCriticalPerformanceRequestedBlockTillOnWorld = false;
		CriticalPerformanceBlockTillLevelStreamingCompletedEpoch = World->GetBlockTillLevelStreamingCompletedEpoch();
	}
	
	// Update streaming sources
	UpdateStreamingSources();
	
	TSet<FName> ClientVisibleLevelNames;
	bool bUpdateEpoch = false;

	check(FrameActivateCells.IsEmpty());
	check(FrameLoadCells.IsEmpty());

	ON_SCOPE_EXIT
	{
		// Reset frame StreamingSourceCells (optimization to avoid reallocation at every call to UpdateStreamingState)
		FrameActivateCells.Reset();
		FrameLoadCells.Reset();
	};

	const bool bIsServer = WorldPartition->IsServer();
	const bool bIsServerStreamingEnabled = WorldPartition->IsServerStreamingEnabled();
	const bool bCanDeactivateOrUnloadCells = !bIsServer || WorldPartition->IsServerStreamingOutEnabled();
	const int32 NewServerStreamingEnabledEpoch = bIsServer ? (bIsServerStreamingEnabled ? 1 : 0) : INT_MIN;

	if (!bIsServer || bIsServerStreamingEnabled || AWorldPartitionReplay::IsPlaybackEnabled(World))
	{
		// When world partition can't stream, all cells must be unloaded
		if (WorldPartition->CanStream() && WorldPartition->RuntimeHash)
		{
			UWorldPartitionRuntimeCell::DirtyStreamingSourceCacheEpoch();

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
		// Server will activate all non data layer cells at first and then load/activate/unload data layer cells only when the data layer states change
		if (!bIsServerStreamingEnabled && 
			(ServerStreamingEnabledEpoch == NewServerStreamingEnabledEpoch) &&
			(ContentBundleServerEpoch == FContentBundle::GetContentBundleEpoch()) &&
			(DataLayersStatesServerEpoch == AWorldDataLayers::GetDataLayersStateEpoch()))
		{
			// Server as nothing to do early out
			return; 
		}

		bUpdateEpoch = true;

		// Gather Client visible level names
		if (const UNetDriver* NetDriver = World->GetNetDriver())
		{
			for (UNetConnection* Connection : NetDriver->ClientConnections)
			{
				ClientVisibleLevelNames.Add(Connection->GetClientWorldPackageName());
				ClientVisibleLevelNames.Append(Connection->ClientVisibleLevelNames);
				ClientVisibleLevelNames.Append(Connection->GetClientMakingVisibleLevelNames());
			}
		}

		const UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>();
		TSet<FName> EffectiveActiveDataLayerNames = DataLayerSubsystem->GetEffectiveActiveDataLayerNames();
		TSet<FName> EffectiveLoadedDataLayerNames = DataLayerSubsystem->GetEffectiveLoadedDataLayerNames();

		auto AddServerFrameCells = [this, &EffectiveLoadedDataLayerNames, &EffectiveActiveDataLayerNames](const UWorldPartitionRuntimeCell* Cell)
		{
			// Non Data Layer Cells + Active Data Layers
			if (!Cell->HasDataLayers() || Cell->HasAnyDataLayer(EffectiveActiveDataLayerNames))
			{
				FrameActivateCells.Add(Cell);
			}

			// Loaded Data Layers Cells only
			if (EffectiveLoadedDataLayerNames.Num())
			{
				if (Cell->HasDataLayers() && Cell->HasAnyDataLayer(EffectiveLoadedDataLayerNames))
				{
					FrameLoadCells.Add(Cell);
				}
			}
		};

		if (!bIsServerStreamingEnabled)
		{
			if (WorldPartition->RuntimeHash)
			{
				WorldPartition->RuntimeHash->ForEachStreamingCells([this, &AddServerFrameCells](const UWorldPartitionRuntimeCell* Cell)
				{
					AddServerFrameCells(Cell);
					return true;
				});
			}
		}
		else if (!bCanDeactivateOrUnloadCells)
		{
			// When server streaming-out is disabled, revisit existing loaded/activated cells and add them in the proper FrameLoadCells/FrameActivateCells
			for (const UWorldPartitionRuntimeCell* Cell : ActivatedCells)
			{
				AddServerFrameCells(Cell);
			}
			for (const UWorldPartitionRuntimeCell* Cell : LoadedCells)
			{
				AddServerFrameCells(Cell);
			}
		}
	}

	auto ShouldWaitForClientVisibility = [bIsServer, &ClientVisibleLevelNames, &bUpdateEpoch](const UWorldPartitionRuntimeCell* Cell)
	{
		check(bIsServer);
		if (ULevel* Level = Cell->GetLevel())
		{
			if (ClientVisibleLevelNames.Contains(Cell->GetLevel()->GetPackage()->GetFName()))
			{
				UE_CLOG(bUpdateEpoch, LogWorldPartition, Verbose, TEXT("Server epoch update delayed by client visibility"));
				bUpdateEpoch = false;
				return true;
			}
		}
		return false;
	};

	auto ShouldSkipDisabledHLODCell = [](const UWorldPartitionRuntimeCell* Cell)
	{
		return Cell->GetIsHLOD() && !UHLODSubsystem::IsHLODEnabled();
	};

	// Process cells to activate
	auto ProcessCellsToActivate = [this, &ShouldSkipDisabledHLODCell](TSet<const UWorldPartitionRuntimeCell*>& Cells)
	{
		for (TSet<const UWorldPartitionRuntimeCell*>::TIterator It = Cells.CreateIterator(); It; ++It)
		{
			const UWorldPartitionRuntimeCell* Cell = *It;
			if (ShouldSkipCellForPerformance(Cell) || ShouldSkipDisabledHLODCell(Cell))
			{
				It.RemoveCurrent();
			}
			else
			{
				Cell->MergeStreamingSourceInfo();
			}
		}
	};

	// Process cells to load
	auto ProcessCellsToLoad = [this, bIsServer, &ShouldWaitForClientVisibility, &ShouldSkipDisabledHLODCell](TSet<const UWorldPartitionRuntimeCell*>& Cells)
	{
		for (TSet<const UWorldPartitionRuntimeCell*>::TIterator It = Cells.CreateIterator(); It; ++It)
		{
			const UWorldPartitionRuntimeCell* Cell = *It;
			// Only deactivated server cells need to call ShouldWaitForClientVisibility (those part of ActivatedCells)
			const bool bIsServerCellToDeactivate = bIsServer && ActivatedCells.Contains(Cell);
			const bool bShoudSkipServerCell = bIsServerCellToDeactivate && ShouldWaitForClientVisibility(Cell);

			if (bShoudSkipServerCell || ShouldSkipCellForPerformance(Cell) || ShouldSkipDisabledHLODCell(Cell))
			{
				It.RemoveCurrent();
			}
			else
			{
				Cell->MergeStreamingSourceInfo();
			}
		}
	};

	// Process cells to unload
	auto ProcessCellsToUnload = [this, bIsServer, &ShouldWaitForClientVisibility](TSet<const UWorldPartitionRuntimeCell*>& Cells)
	{
		// Only loop for server as ShouldWaitForClientVisibility only concerns server
		if (bIsServer)
		{
			for (TSet<const UWorldPartitionRuntimeCell*>::TIterator It = Cells.CreateIterator(); It; ++It)
			{
				const UWorldPartitionRuntimeCell* Cell = *It;
				if (ShouldWaitForClientVisibility(Cell))
				{
					It.RemoveCurrent();
				}
			}
		}
	};

	// Activation superseeds Loading
	FrameLoadCells = FrameLoadCells.Difference(FrameActivateCells);

	// Sort cells by importance
	auto SortStreamingCells = [this](const TArray<FWorldPartitionStreamingSource>& Sources, const TSet<const UWorldPartitionRuntimeCell*>& InCells, TArray<const UWorldPartitionRuntimeCell*>& OutCells)
	{
		OutCells.Empty(InCells.Num());
		SortStreamingCellsByImportance(InCells, OutCells);
	};

	// Determine cells to activate (sorted by importance)
	TArray<const UWorldPartitionRuntimeCell*> ToActivateCells;
	{
		TSet<const UWorldPartitionRuntimeCell*> ToActivateCellsUnsorted = FrameActivateCells.Difference(ActivatedCells);
		ProcessCellsToActivate(ToActivateCellsUnsorted);
		SortStreamingCells(StreamingSources, ToActivateCellsUnsorted, ToActivateCells);
	}

	// Determine cells to load (sorted by importance)
	TArray<const UWorldPartitionRuntimeCell*> ToLoadCells;
	{
		TSet<const UWorldPartitionRuntimeCell*> ToLoadCellsUnsorted = FrameLoadCells.Difference(LoadedCells);
		ProcessCellsToLoad(ToLoadCellsUnsorted);
		SortStreamingCells(StreamingSources, ToLoadCellsUnsorted, ToLoadCells);
	}

	// Determine cells to unload
	TArray<const UWorldPartitionRuntimeCell*> ToUnloadCells;
	{
		TSet<const UWorldPartitionRuntimeCell*> ToUnloadCellsUnsorted = ActivatedCells.Union(LoadedCells).Difference(FrameActivateCells.Union(FrameLoadCells));
		ProcessCellsToUnload(ToUnloadCellsUnsorted);
		ToUnloadCells = ToUnloadCellsUnsorted.Array();
	}

	if(World->bMatchStarted)
	{
		WORLDPARTITION_LOG_UPDATESTREAMINGSTATE(Verbose);
	}
	else
	{
		WORLDPARTITION_LOG_UPDATESTREAMINGSTATE(Log);
	}

#if !UE_BUILD_SHIPPING
	UpdateDebugCellsStreamingPriority(FrameActivateCells, FrameLoadCells);
#endif

	if (ToUnloadCells.Num() > 0)
	{
		SetCellsStateToUnloaded(ToUnloadCells);
	}

	// Do Activation State first as it is higher prio than Load State (if we have a limited number of loading cells per frame)
	if (ToActivateCells.Num() > 0)
	{
		SetCellsStateToActivated(ToActivateCells);
	}

	if (ToLoadCells.Num() > 0)
	{
		SetCellsStateToLoaded(ToLoadCells);
	}

	// Sort cells and update streaming priority 
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SortCellsAndUpdateStreamingPriority);
		TSet<const UWorldPartitionRuntimeCell*> AddToWorldCells;
		for (const UWorldPartitionRuntimeCell* ActivatedCell : ActivatedCells)
		{
			if (!ActivatedCell->IsAddedToWorld() && !ActivatedCell->IsAlwaysLoaded())
			{
				AddToWorldCells.Add(ActivatedCell);
			}
		}

		SortStreamingCells(StreamingSources, AddToWorldCells, SortedAddToWorldCells);

		// Update level streaming priority so that UWorld::UpdateLevelStreaming will naturally process the levels in the correct order
		const int32 MaxPrio = SortedAddToWorldCells.Num();
		int32 Prio = MaxPrio;
		const ULevel* CurrentPendingVisibility = GetWorld()->GetCurrentLevelPendingVisibility();
		for (const UWorldPartitionRuntimeCell* Cell : SortedAddToWorldCells)
		{
			// Current pending visibility level is the most important
			const bool bIsCellPendingVisibility = CurrentPendingVisibility && (Cell->GetLevel() == CurrentPendingVisibility);
			const int32 SortedPriority = bIsCellPendingVisibility ? MaxPrio + 1 : Prio--;
			Cell->SetStreamingPriority(SortedPriority);
		}
	}

	// Evaluate streaming performance based on cells that should be activated
	UpdateStreamingPerformance(FrameActivateCells);
	
	// Update Epoch if we aren't waiting for clients anymore
	if (bUpdateEpoch)
	{
		DataLayersStatesServerEpoch = AWorldDataLayers::GetDataLayersStateEpoch();
		ContentBundleServerEpoch = FContentBundle::GetContentBundleEpoch();
		ServerStreamingEnabledEpoch = NewServerStreamingEnabledEpoch;
		UE_LOG(LogWorldPartition, Verbose, TEXT("Server epoch updated"));
	}
}

#if !UE_BUILD_SHIPPING
void UWorldPartitionStreamingPolicy::UpdateDebugCellsStreamingPriority(const TSet<const UWorldPartitionRuntimeCell*>& ActivateStreamingCells, const TSet<const UWorldPartitionRuntimeCell*>& LoadStreamingCells)
{
	if (FWorldPartitionDebugHelper::IsRuntimeSpatialHashCellStreamingPriorityShown())
	{
		TSet<const UWorldPartitionRuntimeCell*> CellsToSort;
		CellsToSort.Append(ActivateStreamingCells);
		CellsToSort.Append(LoadStreamingCells);

		TArray<const UWorldPartitionRuntimeCell*> SortedCells;
		SortStreamingCellsByImportance(CellsToSort, SortedCells);

		const int32 CellCount = SortedCells.Num();
		int32 CellPrio = 0;
		for (const UWorldPartitionRuntimeCell* SortedCell : SortedCells)
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

int32 UWorldPartitionStreamingPolicy::GetMaxCellsToLoad() const
{
	// This policy limits the number of concurrent loading streaming cells, except if match hasn't started
	if (!WorldPartition->IsServer())
	{
		return !IsInBlockTillLevelStreamingCompleted() ? (GMaxLoadingStreamingCells - GetCellLoadingCount()) : MAX_int32;
	}

	// Always allow max on server to make sure StreamingLevels are added before clients update the visibility
	return MAX_int32;
}

void UWorldPartitionStreamingPolicy::SetCellsStateToLoaded(const TArray<const UWorldPartitionRuntimeCell*>& ToLoadCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SetCellsStateToLoaded);

	int32 MaxCellsToLoad = GetMaxCellsToLoad();

	// Trigger cell loading. Depending on actual state of cell limit loading.
	for (const UWorldPartitionRuntimeCell* Cell : ToLoadCells)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::LoadCells %s"), *Cell->GetName());

		if (ActivatedCells.Contains(Cell))
		{
			Cell->Deactivate();
			ActivatedCells.Remove(Cell);
			LoadedCells.Add(Cell);
		}
		else if (MaxCellsToLoad > 0)
		{
			Cell->Load();
			LoadedCells.Add(Cell);
			if (!Cell->IsAlwaysLoaded())
			{
				--MaxCellsToLoad;
			}
		}
	}
}

void UWorldPartitionStreamingPolicy::SetCellsStateToActivated(const TArray<const UWorldPartitionRuntimeCell*>& ToActivateCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SetCellsStateToActivated);

	int32 MaxCellsToLoad = GetMaxCellsToLoad();

	// Trigger cell activation. Depending on actual state of cell limit loading.
	for (const UWorldPartitionRuntimeCell* Cell : ToActivateCells)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::ActivateCells %s"), *Cell->GetName());

		if (LoadedCells.Contains(Cell))
		{
			LoadedCells.Remove(Cell);
			ActivatedCells.Add(Cell);
			Cell->Activate();
		}
		else if (MaxCellsToLoad > 0)
		{
			if (!Cell->IsAlwaysLoaded())
			{
				--MaxCellsToLoad;
			}
			ActivatedCells.Add(Cell);
			Cell->Activate();
		}
	}
}

void UWorldPartitionStreamingPolicy::SetCellsStateToUnloaded(const TArray<const UWorldPartitionRuntimeCell*>& ToUnloadCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SetCellsStateToUnloaded);

	for (const UWorldPartitionRuntimeCell* Cell : ToUnloadCells)
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

bool UWorldPartitionStreamingPolicy::CanAddLoadedLevelToWorld(ULevel* InLevel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::CanAddLoadedLevelToWorld);

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

	const UWorldPartitionRuntimeCell** Cell = Algo::FindByPredicate(SortedAddToWorldCells, [InLevel](const UWorldPartitionRuntimeCell* ItCell) { return ItCell->GetLevel() == InLevel; });
	if (Cell && ShouldSkipCellForPerformance(*Cell))
	{
		return false;
	}
 
	return true;
}

bool UWorldPartitionStreamingPolicy::IsStreamingCompleted(const FWorldPartitionStreamingSource* InStreamingSource) const
{
	const UWorld* World = GetWorld();
	check(World);
	check(World->IsGameWorld());
	const UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	const bool bTestProvidedStreamingSource = !!InStreamingSource;

	// Always test non-spatial cells
	{
		// Test non-data layer and activated data layers
		TArray<FWorldPartitionStreamingQuerySource> QuerySources;
		FWorldPartitionStreamingQuerySource& QuerySource = QuerySources.Emplace_GetRef();
		QuerySource.bSpatialQuery = false;
		QuerySource.bDataLayersOnly = false;
		QuerySource.DataLayers = DataLayerSubsystem->GetEffectiveActiveDataLayerNames().Array();
		if (!IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, QuerySources, true))
		{
			return false;
		}

		// Test only loaded data layers
		if (!DataLayerSubsystem->GetEffectiveLoadedDataLayerNames().IsEmpty())
		{
			QuerySource.bDataLayersOnly = true;
			QuerySource.DataLayers = DataLayerSubsystem->GetEffectiveLoadedDataLayerNames().Array();
			if (!IsStreamingCompleted(EWorldPartitionRuntimeCellState::Loaded, QuerySources, true))
			{
				return false;
			}
		}
	}

	// Test spatially loaded cells using streaming sources (or provided streaming source)
	TArrayView<const FWorldPartitionStreamingSource> QueriedStreamingSources = bTestProvidedStreamingSource ? MakeArrayView({ *InStreamingSource }) : StreamingSources;
	for (const FWorldPartitionStreamingSource& StreamingSource : QueriedStreamingSources)
	{
		// Build a query source from a Streaming Source
		TArray<FWorldPartitionStreamingQuerySource> QuerySources;
		FWorldPartitionStreamingQuerySource& QuerySource = QuerySources.Emplace_GetRef();
		QuerySource.bSpatialQuery = true;
		QuerySource.Location = StreamingSource.Location;
		QuerySource.Rotation = StreamingSource.Rotation;
		QuerySource.TargetGrid = StreamingSource.TargetGrid;
		QuerySource.Shapes = StreamingSource.Shapes;
		QuerySource.bUseGridLoadingRange = true;
		QuerySource.Radius = 0.f;
		QuerySource.bDataLayersOnly = false;
		QuerySource.DataLayers = (StreamingSource.TargetState == EStreamingSourceTargetState::Loaded) ? DataLayerSubsystem->GetEffectiveLoadedDataLayerNames().Array() : DataLayerSubsystem->GetEffectiveActiveDataLayerNames().Array();

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
	const UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>();
	const bool bIsHLODEnabled = UHLODSubsystem::IsHLODEnabled();

	bool bResult = true;
	for (const FWorldPartitionStreamingQuerySource& QuerySource : QuerySources)
	{
		WorldPartition->RuntimeHash->ForEachStreamingCellsQuery(QuerySource, [QuerySource, QueryState, bExactState, bIsHLODEnabled, DataLayerSubsystem, &bResult](const UWorldPartitionRuntimeCell* Cell)
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
						if (!QuerySource.DataLayers.Contains(CellDataLayer) && DataLayerSubsystem->GetDataLayerEffectiveRuntimeStateByName(CellDataLayer) > EDataLayerRuntimeState::Unloaded)
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

bool UWorldPartitionStreamingPolicy::DrawRuntimeHash2D(class UCanvas* Canvas, const FVector2D& PartitionCanvasSize, const FVector2D& Offset, FVector2D& OutUsedCanvasSize)
{
	if (StreamingSources.Num() > 0 && WorldPartition->RuntimeHash)
	{
		return WorldPartition->RuntimeHash->Draw2D(Canvas, StreamingSources, PartitionCanvasSize, Offset, OutUsedCanvasSize);
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

/*
 * FStreamingSourceVelocity Implementation
 */

FStreamingSourceVelocity::FStreamingSourceVelocity(const FName& InSourceName)
: SourceName(InSourceName)
, LastIndex(INDEX_NONE)
, LastUpdateTime(-1.0)
, VelocitiesHistorySum(0.f)
{
	VelocitiesHistory.SetNumZeroed(VELOCITY_HISTORY_SAMPLE_COUNT);
}

float FStreamingSourceVelocity::GetAverageVelocity(const FVector& NewPosition, const float CurrentTime)
{
	const double TeleportDistance = 100;
	const float MaxDeltaSeconds = 5.f;
	const bool bIsFirstCall = (LastIndex == INDEX_NONE);
	const float DeltaSeconds = bIsFirstCall ? 0.f : (CurrentTime - LastUpdateTime);
	const double Distance = bIsFirstCall ? 0.f : ((NewPosition - LastPosition) * 0.01).Size();
	if (bIsFirstCall)
	{
		UE_LOG(LogWorldPartition, Log, TEXT("New Streaming Source: %s -> Position: %s"), *SourceName.ToString(), *NewPosition.ToString());
		LastIndex = 0;
	}

	ON_SCOPE_EXIT
	{
		LastUpdateTime = CurrentTime;
		LastPosition = NewPosition;
	};

	// Handle invalid cases
	if (bIsFirstCall || (DeltaSeconds <= 0.f) || (DeltaSeconds > MaxDeltaSeconds) || (Distance > TeleportDistance))
	{
		UE_CLOG(Distance > TeleportDistance, LogWorldPartition, Log, TEXT("Detected Streaming Source Teleport: %s -> Last Position: %s -> New Position: %s"), *SourceName.ToString(), *LastPosition.ToString(), *NewPosition.ToString());
		return 0.f;
	}

	// Compute velocity (m/s)
	check(Distance < MAX_flt);
	const float Velocity = (float)Distance / DeltaSeconds;
	// Update velocities history buffer and sum
	LastIndex = (LastIndex + 1) % VELOCITY_HISTORY_SAMPLE_COUNT;
	VelocitiesHistorySum = FMath::Max<float>(0.f, (VelocitiesHistorySum + Velocity - VelocitiesHistory[LastIndex]));
	VelocitiesHistory[LastIndex] = Velocity;

	// return average
	return (VelocitiesHistorySum / (float)VELOCITY_HISTORY_SAMPLE_COUNT);
}

#undef LOCTEXT_NAMESPACE
