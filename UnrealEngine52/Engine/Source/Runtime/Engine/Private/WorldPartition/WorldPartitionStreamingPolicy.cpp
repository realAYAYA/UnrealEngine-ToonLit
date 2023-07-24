// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * WorldPartitionStreamingPolicy Implementation
 */

#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "Algo/Find.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Misc/ScopeExit.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionReplay.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "Engine/Level.h"
#include "Engine/NetConnection.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionStreamingPolicy)

#if WITH_EDITOR
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

static FString GServerDisallowStreamingOutDataLayersString;
static FAutoConsoleVariableRef CVarServerDisallowStreamingOutDataLayers(
	TEXT("wp.Runtime.ServerDisallowStreamingOutDataLayers"),
	GServerDisallowStreamingOutDataLayersString,
	TEXT("Comma separated list of data layer names that aren't allowed to be unloaded or deactivated on the server"),
	ECVF_ReadOnly);

bool UWorldPartitionStreamingPolicy::IsUpdateOptimEnabled = false;
FAutoConsoleVariableRef UWorldPartitionStreamingPolicy::CVarUpdateOptimEnabled(
	TEXT("wp.Runtime.UpdateStreaming.EnableOptimization"),
	UWorldPartitionStreamingPolicy::IsUpdateOptimEnabled,
	TEXT("Set to 1 to enable an optimization that skips world partition streaming update\n")
	TEXT("if nothing relevant changed since last update."),
	ECVF_Default);

int32 UWorldPartitionStreamingPolicy::LocationQuantization = 400;
FAutoConsoleVariableRef UWorldPartitionStreamingPolicy::CVarLocationQuantization(
	TEXT("wp.Runtime.UpdateStreaming.LocationQuantization"),
	UWorldPartitionStreamingPolicy::LocationQuantization,
	TEXT("Distance (in Unreal units) used to quantize the streaming sources location to determine if a world partition streaming update is necessary."),
	ECVF_Default);

int32 UWorldPartitionStreamingPolicy::RotationQuantization = 10;
FAutoConsoleVariableRef UWorldPartitionStreamingPolicy::CVarRotationQuantization(
	TEXT("wp.Runtime.UpdateStreaming.RotationQuantization"),
	UWorldPartitionStreamingPolicy::RotationQuantization,
	TEXT("Angle (in degrees) used to quantize the streaming sources rotation to determine if a world partition streaming update is necessary."),
	ECVF_Default);

int32 UWorldPartitionStreamingPolicy::ForceUpdateFrameCount = 30;
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
	, bLastUpdateCompletedLoadingAndActivation(false)
	, bCriticalPerformanceRequestedBlockTillOnWorld(false)
	, CriticalPerformanceBlockTillLevelStreamingCompletedEpoch(0)
	, DataLayersStatesServerEpoch(INT_MIN)
	, ContentBundleServerEpoch(INT_MIN)
	, ServerStreamingEnabledEpoch(INT_MIN)
	, UpdateStreamingHash(0)
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
		bool bAllowPlayerControllerStreamingSources = true;

#if WITH_EDITOR
		if (UWorldPartition::IsSimulating())
		{
			// We are in the SIE
			const FVector ViewLocation = WorldToLocal.TransformPosition(GCurrentLevelEditingViewportClient->GetViewLocation());
			const FRotator ViewRotation = WorldToLocal.TransformRotation(GCurrentLevelEditingViewportClient->GetViewRotation().Quaternion()).Rotator();
			static const FName NAME_SIE(TEXT("SIE"));
			StreamingSources.Add(FWorldPartitionStreamingSource(NAME_SIE, ViewLocation, ViewRotation, EStreamingSourceTargetState::Activated, /*bBlockOnSlowLoading=*/false, EStreamingSourcePriority::Default, false));
			bAllowPlayerControllerStreamingSources = false;
		}
#endif
		if (!bIsServer || bIsServerStreamingEnabled || AWorldPartitionReplay::IsRecordingEnabled(World))
		{
			UWorldPartitionSubsystem* WorldPartitionSubsystem = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
			check(WorldPartitionSubsystem);

			TArray<FWorldPartitionStreamingSource> ProviderStreamingSources;
			for (IWorldPartitionStreamingSourceProvider* StreamingSourceProvider : WorldPartitionSubsystem->GetStreamingSourceProviders())
			{
				if (bAllowPlayerControllerStreamingSources || !Cast<APlayerController>(StreamingSourceProvider->GetStreamingSourceOwner()))
				{
					ProviderStreamingSources.Reset();
					if (StreamingSourceProvider->GetStreamingSources(ProviderStreamingSources))
					{
						for (FWorldPartitionStreamingSource& ProviderStreamingSource : ProviderStreamingSources)
						{
							// Transform to Local
							ProviderStreamingSource.Location = WorldToLocal.TransformPosition(ProviderStreamingSource.Location);
							ProviderStreamingSource.Rotation = WorldToLocal.TransformRotation(ProviderStreamingSource.Rotation.Quaternion()).Rotator();
							StreamingSources.Add(MoveTemp(ProviderStreamingSource));
						}
					}
				}
			}
		}
	}

	for (auto& Pair : StreamingSourcesVelocity)
	{
		Pair.Value.Invalidate();
	}

	// Update streaming sources velocity
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	for (FWorldPartitionStreamingSource& StreamingSource : StreamingSources)
	{
		if (!StreamingSource.Name.IsNone())
		{
			FStreamingSourceVelocity& SourceVelocity = StreamingSourcesVelocity.FindOrAdd(StreamingSource.Name, FStreamingSourceVelocity(StreamingSource.Name));
			StreamingSource.Velocity = SourceVelocity.GetAverageVelocity(StreamingSource.Location, CurrentTime);
		}
	}

	// Cleanup StreamingSourcesVelocity
	for (auto It(StreamingSourcesVelocity.CreateIterator()); It; ++It)
	{
		if (!It.Value().IsValid())
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

int32 UWorldPartitionStreamingPolicy::ComputeServerStreamingEnabledEpoch() const
{
	return WorldPartition->IsServer() ? (WorldPartition->IsServerStreamingEnabled() ? 1 : 0) : INT_MIN;
}

bool UWorldPartitionStreamingPolicy::IsUpdateStreamingOptimEnabled()
{
	return UWorldPartitionStreamingPolicy::IsUpdateOptimEnabled &&
		(UWorldPartitionStreamingPolicy::LocationQuantization > 0) &&
		(UWorldPartitionStreamingPolicy::RotationQuantization > 0);
};

uint32 UWorldPartitionStreamingPolicy::ComputeUpdateStreamingHash() const
{
	const UWorldPartitionRuntimeHash* RuntimeHash = WorldPartition->RuntimeHash;
	const bool bForceFrameUpdate = (UWorldPartitionStreamingPolicy::ForceUpdateFrameCount > 0) ? ((UpdateStreamingStateCalls % UWorldPartitionStreamingPolicy::ForceUpdateFrameCount) == 0) : false;

	const bool bCanOptimize = 
		RuntimeHash &&
		IsUpdateStreamingOptimEnabled() &&							// Check CVars to see if optimization is enabled
		!WorldPartition->IsServer() &&								// Don't optimize on the server
		bLastUpdateCompletedLoadingAndActivation &&					// Don't optimize if last frame didn't process all cells to load/activate
		!IsInBlockTillLevelStreamingCompleted() &&					// Don't optimize when inside UWorld::BlockTillLevelStreamingCompleted
		ActivatedCells.GetPendingAddToWorldCells().IsEmpty() &&		// Don't optimize when remaining cells to add to world
		!bForceFrameUpdate;											// We garantee to update every N frame to force some internal updates like UpdateStreamingPerformance

	if (bCanOptimize)
	{
		// Build hash that will be used to detect relevant changes
		uint32 NewHash = GetTypeHash(ComputeServerStreamingEnabledEpoch());
		NewHash = HashCombine(NewHash, GetTypeHash(FContentBundle::GetContentBundleEpoch()));
		NewHash = HashCombine(NewHash, GetTypeHash(AWorldDataLayers::GetDataLayersStateEpoch()));
		NewHash = HashCombine(NewHash, GetTypeHash(RuntimeHash->IsStreaming3D()));
		for (const FWorldPartitionStreamingSource& Source : StreamingSources)
		{
			NewHash = HashCombine(NewHash, ComputeStreamingSourceHash(Source));
		}
		return NewHash;
	}

	return 0;
};

uint32 UWorldPartitionStreamingPolicy::ComputeStreamingSourceHash(const FWorldPartitionStreamingSource& Source) const
{
	uint32 Hash = (GetTypeHash(Source.Name));
	Hash = HashCombine(Hash, GetTypeHash(FMath::FloorToInt(Source.Location.X / UWorldPartitionStreamingPolicy::LocationQuantization)));
	Hash = HashCombine(Hash, GetTypeHash(FMath::FloorToInt(Source.Location.Y / UWorldPartitionStreamingPolicy::LocationQuantization)));
	Hash = HashCombine(Hash, GetTypeHash(FMath::FloorToInt(Source.Rotation.Yaw / UWorldPartitionStreamingPolicy::RotationQuantization)));
	// Only consider Z position and pitch/roll rotations when hash is streaming in 3D
	if (WorldPartition->RuntimeHash && WorldPartition->RuntimeHash->IsStreaming3D())
	{
		Hash = HashCombine(Hash, GetTypeHash(FMath::FloorToInt(Source.Location.Z / UWorldPartitionStreamingPolicy::LocationQuantization)));
		Hash = HashCombine(Hash, GetTypeHash(FMath::FloorToInt(Source.Rotation.Pitch / UWorldPartitionStreamingPolicy::RotationQuantization)));
		Hash = HashCombine(Hash, GetTypeHash(FMath::FloorToInt(Source.Rotation.Roll / UWorldPartitionStreamingPolicy::RotationQuantization)));
	}
	Hash = HashCombine(Hash, GetTypeHash(Source.TargetState));
	Hash = HashCombine(Hash, GetTypeHash(Source.bBlockOnSlowLoading));
	Hash = HashCombine(Hash, GetTypeHash(Source.bReplay));
	Hash = HashCombine(Hash, GetTypeHash(Source.bRemote));
	Hash = HashCombine(Hash, GetTypeHash(Source.Priority));
	Hash = HashCombine(Hash, GetTypeHash(Source.TargetBehavior));
	for (FName TargetGrid : Source.TargetGrids)
	{
		Hash = HashCombine(Hash, GetTypeHash(TargetGrid));
	}
	for (const FSoftObjectPath& TargetHLODLayer : Source.TargetHLODLayers)
	{
		Hash = HashCombine(Hash, GetTypeHash(TargetHLODLayer));
	}
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Hash = HashCombine(Hash, GetTypeHash(Source.TargetGrid));
	Hash = HashCombine(Hash, GetTypeHash(Source.TargetHLODLayer));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	for (const FStreamingSourceShape& Shape : Source.Shapes)
	{
		Hash = HashCombine(Hash, GetTypeHash(Shape));
	}
	return Hash;
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
		const bool bCanUseSortingCache = false;
		int32 SortCompare = CellA->SortCompare(CellB, bCanUseSortingCache);
		if (SortCompare == 0)
		{
			// Closest distance (lower value is higher prio)
			const double Diff = QueryCache.GetCellMinSquareDist(CellA) - QueryCache.GetCellMinSquareDist(CellB);
			if (FMath::IsNearlyZero(Diff))
			{
				return CellA->GetLevelPackageName().LexicalLess(CellB->GetLevelPackageName());
			}
			else
			{
				return Diff < 0;
			}
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

			if (const UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
			{
				for (const FString& DataLayerAssetName : AllDLAssetsStrings)
				{
					if (const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstanceFromAssetName(FName(DataLayerAssetName)))
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

	// Dermine if the World's BlockTillLevelStreamingCompleted was triggered by WorldPartitionStreamingPolicy
	if (bCriticalPerformanceRequestedBlockTillOnWorld && IsInBlockTillLevelStreamingCompleted())
	{
		bCriticalPerformanceRequestedBlockTillOnWorld = false;
		CriticalPerformanceBlockTillLevelStreamingCompletedEpoch = World->GetBlockTillLevelStreamingCompletedEpoch();
	}
	
	// Update streaming sources
	UpdateStreamingSources();

	// Detect if nothing relevant changed and early out
	const uint32 NewUpdateStreamingHash = ComputeUpdateStreamingHash();
	const bool bShouldSkipUpdate = NewUpdateStreamingHash && (UpdateStreamingHash == NewUpdateStreamingHash);
	if (bShouldSkipUpdate)
	{
		return;
	}

	// Update new streaming sources hash
	UpdateStreamingHash = NewUpdateStreamingHash;

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

	const bool bCanStream = WorldPartition->CanStream();
	const bool bIsServer = WorldPartition->IsServer();
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

			auto CanServerDeactivateOrUnloadDataLayerCell = [&ServerDisallowStreamingOutDataLayers](const UWorldPartitionRuntimeCell* Cell)
			{
				if (Cell->HasDataLayers())
				{
					if (Cell->HasAnyDataLayer(ServerDisallowStreamingOutDataLayers))
					{
						return false;
					}
				}

				return true;
			};

			auto AddServerFrameCell = [this, CanServerDeactivateOrUnloadDataLayerCell, &EffectiveLoadedDataLayerNames, &EffectiveActiveDataLayerNames](const UWorldPartitionRuntimeCell* Cell)
			{
				// Keep Data Layer cells in their current state if server cannot deactivate/unload data layer cells
				if (!CanServerDeactivateOrUnloadDataLayerCell(Cell))
				{
					if (ActivatedCells.Contains(Cell))
					{
						FrameActivateCells.Add(Cell);
						return;
					}
					else
					{
						// Allow a cell with data layer(s) to switch from loaded to activated. Do not early return here in that case.
						const bool bIsAnActivatedDataLayerCell = EffectiveActiveDataLayerNames.Num() && Cell->HasAnyDataLayer(EffectiveActiveDataLayerNames);
						if (LoadedCells.Contains(Cell) && !bIsAnActivatedDataLayerCell)
						{
							FrameLoadCells.Add(Cell);
							return;
						}
					}
				}
				
				// Non Data Layer Cells + Active Data Layers
				if (!Cell->HasDataLayers() || (EffectiveActiveDataLayerNames.Num() && Cell->HasAnyDataLayer(EffectiveActiveDataLayerNames)))
				{
					FrameActivateCells.Add(Cell);
				}
				// Loaded Data Layers Cells only
				else if (Cell->HasDataLayers() && EffectiveLoadedDataLayerNames.Num() && Cell->HasAnyDataLayer(EffectiveLoadedDataLayerNames))
				{
					FrameLoadCells.Add(Cell);
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

	// Activation superseeds Loading
	FrameLoadCells = FrameLoadCells.Difference(FrameActivateCells);

	// Determine cells to activate (sorted by importance)
	TArray<const UWorldPartitionRuntimeCell*> ToActivateCells;
	if (bCanStream)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ToActivateCells);
		for (const UWorldPartitionRuntimeCell* Cell : FrameActivateCells)
		{
			if (!ActivatedCells.Contains(Cell) && !ShouldSkipCellForPerformance(Cell) && !ShouldSkipDisabledHLODCell(Cell))
			{
				Cell->MergeStreamingSourceInfo();
				ToActivateCells.Add(Cell);
			}
		}
		SortStreamingCellsByImportance(ToActivateCells);
	}

	// Determine cells to load (sorted by importance)
	TArray<const UWorldPartitionRuntimeCell*> ToLoadCells;
	if (bCanStream)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ToLoadCells);
		for (const UWorldPartitionRuntimeCell* Cell : FrameLoadCells)
		{
			if (!LoadedCells.Contains(Cell))
			{
				// Only deactivated server cells need to call ShouldWaitForClientVisibility (those part of ActivatedCells)
				const bool bIsServerCellToDeactivate = bIsServer && ActivatedCells.Contains(Cell);
				const bool bShoudSkipServerCell = bIsServerCellToDeactivate && ShouldWaitForClientVisibility(Cell);
				if (!bShoudSkipServerCell && !ShouldSkipCellForPerformance(Cell) && !ShouldSkipDisabledHLODCell(Cell))
				{
					Cell->MergeStreamingSourceInfo();
					ToLoadCells.Add(Cell);
				}
			}
		}
		SortStreamingCellsByImportance(ToLoadCells);
	}

	// Determine cells to unload
	TArray<const UWorldPartitionRuntimeCell*> ToUnloadCells;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::UpdateStreamingState_ToUnloadCells);
		auto BuildCellsToUnload = [this, &ToUnloadCells, bCanStream, bIsServer, ShouldWaitForClientVisibility](const TSet<const UWorldPartitionRuntimeCell*>& InCells)
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

	int32 ToLoadAndActivateCount = 0;

	if (bIsStreamingInEnabled)
	{
		// Do Activation State first as it is higher prio than Load State (if we have a limited number of loading cells per frame)
		if (ToActivateCells.Num() > 0)
		{
			ToLoadAndActivateCount += SetCellsStateToActivated(ToActivateCells);
		}

		if (ToLoadCells.Num() > 0)
		{
			ToLoadAndActivateCount += SetCellsStateToLoaded(ToLoadCells);
		}
	}

	bLastUpdateCompletedLoadingAndActivation = (ToLoadAndActivateCount == (ToActivateCells.Num() + ToLoadCells.Num()));

	// Sort cells and update streaming priority 
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SortCellsAndUpdateStreamingPriority);
		SortedAddToWorldCells = ActivatedCells.GetPendingAddToWorldCells().Array();
		SortStreamingCellsByImportance(SortedAddToWorldCells);

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
		TArray<const UWorldPartitionRuntimeCell*> Cells = ActivateStreamingCells.Array();
		Cells.Append(LoadStreamingCells.Array());

		for (const UWorldPartitionRuntimeCell* Cell : Cells)
		{
			Cell->RuntimeCellData->MergeStreamingSourceInfo();
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

int32 UWorldPartitionStreamingPolicy::SetCellsStateToLoaded(const TArray<const UWorldPartitionRuntimeCell*>& ToLoadCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SetCellsStateToLoaded);

	int32 MaxCellsToLoad = GetMaxCellsToLoad();

	int32 LoadedCount = 0;

	// Trigger cell loading. Depending on actual state of cell limit loading.
	for (const UWorldPartitionRuntimeCell* Cell : ToLoadCells)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::LoadCells %s"), *Cell->GetName());

		if (ActivatedCells.Contains(Cell))
		{
			Cell->Deactivate();
			ActivatedCells.Remove(Cell);
			LoadedCells.Add(Cell);
			++LoadedCount;
		}
		else if (MaxCellsToLoad > 0)
		{
			Cell->Load();
			LoadedCells.Add(Cell);
			++LoadedCount;
			if (!Cell->IsAlwaysLoaded())
			{
				--MaxCellsToLoad;
			}
		}
	}

	return LoadedCount;
}

int32 UWorldPartitionStreamingPolicy::SetCellsStateToActivated(const TArray<const UWorldPartitionRuntimeCell*>& ToActivateCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionStreamingPolicy::SetCellsStateToActivated);

	int32 MaxCellsToLoad = GetMaxCellsToLoad();
	
	int32 ActivatedCount = 0;
	// Trigger cell activation. Depending on actual state of cell limit loading.
	for (const UWorldPartitionRuntimeCell* Cell : ToActivateCells)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("UWorldPartitionStreamingPolicy::ActivateCells %s"), *Cell->GetName());

		if (LoadedCells.Contains(Cell))
		{
			LoadedCells.Remove(Cell);
			ActivatedCells.Add(Cell);
			Cell->Activate();
			++ActivatedCount;
		}
		else if (MaxCellsToLoad > 0)
		{
			if (!Cell->IsAlwaysLoaded())
			{
				--MaxCellsToLoad;
			}
			ActivatedCells.Add(Cell);
			Cell->Activate();
			++ActivatedCount;
		}
	}
	return ActivatedCount;
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

bool UWorldPartitionStreamingPolicy::IsStreamingCompleted(const TArray<FWorldPartitionStreamingSource>* InStreamingSources) const
{
	const UWorld* World = GetWorld();
	check(World);
	check(World->IsGameWorld());
	const UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
	const bool bTestProvidedStreamingSource = !!InStreamingSources;

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
		QuerySource.TargetHLODLayers = StreamingSource.TargetHLODLayers;
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

void UWorldPartitionStreamingPolicy::OnCellShown(const UWorldPartitionRuntimeCell* InCell)
{
	ActivatedCells.OnAddedToWorld(InCell);
}

void UWorldPartitionStreamingPolicy::OnCellHidden(const UWorldPartitionRuntimeCell* InCell)
{
	ActivatedCells.OnRemovedFromWorld(InCell);
}

void UWorldPartitionStreamingPolicy::FActivatedCells::Add(const UWorldPartitionRuntimeCell* InCell)
{
	Cells.Add(InCell);
	if (!InCell->IsAlwaysLoaded())
	{
		PendingAddToWorldCells.Add(InCell);
	}
}

void UWorldPartitionStreamingPolicy::FActivatedCells::Remove(const UWorldPartitionRuntimeCell* InCell)
{
	Cells.Remove(InCell);
	PendingAddToWorldCells.Remove(InCell);
}

void UWorldPartitionStreamingPolicy::FActivatedCells::OnAddedToWorld(const UWorldPartitionRuntimeCell* InCell)
{
	PendingAddToWorldCells.Remove(InCell);
}

void UWorldPartitionStreamingPolicy::FActivatedCells::OnRemovedFromWorld(const UWorldPartitionRuntimeCell* InCell)
{
	if (Cells.Contains(InCell))
	{
		if (!InCell->IsAlwaysLoaded())
		{
			PendingAddToWorldCells.Add(InCell);
		}
	}
}

/*
 * FStreamingSourceVelocity Implementation
 */

FStreamingSourceVelocity::FStreamingSourceVelocity(const FName& InSourceName)
: bIsValid(false)
, SourceName(InSourceName)
, LastIndex(INDEX_NONE)
, LastUpdateTime(-1.0)
, VelocitiesHistorySum(0.f)
{
	VelocitiesHistory.SetNumZeroed(VELOCITY_HISTORY_SAMPLE_COUNT);
}

float FStreamingSourceVelocity::GetAverageVelocity(const FVector& NewPosition, const float CurrentTime)
{
	bIsValid = true;

	const double TeleportDistance = 100;
	const float MaxDeltaSeconds = 5.f;
	const bool bIsFirstCall = (LastIndex == INDEX_NONE);
	const float DeltaSeconds = bIsFirstCall ? 0.f : (CurrentTime - LastUpdateTime);
	const double Distance = bIsFirstCall ? 0.f : ((NewPosition - LastPosition) * 0.01).Size();
	if (bIsFirstCall)
	{
		UE_LOG(LogWorldPartition, Verbose, TEXT("New Streaming Source: %s -> Position: %s"), *SourceName.ToString(), *NewPosition.ToString());
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
		UE_CLOG(Distance > TeleportDistance, LogWorldPartition, Verbose, TEXT("Detected Streaming Source Teleport: %s -> Last Position: %s -> New Position: %s"), *SourceName.ToString(), *LastPosition.ToString(), *NewPosition.ToString());
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
