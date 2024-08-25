// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	World.cpp: UWorld implementation
=============================================================================*/

#include "Engine/World.h"

#include "Engine/ChildConnection.h"
#include "Engine/GameInstance.h"
#include "Engine/CollisionProfile.h"
#include "Engine/PawnIterator.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Engine/PendingNetGame.h"
#include "Misc/Paths.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Particles/ParticleSystemComponent.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopedSlowTask.h"
#include "SceneInterface.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectRedirector.h"
#include "SceneView.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectAnnotation.h"
#include "Misc/PackageName.h"
#include "GameMapsSettings.h"
#include "TimerManager.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Model.h"
#include "Engine/LevelBounds.h"
#include "UObject/MetaData.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Engine/Canvas.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "RendererInterface.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingGCHelper.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Engine/LocalPlayer.h"
#include "ComponentReregisterContext.h"
#include "UnrealEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/CullDistanceVolume.h"
#include "Engine/Console.h"
#include "Engine/WorldComposition.h"
#include "ExternalPackageHelper.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/NetworkProfiler.h"
#include "TickTaskManagerInterface.h"
#include "FXSystem.h"
#include "AudioDevice.h"
#include "VisualLogger/VisualLogger.h"
#include "LevelUtils.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "AI/AISystemBase.h"
#include "Camera/CameraActor.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/GameStateBase.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Physics/Experimental/ChaosEventRelay.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "Components/ModelComponent.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/DemoNetDriver.h"
#include "VT/LightmapVirtualTexture.h" 
#include "Materials/MaterialParameterCollectionInstance.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Streaming/ServerStreamingLevelsVisibility.h"
#include "Streaming/LevelStreamingDelegates.h"

#if WITH_EDITOR
	#include "DerivedDataCacheInterface.h"
	#include "ThumbnailRendering/WorldThumbnailInfo.h"
	#include "Editor/UnrealEdTypes.h"
	#include "HierarchicalLOD.h"
	#include "IHierarchicalLODUtilities.h"
	#include "HierarchicalLODUtilitiesModule.h"
	#include "ObjectTools.h"
	#include "Engine/LODActor.h"
	#include "PIEPreviewDeviceProfileSelectorModule.h"
	#include "StaticMeshCompiler.h"
	#include "WorldPartition/DataLayer/WorldDataLayers.h"
	#include "PieFixupSerializer.h"
	#include "ActorFolder.h"
	#include "ActorDeferredScriptManager.h"
	#include "AssetCompilingManager.h"
#endif


#include "PhysicsField/PhysicsFieldComponent.h"
#include "EngineModule.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Net/DataChannel.h"
#include "Engine/LevelStreamingPersistent.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "Engine/ShadowMapTexture2D.h"
#include "Components/LineBatchComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/LightMapTexture2D.h"
#include "UObject/UObjectThreadContext.h"
#include "Engine/CoreSettings.h"
#include "Net/PerfCountersHelpers.h"
#include "InGamePerformanceTracker.h"
#include "Engine/AssetManager.h"
#include "Engine/HLODProxy.h"
#include "MoviePlayerProxy.h"
#include "ObjectTrace.h"
#include "ReplaySubsystem.h"
#include "Net/NetPing.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Net/Iris/ReplicationSystem/ActorReplicationBridge.h"
#endif // UE_WITH_IRIS

#include "ChaosSolversModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(World)

DEFINE_LOG_CATEGORY_STATIC(LogWorld, Log, All);
DEFINE_LOG_CATEGORY(LogSpawn);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);
CSV_DEFINE_CATEGORY(LevelStreamingAdaptive, true);
CSV_DEFINE_CATEGORY(LevelStreamingAdaptiveDetail, false);
CSV_DEFINE_CATEGORY(LevelStreamingDetail, false);
CSV_DEFINE_CATEGORY(LevelStreamingPendingPurge, (!UE_BUILD_SHIPPING));

#define LOCTEXT_NAMESPACE "World"

static int32 bDisableRemapScriptActors = 0;
FAutoConsoleVariableRef CVarDisableRemapScriptActors(TEXT("net.DisableRemapScriptActors"), bDisableRemapScriptActors, TEXT("When set, disables name remapping of compiled script actors (for networking)"));

static bool bDisableInGamePerfTrackersForUninitializedWorlds = true;
FAutoConsoleVariableRef CVarDisableInGamePerfTrackersForUninitializedWorlds(TEXT("s.World.SkipPerfTrackerForUninitializedWorlds"), bDisableInGamePerfTrackersForUninitializedWorlds, TEXT("When set, disables allocation of InGamePerformanceTrackers for Worlds that aren't initialized."));


static TAutoConsoleVariable<int32> CVarPurgeEditorSceneDuringPIE(
	TEXT("r.PurgeEditorSceneDuringPIE"),
	0,
	TEXT("0 to keep editor scene fully initialized during PIE (default)\n")
	TEXT("1 to purge editor scene from memory during PIE and restore when the session finishes."));

/*-----------------------------------------------------------------------------
	FAdaptiveAddToWorld implementation.
-----------------------------------------------------------------------------*/
static int32 GAdaptiveAddToWorldEnabled = 0;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldEnabled(TEXT("s.AdaptiveAddToWorld.Enabled"), GAdaptiveAddToWorldEnabled, 
	TEXT("Enables the adaptive AddToWorld timeslice (replaces s.LevelStreamingActorsUpdateTimeLimit) (default: off)"));

static int32 GAdaptiveAddToWorldMethod = 1;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldMethod(TEXT("s.AdaptiveAddToWorld.Method"), GAdaptiveAddToWorldMethod,
	TEXT("Heuristic to use for the adaptive timeslice\n")
	TEXT("0 - compute the target timeslice based on remaining work time\n")
	TEXT("1 - compute the target timeslice based on total work time for levels in flight (this avoids slowing before a level completes)\n"));

static float GAdaptiveAddToWorldTimeSliceMin = 1.0f;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldTimeSliceMin(TEXT("s.AdaptiveAddToWorld.AddToWorldTimeSliceMin"), GAdaptiveAddToWorldTimeSliceMin, 
	TEXT("Minimum adaptive AddToWorld timeslice"));

static float GAdaptiveAddToWorldTimeSliceMax = 6.0f;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldTimeSliceMax(TEXT("s.AdaptiveAddToWorld.AddToWorldTimeSliceMax"), GAdaptiveAddToWorldTimeSliceMax, 
	TEXT("Maximum adaptive AddToWorld timeslice"));

static float GAdaptiveAddToWorldTargetMaxTimeRemaining = 6.0f;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldTargetMaxTimeRemaining(TEXT("s.AdaptiveAddToWorld.TargetMaxTimeRemaining"), GAdaptiveAddToWorldTargetMaxTimeRemaining, 
	TEXT("Target max time remaining in seconds. If our estimated completion time is longer than this, the timeslice will increase. Lower values are more aggressive"));

static float GAdaptiveAddToWorldTimeSliceMaxIncreasePerSecond = 0.0f;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldTimeSliceMaxIncreasePerSecond(TEXT("s.AdaptiveAddToWorld.TimeSliceMaxIncreasePerSecond"), GAdaptiveAddToWorldTimeSliceMaxIncreasePerSecond,
	TEXT("Max rate at which the adptive AddToWorld timeslice will increase. Set to 0 to increase instantly"));

static float GAdaptiveAddToWorldTimeSliceMaxReducePerSecond = 0.0f;
FAutoConsoleVariableRef CVarAdaptiveAddToWorldTimesliceReductionSpeedPerSecond(TEXT("s.AdaptiveAddToWorld.TimeSliceMaxReducePerSecond"), GAdaptiveAddToWorldTimeSliceMaxReducePerSecond,
	TEXT("Max rate at which the adptive AddToWorld timeslice will reduce. Set to 0 to reduce instantly"));

class FAdaptiveAddToWorld
{
	struct FHistoryEntry
	{
		float DeltaT = 0.0f;
		float TimeSlice = 0.0f;
		int32 WorkUnitsProcessed = 0;
	};
public:
	FAdaptiveAddToWorld()
	{
	}

	bool IsEnabled() const
	{
		return bEnabled;
	} 

	void SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	// Called when level streaming update begins. 
	// Note: multiple updates per frame are supported, e.g for multiple views/splitscreen. Each update is counted as a separate history entry
	void BeginUpdate()
	{
		if (bEnabled)
		{
		    WorkUnitsProcessedThisUpdate = 0;
		    TotalWorkUnitsRemaining = 0;
			TotalWorkUnitsForLevelsInFlight = 0;
		}
	}

	// Called when level streaming update ends 
	void EndUpdate()
	{
		if ( !bEnabled )
		{
			return;
		}
		QUICK_SCOPE_CYCLE_COUNTER(STAT_LevelStreamingAdaptiveUpdate);
		CSV_SCOPED_TIMING_STAT(LevelStreamingAdaptiveDetail, Update);
		// Only count frames where we have a reasonable number of work units to process
		double CurrentTimestamp = FPlatformTime::Seconds();
		float DeltaT = float(CurrentTimestamp - LastUpdateTimestamp);
		// Only consider frames where we're using the full timeslice
		if (TotalWorkUnitsRemaining > 0)
		{
			FHistoryEntry& HistoryEntry = UpdateHistory[HistoryFrameIndex];
			HistoryEntry.WorkUnitsProcessed = WorkUnitsProcessedThisUpdate;
			HistoryEntry.DeltaT = DeltaT;
			HistoryEntry.TimeSlice = GetTimeSlice();
			HistoryFrameIndex = (HistoryFrameIndex + 1) % HistorySize;

			// Make sure we have a complete history buffer before calculating
			if (ValidFrameCount < HistorySize)
			{
				BaseTimeSlice = GAdaptiveAddToWorldTimeSliceMin;
				ValidFrameCount++;
			}
			else
			{
				// Compute the rolling averages
				int32 TotalWorkUnits = 0;
				float TotalTime = 0.0f;
				float TotalTimeSliceMs = 0.0f;
				for (int i = 0; i < HistorySize; i++)
				{
					TotalWorkUnits += UpdateHistory[i].WorkUnitsProcessed;
					TotalTime += UpdateHistory[i].DeltaT;
					TotalTimeSliceMs += UpdateHistory[i].TimeSlice;
				}

				float AverageTimeSliceMs = TotalTimeSliceMs / float(HistorySize);

				float AverageWorkUnitsPerSecondWith1MsTimeSlice = (float)TotalWorkUnits / ( TotalTime * AverageTimeSliceMs );
				CSV_CUSTOM_STAT(LevelStreamingAdaptiveDetail, AverageWorkUnitsPerSecondWith1MsTimeSlice, AverageWorkUnitsPerSecondWith1MsTimeSlice, ECsvCustomStatOp::Set);

				// Work out what the timeslice should be
				float TargetTimeSlice = 0.0f;
				if (GAdaptiveAddToWorldMethod == 1)
				{
					float EstimatedTotalTimeForCurrentLevelsWith1MsTimeSlice = (float)TotalWorkUnitsForLevelsInFlight / AverageWorkUnitsPerSecondWith1MsTimeSlice;
					CSV_CUSTOM_STAT(LevelStreamingAdaptiveDetail, EstimatedTotalTimeForCurrentLevelsWith1MsTimeSlice, EstimatedTotalTimeForCurrentLevelsWith1MsTimeSlice, ECsvCustomStatOp::Set);
					TargetTimeSlice = EstimatedTotalTimeForCurrentLevelsWith1MsTimeSlice / GAdaptiveAddToWorldTargetMaxTimeRemaining - ExtraTimeSlice;
				}
				else
				{
					float EstimatedTimeRemainingWith1MsTimeSlice = (float)TotalWorkUnitsRemaining / AverageWorkUnitsPerSecondWith1MsTimeSlice;
					CSV_CUSTOM_STAT(LevelStreamingAdaptiveDetail, EstimatedTimeRemainingWith1MsTimeSlice, EstimatedTimeRemainingWith1MsTimeSlice, ECsvCustomStatOp::Set);
					TargetTimeSlice = EstimatedTimeRemainingWith1MsTimeSlice / GAdaptiveAddToWorldTargetMaxTimeRemaining - ExtraTimeSlice;
				}

				float DesiredBaseTimeSlice = FMath::Clamp(TargetTimeSlice, GAdaptiveAddToWorldTimeSliceMin, GAdaptiveAddToWorldTimeSliceMax);

				// Limit the timeslice change based on MaxIncreasePerSecond/ MaxReducePerSecond
				float NewBaseTimeSlice = DesiredBaseTimeSlice;
				if (DesiredBaseTimeSlice > BaseTimeSlice && GAdaptiveAddToWorldTimeSliceMaxIncreasePerSecond > 0.0)
				{
					NewBaseTimeSlice = FMath::Min(NewBaseTimeSlice, BaseTimeSlice + GAdaptiveAddToWorldTimeSliceMaxIncreasePerSecond * DeltaT);
				}
				if (DesiredBaseTimeSlice < BaseTimeSlice && GAdaptiveAddToWorldTimeSliceMaxReducePerSecond > 0.0)
				{
					NewBaseTimeSlice = FMath::Max(NewBaseTimeSlice, BaseTimeSlice - GAdaptiveAddToWorldTimeSliceMaxReducePerSecond * DeltaT);
				}
				BaseTimeSlice = NewBaseTimeSlice;
			}
		}
		else
		{
			BaseTimeSlice = GAdaptiveAddToWorldTimeSliceMin;
		}

		CSV_CUSTOM_STAT(LevelStreamingAdaptiveDetail, WorkUnitsProcessedPerSecond, float(WorkUnitsProcessedThisUpdate)/DeltaT, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(LevelStreamingAdaptiveDetail, WorkUnitsProcessedThisUpdate, WorkUnitsProcessedThisUpdate, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(LevelStreamingAdaptive, TimeSlice, GetTimeSlice(), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(LevelStreamingAdaptive, WorkUnitsRemaining, TotalWorkUnitsRemaining, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(LevelStreamingAdaptive, TotalWorkUnitsForLevelsInFlight, TotalWorkUnitsForLevelsInFlight, ECsvCustomStatOp::Set);
		LastUpdateTimestamp = CurrentTimestamp;
	}

	void RegisterAddToWorldWork(int32 StartWorkUnits, int32 EndWorkUnits, int32 TotalWorkUnits)
	{
		WorkUnitsProcessedThisUpdate += StartWorkUnits - EndWorkUnits;
		TotalWorkUnitsRemaining += EndWorkUnits;
		TotalWorkUnitsForLevelsInFlight += TotalWorkUnits;
	}

	float GetTimeSlice() const
	{
		return ExtraTimeSlice + BaseTimeSlice;
	}

	void SetExtraTimeSlice(float InExtraTimeSlice)
	{
		ExtraTimeSlice = InExtraTimeSlice;
	}


private:
	static const int32 HistorySize = 16;

	int32 WorkUnitsProcessedThisUpdate = 0;
	int32 TotalWorkUnitsRemaining = 0;
	int32 TotalWorkUnitsForLevelsInFlight = 0;
	int32 HistoryFrameIndex = 0;
	FHistoryEntry UpdateHistory[HistorySize];
	float BaseTimeSlice = 1.0f;
	float ExtraTimeSlice = 0.0f;
	double LastUpdateTimestamp = 0.0;
	int32 ValidFrameCount = 0;
    bool bEnabled = false;
};

static FAdaptiveAddToWorld GAdaptiveAddToWorld;


template<class Function>
static void ForEachNetDriver(UEngine* Engine, const UWorld* const World, const Function InFunction)
{
	if (Engine == nullptr || World == nullptr)
	{
		return;
	}

	FWorldContext* const Context = Engine->GetWorldContextFromWorld(World);
	if (Context != nullptr)
	{
		for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
		{
			InFunction(Driver.NetDriver);
		}
	}
}

FActorSpawnParameters::FActorSpawnParameters()
: Name(NAME_None)
, Template(NULL)
, Owner(NULL)
, Instigator(NULL)
, OverrideLevel(NULL)
#if WITH_EDITOR
, OverridePackage(nullptr)
#endif
, OverrideParentComponent(nullptr)
, SpawnCollisionHandlingOverride(ESpawnActorCollisionHandlingMethod::Undefined)
, bRemoteOwned(false)
, bNoFail(false)
, bDeferConstruction(false)
, bAllowDuringConstructionScript(false)
#if !WITH_EDITOR
, bForceGloballyUniqueName(false)
#else
, bTemporaryEditorActor(false)
, bHideFromSceneOutliner(false)
, bCreateActorPackage(true)
#endif
, NameMode(ESpawnActorNameMode::Required_Fatal)
, ObjectFlags(RF_Transactional)
{
}

FLevelCollection::FLevelCollection()
	: CollectionType(ELevelCollectionType::DynamicSourceLevels)
	, bIsVisible(true)
	, GameState(nullptr)
	, NetDriver(nullptr)
	, DemoNetDriver(nullptr)
	, PersistentLevel(nullptr)
{
}

FLevelCollection::~FLevelCollection()
{
	for (ULevel* Level : Levels)
	{
		if (Level)
		{
			check(Level->GetCachedLevelCollection() == this);
			Level->SetCachedLevelCollection(nullptr);
		}
	}
}

FLevelCollection::FLevelCollection(FLevelCollection&& Other)
	: CollectionType(Other.CollectionType)
	, bIsVisible(Other.bIsVisible)
	, GameState(Other.GameState)
	, NetDriver(Other.NetDriver)
	, DemoNetDriver(Other.DemoNetDriver)
	, PersistentLevel(Other.PersistentLevel)
	, Levels(MoveTemp(Other.Levels))
{
	for (ULevel* Level : Levels)
	{
		if (Level)
		{
			check(Level->GetCachedLevelCollection() == &Other);
			Level->SetCachedLevelCollection(this);
		}
	}
}

FLevelCollection& FLevelCollection::operator=(FLevelCollection&& Other)
{
	if (this != &Other)
	{
		CollectionType = Other.CollectionType;
		GameState = Other.GameState;
		NetDriver = Other.NetDriver;
		DemoNetDriver = Other.DemoNetDriver;
		PersistentLevel = Other.PersistentLevel;
		Levels = MoveTemp(Other.Levels);
		bIsVisible = Other.bIsVisible;

		for (ULevel* Level : Levels)
		{
			if (Level)
			{
				check(Level->GetCachedLevelCollection() == &Other);
				Level->SetCachedLevelCollection(this);
			}
		}
	}

	return *this;
}

void FLevelCollection::SetPersistentLevel(ULevel* const Level)
{
	PersistentLevel = Level;
}

void FLevelCollection::AddLevel(ULevel* const Level)
{
	if (Level)
	{
		// Sanity check that Level isn't already in another collection.
		check(Level->GetCachedLevelCollection() == nullptr);
		Levels.Add(Level);
		Level->SetCachedLevelCollection(this);
	}
}

void FLevelCollection::RemoveLevel(ULevel* const Level)
{
	if (Level)
	{
		check(Level->GetCachedLevelCollection() == this);
		Level->SetCachedLevelCollection(nullptr);
		Levels.Remove(Level);
	}
}

FScopedLevelCollectionContextSwitch::FScopedLevelCollectionContextSwitch(const FLevelCollection* const InLevelCollection, UWorld* const InWorld)
	: World(InWorld)
	, SavedTickingCollectionIndex(InWorld ? InWorld->GetActiveLevelCollectionIndex() : INDEX_NONE)
{
	if (World)
	{
		const int32 FoundIndex = World->GetLevelCollections().IndexOfByPredicate([InLevelCollection](const FLevelCollection& Collection)
		{
			return &Collection == InLevelCollection;
		});

		World->SetActiveLevelCollection(FoundIndex);
	}
}

FScopedLevelCollectionContextSwitch::FScopedLevelCollectionContextSwitch(int32 InLevelCollectionIndex, UWorld* const InWorld)
	: World(InWorld)
	, SavedTickingCollectionIndex(InWorld ? InWorld->GetActiveLevelCollectionIndex() : INDEX_NONE)
{
	if (World)
	{
		World->SetActiveLevelCollection(InLevelCollectionIndex);
	}
}

FScopedLevelCollectionContextSwitch::~FScopedLevelCollectionContextSwitch()
{
	if (World)
	{
		World->SetActiveLevelCollection(SavedTickingCollectionIndex);
	}
}

void FWorldPartitionEvents::BroadcastWorldPartitionInitialized(UWorld* InWorld, UWorldPartition* InWorldPartition)
{
	check(InWorld);
	InWorld->BroadcastWorldPartitionInitialized(InWorldPartition);
}

void FWorldPartitionEvents::BroadcastWorldPartitionUninitialized(UWorld* InWorld, UWorldPartition* InWorldPartition)
{
	check(InWorld);
	InWorld->BroadcastWorldPartitionUninitialized(InWorldPartition);
}


FAudioDeviceWorldDelegates::FOnWorldRegisteredToAudioDevice FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice;

FAudioDeviceWorldDelegates::FOnWorldUnregisteredWithAudioDevice FAudioDeviceWorldDelegates::OnWorldUnregisteredWithAudioDevice;

/*-----------------------------------------------------------------------------
	UWorld implementation.
-----------------------------------------------------------------------------*/

/** Global world pointer */
UWorldProxy GWorld;

TMap<FName, EWorldType::Type> UWorld::WorldTypePreLoadMap;

FWorldDelegates::FWorldEvent FWorldDelegates::OnPostWorldCreation;
FWorldDelegates::FWorldInitializationEvent FWorldDelegates::OnPreWorldInitialization;
FWorldDelegates::FWorldInitializationEvent FWorldDelegates::OnPostWorldInitialization;
#if WITH_EDITOR
FWorldDelegates::FWorldPreRenameEvent FWorldDelegates::OnPreWorldRename;
FWorldDelegates::FWorldPostRenameEvent FWorldDelegates::OnPostWorldRename;
FWorldDelegates::FWorldCurrentLevelChangedEvent FWorldDelegates::OnCurrentLevelChanged;
FWorldDelegates::FWorldCollectSaveReferencesEvent FWorldDelegates::OnCollectSaveReferences;
#endif // WITH_EDITOR
FWorldDelegates::FWorldPostDuplicateEvent FWorldDelegates::OnPostDuplicate;
FWorldDelegates::FWorldCleanupEvent FWorldDelegates::OnWorldCleanup;
FWorldDelegates::FWorldCleanupEvent FWorldDelegates::OnPostWorldCleanup;
FWorldDelegates::FWorldEvent FWorldDelegates::OnPreWorldFinishDestroy;
FWorldDelegates::FOnLevelChanged FWorldDelegates::LevelAddedToWorld;
FWorldDelegates::FOnLevelChanged FWorldDelegates::PreLevelRemovedFromWorld;
FWorldDelegates::FOnLevelChanged FWorldDelegates::LevelRemovedFromWorld;
FWorldDelegates::FLevelOffsetEvent FWorldDelegates::PostApplyLevelOffset;
FWorldDelegates::FLevelTransformEvent FWorldDelegates::PostApplyLevelTransform;
PRAGMA_DISABLE_DEPRECATION_WARNINGS;
FWorldDelegates::FWorldGetAssetTags FWorldDelegates::GetAssetTags;
PRAGMA_ENABLE_DEPRECATION_WARNINGS;
FWorldDelegates::FWorldGetAssetTagsWithContext FWorldDelegates::GetAssetTagsWithContext;
FWorldDelegates::FOnWorldTickStart FWorldDelegates::OnWorldTickStart;
FWorldDelegates::FOnWorldTickEnd FWorldDelegates::OnWorldTickEnd;
FWorldDelegates::FOnWorldPreActorTick FWorldDelegates::OnWorldPreActorTick;
FWorldDelegates::FOnWorldPostActorTick FWorldDelegates::OnWorldPostActorTick;
FWorldDelegates::FOnWorldPreSendAllEndOfFrameUpdates FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates;
FWorldDelegates::FWorldEvent FWorldDelegates::OnWorldBeginTearDown;
#if WITH_EDITOR
FWorldDelegates::FRefreshLevelScriptActionsEvent FWorldDelegates::RefreshLevelScriptActions;
FWorldDelegates::FOnWorldPIEStarted FWorldDelegates::OnPIEStarted;
FWorldDelegates::FOnWorldPIEReady FWorldDelegates::OnPIEReady;
FWorldDelegates::FOnWorldPIEMapCreated FWorldDelegates::OnPIEMapCreated;
FWorldDelegates::FOnWorldPIEMapReady FWorldDelegates::OnPIEMapReady;
FWorldDelegates::FOnWorldPIEEnded FWorldDelegates::OnPIEEnded;
#endif // WITH_EDITOR
FWorldDelegates::FOnSeamlessTravelStart FWorldDelegates::OnSeamlessTravelStart;
FWorldDelegates::FOnSeamlessTravelTransition FWorldDelegates::OnSeamlessTravelTransition;
FWorldDelegates::FOnNetDriverCreated FWorldDelegates::OnNetDriverCreated;
FWorldDelegates::FOnCopyWorldData FWorldDelegates::OnCopyWorldData;
FWorldDelegates::FGameInstanceEvent FWorldDelegates::OnStartGameInstance;

UWorld::FOnWorldInitializedActors FWorldDelegates::OnWorldInitializedActors;

uint32 UWorld::CleanupWorldGlobalTag = 0;

UWorld::UWorld( const FObjectInitializer& ObjectInitializer )
: UObject(ObjectInitializer)
, bAllowDeferredPhysicsStateCreation(false)
, FeatureLevel(GMaxRHIFeatureLevel)
, bIsBuilt(false)
, bIsWorldInitialized(false)
#if WITH_EDITOR
, bDebugFrameStepExecutedThisFrame(false)
, bToggledBetweenPIEandSIEThisFrame(false)
, bPurgedScene(false)
#endif
, bShouldTick(true)
, bHasEverBeenInitialized(false)
, bIsBeingCleanedUp(false)
, ActiveLevelCollectionIndex(INDEX_NONE)
, AudioDeviceHandle()
#if WITH_EDITOR
, HierarchicalLODBuilder(new FHierarchicalLODBuilder(this))
#endif
, URL(FURL(NULL))
,	FXSystem(NULL)
,	TickTaskLevel(FTickTaskManagerInterface::Get().AllocateTickTaskLevel())
,	FlushLevelStreamingType(EFlushLevelStreamingType::None)
,	NextTravelType(TRAVEL_Relative)
,	CleanupWorldTag(0)

{
	TimerManager = new FTimerManager();
#if WITH_EDITOR
	SetPlayInEditorInitialNetMode(ENetMode::NM_Standalone);
	bBroadcastSelectionChange = true; //Ed Only
	EditorViews.SetNum(ELevelViewportType::LVT_MAX);
#endif // WITH_EDITOR

	FWorldDelegates::OnPostWorldCreation.Broadcast(this);

	if (!bDisableInGamePerfTrackersForUninitializedWorlds)
	{
		PerfTrackers = new FWorldInGamePerformanceTrackers();
	}
	else
	{
		PerfTrackers = nullptr;
	}

	IsInBlockTillLevelStreamingCompleted = 0;
	BlockTillLevelStreamingCompletedEpoch = 0;
}

UWorld::~UWorld()
{
	if (PerfTrackers)
	{
		delete PerfTrackers;
	}
}

void UWorld::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << PersistentLevel;

	if (Ar.UEVer() < VER_UE4_ADD_EDITOR_VIEWS)
	{
#if WITH_EDITOR
		EditorViews.SetNum(4);
#endif
		for (int32 i = 0; i < 4; ++i)
		{
			FLevelViewportInfo TempViewportInfo;
			Ar << TempViewportInfo;
#if WITH_EDITOR
			if (Ar.IsLoading())
			{
				EditorViews[i] = TempViewportInfo;
			}
#endif
		}
	}
#if WITH_EDITOR
	if ( Ar.IsLoading() )
	{
		for (FLevelViewportInfo& ViewportInfo : EditorViews)
		{
			ViewportInfo.CamUpdated = true;

			if ( ViewportInfo.CamOrthoZoom < MIN_ORTHOZOOM || ViewportInfo.CamOrthoZoom > MAX_ORTHOZOOM )
			{
				ViewportInfo.CamOrthoZoom = DEFAULT_ORTHOZOOM;
			}
		}
		EditorViews.SetNum(ELevelViewportType::LVT_MAX);
	}
#endif

	if (Ar.UEVer() < VER_UE4_REMOVE_SAVEGAMESUMMARY)
	{
		UObject* DummyObject;
		Ar << DummyObject;
	}

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << Levels;
#if WITH_EDITORONLY_DATA
		Ar << CurrentLevel;
#endif
		Ar << URL;

		Ar << NetDriver;
		
		Ar << LineBatcher;
		Ar << PersistentLineBatcher;
		Ar << ForegroundLineBatcher;  
		Ar << PhysicsField;

		Ar << MyParticleEventManager;
		Ar << GameState;
		Ar << AuthorityGameMode;
		Ar << NetworkManager;

		Ar << NavigationSystem;
		Ar << AvoidanceManager;
	}

	Ar << ExtraReferencedObjects;

#if WITH_EDITOR
	if (Ar.IsSaving() && Ar.IsPersistent())
	{
		TArray<ULevelStreaming*> PersistedStreamingLevels;
		PersistedStreamingLevels.Reserve(StreamingLevels.Num());
		Algo::CopyIf(StreamingLevels, PersistedStreamingLevels, [&](ULevelStreaming* LevelStreaming) { return LevelStreaming && !LevelStreaming->HasAnyFlags(RF_Transient); });
		Ar << PersistedStreamingLevels;

		if (Ar.IsObjectReferenceCollector())
		{
			FWorldDelegates::OnCollectSaveReferences.Broadcast(this, Ar);
		}
	}
	else
#endif
	{
		Ar << StreamingLevels;
	}
		
	// Mark archive and package as containing a map if we're serializing to disk.
	if( !HasAnyFlags( RF_ClassDefaultObject ) && Ar.IsPersistent() )
	{
		Ar.ThisContainsMap();
		GetOutermost()->ThisContainsMap();
	}

#if WITH_EDITOR
	// Serialize for PIE
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << OriginLocation;
		Ar << RequestedOriginLocation;
		Ar << OriginalWorldName;
	}
	
	// UWorlds loaded/duplicated for PIE must lose RF_Public and RF_Standalone since they should not be referenced by objects in other packages and they should be GCed normally
	if (GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		ClearFlags(RF_Public|RF_Standalone);
	}
#endif
}

void UWorld::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UWorld* This = CastChecked<UWorld>(InThis);

#if WITH_EDITOR
	if( GIsEditor )
	{
		Collector.AddReferencedObject( This->PersistentLevel, This );
		Collector.AddReferencedObjects(This->Levels, This);
		Collector.AddReferencedObject( This->CurrentLevel, This );
		Collector.AddReferencedObject( This->NetDriver, This );
		Collector.AddReferencedObject( This->DemoNetDriver, This );
		Collector.AddReferencedObject( This->LineBatcher, This );
		Collector.AddReferencedObject( This->PersistentLineBatcher, This );
		Collector.AddReferencedObject( This->ForegroundLineBatcher, This );
		Collector.AddReferencedObject( This->PhysicsField, This);
		Collector.AddReferencedObject( This->MyParticleEventManager, This );
		Collector.AddReferencedObject( This->GameState, This );
		Collector.AddReferencedObject( This->AuthorityGameMode, This );
		Collector.AddReferencedObject( This->NetworkManager, This );
		Collector.AddReferencedObject( This->NavigationSystem, This );
		Collector.AddReferencedObject( This->AvoidanceManager, This );
	}
#endif

	This->StreamingLevelsToConsider.AddReferencedObjects(InThis, Collector);

	This->SubsystemCollection.AddReferencedObjects(InThis, Collector);

	Super::AddReferencedObjects( InThis, Collector );
}

#if WITH_EDITOR
EDataValidationResult UWorld::IsDataValid(FDataValidationContext& Context) const 
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	for (FActorIterator It(this); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			Result = CombineDataValidationResults(Result, Actor->IsDataValid(Context));
		}
	}
	return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}

bool UWorld::Rename(const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags)
{
	check(PersistentLevel);

	UPackage* OldPackage = GetOutermost();

	bool bShouldFail = false;
	FWorldDelegates::OnPreWorldRename.Broadcast(this, InName, NewOuter, Flags, bShouldFail);

	// Make sure our legacy lightmap data is initialized so it can be renamed
	PersistentLevel->HandleLegacyMapBuildData();

	const bool bTestRename = (Flags & REN_Test) != 0;

	FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
	IHierarchicalLODUtilities* Utilities = Module.GetUtilities();

	TArray<UPackage*> OldHLODPackages;
	const int32 NumHLODLevels = PersistentLevel->GetWorldSettings()->GetNumHierarchicalLODLevels();

	if (!bTestRename)
	{
		OldHLODPackages.SetNumZeroed(NumHLODLevels);

		for (AActor* Actor : PersistentLevel->Actors)
		{
			if (ALODActor* LODActor = Cast<ALODActor>(Actor))
			{
				if (UHLODProxy* HLODProxy = LODActor->GetProxy())
				{
					OldHLODPackages[LODActor->LODLevel - 1] = HLODProxy->GetPackage();
				}
			}
		}
	}

	if (bShouldFail)
	{
		return false;
	}

	// Rename the world itself
	if ( !Super::Rename(InName, NewOuter, Flags) )
	{
		return false;
	}

	// We're moving the world to a new package, rename UObjects which are map data but don't have the UWorld in their Outer chain.  There are two cases:
	// 1) legacy lightmap textures and MapBuildData object will be in the same package as the UWorld.  We need to move these to the new world package.
	// 2) MapBuildData will be in a separate package with lightmap textures underneath it.  We need to move these to an appropriate build data package.
	if (PersistentLevel->MapBuildData)
	{
		FName NewMapBuildDataName = PersistentLevel->MapBuildData->GetFName();
		UObject* NewMapBuildDataOuter = nullptr;

		if (PersistentLevel->MapBuildData->IsLegacyBuildData())
		{
			NewMapBuildDataOuter = NewOuter;

			TArray<UTexture2D*> LightMapsAndShadowMaps;
			GetLightMapsAndShadowMaps(PersistentLevel, LightMapsAndShadowMaps);

			for (UTexture2D* Tex : LightMapsAndShadowMaps)
			{
				if (Tex)
				{
					if (!Tex->Rename(*Tex->GetName(), NewOuter, Flags))
					{
						return false;
					}
				}
			}
			NewMapBuildDataOuter = NewOuter;
		}
		else
		{
			FString NewPackageName = NewOuter ? NewOuter->GetOutermost()->GetName() : GetOutermost()->GetName();
			NewPackageName += TEXT("_BuiltData");
			NewMapBuildDataName = FPackageName::GetShortFName(*NewPackageName);
			UPackage* BuildDataPackage = PersistentLevel->MapBuildData->GetOutermost();

			if (!BuildDataPackage->Rename(*NewPackageName, nullptr, Flags))
			{
				return false;
			}

			NewMapBuildDataOuter = BuildDataPackage;
		}

		if (!PersistentLevel->MapBuildData->Rename(*NewMapBuildDataName.ToString(), NewMapBuildDataOuter, Flags))
		{
			return false;
		}
	}


	if (!bTestRename)
	{
		// We also need to rename any external actor packages we have since the search for them is based on the world name
		// Make a Copy to iterate as this could modify PersistentLevel->Actors
		TArray<AActor*> CopyActors;
		CopyActors.Reserve(PersistentLevel->Actors.Num());
		Algo::CopyIf(PersistentLevel->Actors, CopyActors, [](AActor* InActor) { return InActor && InActor->IsMainPackageActor(); });
				
		// Instead of just renaming the package, re-embed and re-externalize the actor
		// this will leave dirty empty actor packages being which will be cleaned up though SaveAll, although SaveCurrentLevel won't pick them up
		for (AActor* Actor : CopyActors)
		{
			UPackage* ExternalPackage = Actor->GetPackage();
			Actor->SetPackageExternal(false);
			
			TArray<UObject*> DependantObjects;
			ForEachObjectWithPackage(ExternalPackage, [&DependantObjects](UObject* Object)
			{
				if (!Cast<UMetaData>(Object))
				{
					DependantObjects.Add(Object);
				}
				return true;
			}, false);
						
			Actor->SetPackageExternal(true);

			// Move dependant objects into the new actor package
			for (UObject* DependantObject : DependantObjects)
			{
				DependantObject->Rename(nullptr, Actor->GetExternalPackage(), Flags);
			}
		}

		// Process external objects other than actors
		if (PersistentLevel->IsUsingExternalObjects())
		{
			ForEachObjectWithOuter(PersistentLevel, [this](UObject* Object)
			{
				if (Object->IsPackageExternal() && !Object->IsA<AActor>())
				{
					FExternalPackageHelper::SetPackagingMode(Object, PersistentLevel, false);
					FExternalPackageHelper::SetPackagingMode(Object, PersistentLevel, true);
				}
				return true;
			}, /*bIncludeNestedObjects*/ true);
		}
	}

	// Rename the level script blueprint now, unless we are in PostLoad. ULevel::PostLoad should handle renaming this at load time.
	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		const bool bDontCreate = true;
		UBlueprint* LevelScriptBlueprint = PersistentLevel->GetLevelScriptBlueprint(bDontCreate);
		if ( LevelScriptBlueprint )
		{
			// See if we are just testing for a rename. When testing, the world hasn't actually changed outers, so we need to test for name collisions at the target outer.
			if ( bTestRename )
			{
				// We are just testing. Check for name collisions in the new package. This is only needed because these objects use the supplied outer's Outermost() instead of the outer itself
				if (!LevelScriptBlueprint->RenameGeneratedClasses(InName, NewOuter, Flags))
				{
					return false;
				}
			}
			else
			{
				// The level blueprint must be named the same as the level/world.
				// If there is already something there with that name, rename it to something else.
				if (UObject* ExistingObject = StaticFindObject(nullptr, LevelScriptBlueprint->GetOuter(), InName))
				{
					ExistingObject->Rename(nullptr, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
				}

				// This is a normal rename. Use LevelScriptBlueprint->GetOuter() instead of NULL to make sure the generated top level objects are moved appropriately
				if ( !LevelScriptBlueprint->Rename(InName, LevelScriptBlueprint->GetOuter(), Flags) )
				{
					return false;
				}
			}
		}
	}

	// Update the PKG_ContainsMap package flag
	UPackage* NewPackage = GetOutermost();
	if ( !bTestRename && NewPackage != OldPackage )
	{
		// If this is the last world removed from a package, clear the PKG_ContainsMap flag
		if ( UWorld::FindWorldInPackage(OldPackage) == NULL )
		{
			OldPackage->ClearPackageFlags(PKG_ContainsMap);
		}

		// Set the PKG_ContainsMap flag in the new package
		NewPackage->ThisContainsMap();
	}

	// Move over HLOD assets to new _HLOD Package
	if (!bTestRename && OldHLODPackages.FindByPredicate([](UPackage* InPackage) -> bool { return InPackage != nullptr; }))
	{
		TArray<UObject*> DeleteObjects;

		for (int32 HLODIndex = 0; HLODIndex < NumHLODLevels; ++HLODIndex)
		{
			if (OldHLODPackages[HLODIndex] != nullptr)
			{
				UPackage* NewHLODPackage = Utilities->CreateOrRetrieveLevelHLODPackage(PersistentLevel, HLODIndex);

				TArray<UObject*> Objects;
				// Retrieve all of the HLOD objects 
				ForEachObjectWithOuter(OldHLODPackages[HLODIndex], [&Objects](UObject* Obj)
				{
					if (ObjectTools::IsObjectBrowsable(Obj))
					{
						Objects.Add(Obj);
					}
				});
				// Rename them 'into' the new HLOD package
				for (UObject* Object : Objects)
				{
					if(UHLODProxy* HLODProxy = Cast<UHLODProxy>(Object))
					{
						// HLOD proxy gets the same name as the package
						HLODProxy->Rename(*FPackageName::GetShortName(*NewHLODPackage->GetName()), NewHLODPackage);
						HLODProxy->SetMap(this);
					}
					else
					{
						Object->Rename(*Object->GetName(), NewHLODPackage);
					}
				}
				
				DeleteObjects.Add(Cast<UObject>(OldHLODPackages[HLODIndex]));
			}
		}
		
		// Delete the old HLOD packages
		ObjectTools::DeleteObjectsUnchecked(DeleteObjects);		
	}
	

	if (!bTestRename)
	{
		FWorldDelegates::OnPostWorldRename.Broadcast(this);
	}

	return true;
}
#endif

void UWorld::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	TArray<UObject*> ObjectsToFixReferences;
	TMap<UObject*, UObject*> ReplacementMap;

	// If we are not duplicating for PIE, fix up objects that travel with the world.
	// Note that these objects should really be inners of the world, so if they become inners later, most of this code should not be necessary
	if ( !bDuplicateForPIE )
	{
		check(PersistentLevel);

		// Update the persistent level's owning world. This is needed for some initialization
		if ( !PersistentLevel->OwningWorld )
		{
			PersistentLevel->OwningWorld = this;
		}

#if WITH_EDITORONLY_DATA
		// Update the current level as well
		if ( !CurrentLevel )
		{
			CurrentLevel = PersistentLevel;
		}
#endif

		UPackage* MyPackage = GetOutermost();

		// Make sure PKG_ContainsMap is set
		MyPackage->ThisContainsMap();

#if WITH_EDITOR
		// Add the world to the list of objects in which to fix up references.
		ObjectsToFixReferences.Add(this);

		// We're duplicating the world, also duplicate UObjects which are map data but don't have the UWorld in their Outer chain.  There are two cases:
		// 1) legacy lightmap textures and MapBuildData object will be in the same package as the UWorld
		// 2) MapBuildData will be in a separate package with lightmap textures underneath it
		if (PersistentLevel->MapBuildData)
		{
			UPackage* BuildDataPackage = MyPackage;
			FName NewMapBuildDataName = PersistentLevel->MapBuildData->GetFName();
			
			if (!PersistentLevel->MapBuildData->IsLegacyBuildData())
			{
				BuildDataPackage = PersistentLevel->CreateMapBuildDataPackage();
				NewMapBuildDataName = FPackageName::GetShortFName(BuildDataPackage->GetFName());
			}
			
			UObject* NewBuildData = StaticDuplicateObject(PersistentLevel->MapBuildData, BuildDataPackage, NewMapBuildDataName);
			NewBuildData->MarkPackageDirty();
			ReplacementMap.Add(PersistentLevel->MapBuildData, NewBuildData);
			ObjectsToFixReferences.Add(NewBuildData);

			UObject* NewTextureOuter = MyPackage;

			if (!PersistentLevel->MapBuildData->IsLegacyBuildData())
			{
				NewTextureOuter = NewBuildData;
			}

			TArray<UTexture2D*> LightMapsAndShadowMaps;
			GetLightMapsAndShadowMaps(PersistentLevel, LightMapsAndShadowMaps);

			// Duplicate the textures, if any
			for (UTexture2D* Tex : LightMapsAndShadowMaps)
			{
				if (Tex && Tex->GetOutermost() != NewTextureOuter)
				{
					UObject* NewTex = StaticDuplicateObject(Tex, NewTextureOuter, Tex->GetFName());
					ReplacementMap.Add(Tex, NewTex);
				}
			}
		}
#endif // WITH_EDITOR
	}

	FWorldDelegates::OnPostDuplicate.Broadcast(this, bDuplicateForPIE, ReplacementMap, ObjectsToFixReferences);

#if WITH_EDITOR
	// Now replace references from the old textures/classes to the new ones, if any were duplicated
	if (ReplacementMap.Num() > 0)
	{
		for (UObject* Obj : ObjectsToFixReferences)
		{
			FArchiveReplaceObjectRef<UObject> ReplaceAr(Obj, ReplacementMap, EArchiveReplaceObjectFlags::IgnoreOuterRef);
		}
		// PostEditChange is required for some objects to react to the change, e.g. update render-thread proxies
		for (UObject* Obj : ObjectsToFixReferences)
		{
			Obj->PostEditChange();
		}
	}

	if (bDuplicateForPIE)
	{
		// We use a weak ptr here in case the level gets destroyed before asset
		// compilation finishes.
		TWeakObjectPtr<ULevel> PersistentLevelPtr(PersistentLevel);
		auto ValidateTextureOverridesForPIE =
			[PersistentLevelPtr, FeatureLevel = GetFeatureLevel()]()
			{
				ULevel* Level = PersistentLevelPtr.Get();
				if (Level)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(ValidateTextureOverridesForPIE);

					TSet<UMaterialInterface*> ProcessedMaterials;
					for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
					{
						UPrimitiveComponent* Component = *It;
						AActor* Owner = Component->GetOwner();
						if (Owner != nullptr && !Owner->HasAnyFlags(RF_ClassDefaultObject) && Owner->IsInLevel(Level))
						{
							TArray<UMaterialInterface*> Materials;
							Component->GetUsedMaterials(Materials);
							for (UMaterialInterface* Material : Materials)
							{
								bool bIsAlreadyInSet = false;
								ProcessedMaterials.FindOrAdd(Material, &bIsAlreadyInSet);
								if (!bIsAlreadyInSet)
								{
									if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material))
									{
										MaterialInstance->ValidateTextureOverrides(FeatureLevel);
									}
								}
							}
						}
					}
				}
			};

		// When PIE begins, check/log any problems with textures assigned to material instances
		// but wait until all assets are properly compiled to to so.
		if (FAssetCompilingManager::Get().GetNumRemainingAssets() == 0)
		{
			ValidateTextureOverridesForPIE();
		}
		else
		{
			// Some assets are still being compiled, register to the event so we can do the validation
			// once the compilation is finished.
			TSharedPtr<FDelegateHandle> DelegateHandle = MakeShareable(new FDelegateHandle());
			*DelegateHandle = FAssetCompilingManager::Get().OnAssetPostCompileEvent().AddWeakLambda(this,
				[DelegateHandle, ValidateTextureOverridesForPIE](const TArray<FAssetCompileData>&)
				{
					if (FAssetCompilingManager::Get().GetNumRemainingAssets() == 0)
					{
						ValidateTextureOverridesForPIE();

						// Must be the last line because it will destroy the lambda along with the capture
						verify(FAssetCompilingManager::Get().OnAssetPostCompileEvent().Remove(*DelegateHandle));
					}
				}
			);
		}
	}
#endif // WITH_EDITOR
}

void UWorld::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	// Make sure we catch worlds that where initialized through UEditorEngine::OnAssetLoaded/OnAssetCreated
	// (This can happen if World was loaded, then its RF_Standalone flag was removed and a GC happened) 
	if (WorldType == EWorldType::Inactive && IsInitialized())
	{
		CleanupWorld();
	}
#endif

	for (FLevelCollection& LevelCollection : LevelCollections)
	{
		TSet<TObjectPtr<ULevel>> CollectionLevels = LevelCollection.GetLevels();
		for (ULevel* CollectionLevel : CollectionLevels)
		{
			LevelCollection.RemoveLevel(CollectionLevel);
		}
	}

	if (PhysicsScene != nullptr)
	{
		// Tell PhysicsScene to stop kicking off async work so we can cleanup after pending work is complete.
		PhysicsScene->BeginDestroy();
	}

	if (Scene)
	{
		Scene->UpdateParameterCollections(TArray<FMaterialParameterCollectionInstanceResource*>());
	}

	FAudioDeviceHandle EmptyHandle;
	SetAudioDevice(EmptyHandle);
	check(!AudioDeviceDestroyedHandle.IsValid());
}

void UWorld::ReleasePhysicsScene()
{
	if (PhysicsScene)
	{
		delete PhysicsScene;
		PhysicsScene = NULL;

		if (GPhysCommandHandler)
		{
			GPhysCommandHandler->Flush();
		}
	}
}

void UWorld::FinishDestroy()
{
	if (bIsWorldInitialized)
	{
		UE_LOG(LogWorld, Warning, TEXT("UWorld::FinishDestroy called after InitWorld without calling CleanupWorld first."));
	}

	// Avoid cleanup if the world hasn't been initialized, e.g., the default object or a world object that got loaded
	// due to level streaming.
	if (bHasEverBeenInitialized)
	{
		FWorldDelegates::OnPreWorldFinishDestroy.Broadcast(this);

		// Wait for Async Trace data to finish and reset global variable
		WaitForAllAsyncTraceTasks();

		// navigation system should be removed already by UWorld::CleanupWorld
		// unless it wanted to keep resources but got destroyed now
		SetNavigationSystem(nullptr);

		if (FXSystem)
		{
			FFXSystemInterface::Destroy( FXSystem );
			FXSystem = NULL;
		}

		ReleasePhysicsScene();

		if (Scene)
		{
			Scene->Release();
			Scene = NULL;
		}
	}

	// Clear GWorld pointer if it's pointing to this object.
	if( GWorld == this )
	{
		GWorld = NULL;
	}
	FTickTaskManagerInterface::Get().FreeTickTaskLevel(TickTaskLevel);
	TickTaskLevel = NULL;

	if (TimerManager)
	{
		delete TimerManager;
	}

#if WITH_EDITOR
	if (HierarchicalLODBuilder)
	{
		delete HierarchicalLODBuilder;
	}
#endif // WITH_EDITOR

	// Remove the PKG_ContainsMap flag from packages that no longer contain a world
	{
		UPackage* WorldPackage = GetOutermost();

		if (WorldPackage->HasAnyPackageFlags(PKG_ContainsMap))
		{
			bool bContainsAnotherWorld = false;
			TArray<UObject*> PotentialWorlds;
			GetObjectsWithPackage(WorldPackage, PotentialWorlds, false);
			for (UObject* PotentialWorld : PotentialWorlds)
			{
				UWorld* World = Cast<UWorld>(PotentialWorld);
				if (World && World != this)
				{
					bContainsAnotherWorld = true;
					break;
				}
			}

			if ( !bContainsAnotherWorld )
			{
				WorldPackage->ClearPackageFlags(PKG_ContainsMap);
			}
		}
	}

	Super::FinishDestroy();
}

bool UWorld::IsReadyForFinishDestroy()
{
	// In single threaded, task will never complete unless we wait on it, allow FinishDestroy so we can wait on task, otherwise this will hang GC.
	// In multi threaded, we cannot wait in FinishDestroy, as this may schedule another task that is unsafe during GC.
	const bool bIsSingleThreadEnvironment = FPlatformProcess::SupportsMultithreading() == false;
	if (bIsSingleThreadEnvironment == false)
	{
		if (PhysicsScene != nullptr)
		{
			PhysicsScene->KillSafeAsyncTasks();
			PhysicsScene->WaitSolverTasks();

			if (PhysicsScene->AreAnyTasksPending())
			{
				return false;
			}
		}
	}

	return Super::IsReadyForFinishDestroy();
}

void UWorld::PostLoad()
{
	EWorldType::Type * PreLoadWorldType = UWorld::WorldTypePreLoadMap.Find(GetOuter()->GetFName());
	if (PreLoadWorldType)
	{
		WorldType = *PreLoadWorldType;
	}
	else
	{
		// Since we did not specify a world type, assume it was loaded from a package to persist in memory
		WorldType = EWorldType::Inactive;
	}

	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	CurrentLevel = PersistentLevel;
#endif
#if WITH_EDITOR
	RepairSingletonActors();
	RepairStreamingLevels();
#endif

	for (auto It = StreamingLevels.CreateIterator(); It; ++It)
	{
		if (ULevelStreaming* const StreamingLevel = *It)
		{
			// Make sure that the persistent level isn't in this world's list of streaming levels.  This should
			// never really happen, but was needed in at least one observed case of corrupt map data.
			if (PersistentLevel && (StreamingLevel->GetWorldAsset() == this || StreamingLevel->GetLoadedLevel() == PersistentLevel))
			{
				// Remove this streaming level
				It.RemoveCurrent();
				MarkPackageDirty();
			}
			else
			{
				FStreamingLevelPrivateAccessor::OnLevelAdded(StreamingLevel);
				if (FStreamingLevelPrivateAccessor::UpdateTargetState(StreamingLevel))
				{
					StreamingLevelsToConsider.Add(StreamingLevel);
				}
			}
		}
		else
		{
			// Remove null streaming level entries (could be if level was saved with transient level streaming objects)
			It.RemoveCurrent();
		}
	}

	// Add the garbage collection callbacks
	FLevelStreamingGCHelper::AddGarbageCollectorCallback();

	// Initially set up the parameter collection list. This may be run again in UWorld::InitWorld but it's required here for some editor and streaming cases
	SetupParameterCollectionInstances();

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Avoid renaming PIE worlds and Instanced worlds except for the new maps (/Temp/Untitled) to preserve naming behavior
		const bool bLoadedForDiff = GetPackage()->HasAnyPackageFlags(PKG_ForDiffing);
		if (!GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor) && !bLoadedForDiff && (!IsInstanced() || GetPackage()->GetPathName().StartsWith(TEXT("/Temp/Untitled"))))
		{
			// Needed for VER_UE4_WORLD_NAMED_AFTER_PACKAGE. If this file was manually renamed outside of the editor, this is needed anyway
			const FString ShortPackageName = FPackageName::GetLongPackageAssetName(GetPackage()->GetName());
			OriginalWorldName = GetFName();
			if (GetName() != ShortPackageName)
			{
				// Do not go through UWorld::Rename as we do not want to go through map build data/external actors or hlod renaming in post load
				UObject::Rename(*ShortPackageName, NULL, REN_NonTransactional | REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
			}

			// Worlds are assets so they need RF_Public and RF_Standalone (for the editor)
			SetFlags(RF_Public | RF_Standalone);
		}

		// Ensure the DefaultBrush's model has the same outer as the default brush itself. Older packages erroneously stored this object as a top-level package
		if (ABrush* DefaultBrush = PersistentLevel->Actors.Num() < 2 ? NULL : Cast<ABrush>(PersistentLevel->Actors[1]))
		{
			if (UModel* Model = DefaultBrush->Brush)
			{
				if (Model->GetOuter() != DefaultBrush->GetOuter())
				{
					Model->Rename(TEXT("Brush"), DefaultBrush->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
				}
			}
		}

		// Make sure thumbnail info exists
		if ( !ThumbnailInfo )
		{
			ThumbnailInfo = NewObject<UWorldThumbnailInfo>(this, NAME_None, RF_Transactional);
		}
	}
#endif

	// Reset between worlds so that the metric is only relevant to the current world.
	ResetAverageRequiredTexturePoolSize();
}

#if WITH_EDITORONLY_DATA
void UWorld::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(GEngine->WorldSettingsClass));
	OutConstructClasses.Add(FTopLevelAssetPath(UWorldThumbnailInfo::StaticClass()));
}
#endif

void UWorld::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	if (PersistentLevel)
	{
		PersistentLevel->PreDuplicate(DupParams);
	}
}

bool UWorld::PreSaveRoot(const TCHAR* Filename)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	return Super::PreSaveRoot(Filename);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UWorld::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	if (!ObjectSaveContext.IsFirstConcurrentSave())
	{
		return;
	}

#if WITH_EDITOR
	// Flush outstanding static mesh compilation to ensure that construction scripts are properly ran and not deferred prior to saving
	FStaticMeshCompilingManager::Get().FinishAllCompilation();

	// Execute all pending actor construction scripts
	FActorDeferredScriptManager::Get().FinishAllCompilation();

	// if we are cooking this world, convert its persistent level to internal actors before doing so
	if (ObjectSaveContext.IsCooking())
	{
		PersistentLevel->DetachAttachAllActorsPackages(/*bReattach*/false);
	}
#endif

	// Update components and keep track off whether we need to clean them up afterwards.
	if(!PersistentLevel->bAreComponentsCurrentlyRegistered)
	{
		PersistentLevel->UpdateLevelComponents(true);
		ObjectSaveContext.SetCleanupRequired(true);
	}
}

void UWorld::PostSaveRoot(bool bCleanupIsRequired)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PostSaveRoot(bCleanupIsRequired);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UWorld::PostSaveRoot( FObjectPostSaveRootContext ObjectSaveContext )
{
	Super::PostSaveRoot(ObjectSaveContext);
	if (!ObjectSaveContext.IsLastConcurrentSave())
	{
		return;
	}

	if( ObjectSaveContext.IsCleanupRequired() )
	{
		PersistentLevel->ClearLevelComponents();
	}

#if WITH_EDITOR
	// if we are cooking this world, convert its persistent level back its proper loading strategy if needed
	// NOTE: can't use bCleanupIsRequired since we don't want to unregister component if they were registered...
	if (ObjectSaveContext.IsCooking())
	{
		PersistentLevel->DetachAttachAllActorsPackages(/*bReattach*/true);
	}

	if (!ObjectSaveContext.IsProceduralSave() && !(ObjectSaveContext.GetSaveFlags() & SAVE_FromAutosave))
	{
		// Once saved, OriginalWorldName must match World's name
		OriginalWorldName = GetFName();
	}
#endif
}

UWorld* UWorld::GetWorld() const
{
	// Arg... rather hacky, but it seems conceptually ok because the object passed in should be able to fetch the
	// non-const world it's part of.  That's not a mutable action (normally) on the object, as we haven't changed
	// anything.
	return const_cast<UWorld*>(this);
}

void UWorld::SetupParameterCollectionInstances()
{
	ULevel* Level = PersistentLevel;
	const bool bIsWorldPartitionRuntimeCell = Level && Level->IsWorldPartitionRuntimeCell();
	if (!bIsWorldPartitionRuntimeCell)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_World_SetupParameterCollectionInstances);

		// Create an instance for each parameter collection in memory
		for (UMaterialParameterCollection* CurrentCollection : TObjectRange<UMaterialParameterCollection>())
		{
			AddParameterCollectionInstance(CurrentCollection, false);
		}

		UpdateParameterCollectionInstances(false, false);
	}
}

void UWorld::AddParameterCollectionInstance(UMaterialParameterCollection* Collection, bool bUpdateScene)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_World_AddParameterCollectionInstance);

	int32 ExistingIndex = INDEX_NONE;

	for (int32 InstanceIndex = 0; InstanceIndex < ParameterCollectionInstances.Num(); InstanceIndex++)
	{
		const UMaterialParameterCollectionInstance* Instance = ParameterCollectionInstances[InstanceIndex];
		if (Instance != nullptr && Instance->GetCollection() == Collection)
		{
			ExistingIndex = InstanceIndex;
			break;
		}
	}

	CreateParameterCollectionInstance(ExistingIndex, Collection, bUpdateScene);
}

UMaterialParameterCollectionInstance* UWorld::GetParameterCollectionInstance(const UMaterialParameterCollection* Collection) const
{
	for (int32 InstanceIndex = 0; InstanceIndex < ParameterCollectionInstances.Num(); InstanceIndex++)
	{
		if (ParameterCollectionInstances[InstanceIndex]->GetCollection() == Collection)
		{
			return ParameterCollectionInstances[InstanceIndex];
		}
	}

	// Lazy create one if not found
	return const_cast<UWorld*>(this)->CreateParameterCollectionInstance(INDEX_NONE, const_cast<UMaterialParameterCollection*>(Collection), true);
}

void UWorld::UpdateParameterCollectionInstances(bool bUpdateInstanceUniformBuffers, bool bRecreateUniformBuffer)
{
	if (Scene)
	{
		TArray<FMaterialParameterCollectionInstanceResource*> InstanceResources;

		for (int32 InstanceIndex = 0; InstanceIndex < ParameterCollectionInstances.Num(); InstanceIndex++)
		{
			UMaterialParameterCollectionInstance* Instance = ParameterCollectionInstances[InstanceIndex];

			if (bUpdateInstanceUniformBuffers)
			{
				Instance->UpdateRenderState(bRecreateUniformBuffer);
			}
			else
			{
				checkf(!bRecreateUniformBuffer, TEXT("Recreate Uniform Buffer was requested but cannot be executed because bUpdateInstanceUniformBuffers was false"));
			}

			InstanceResources.Add(Instance->GetResource());
		}

		Scene->UpdateParameterCollections(InstanceResources);
	}
}

UMaterialParameterCollectionInstance* UWorld::CreateParameterCollectionInstance(int32 ExistingIndex, UMaterialParameterCollection* Collection, bool bUpdateScene)
{
	UMaterialParameterCollectionInstance* NewInstance = NewObject<UMaterialParameterCollectionInstance>();
	NewInstance->SetCollection(Collection, this);

	if (ExistingIndex != INDEX_NONE)
	{
		// Overwrite an existing instance
		ParameterCollectionInstances[ExistingIndex] = NewInstance;
	}
	else
	{
		// Add a new instance
		ParameterCollectionInstances.Add(NewInstance);
	}

	// Ensure the new instance creates initial render thread resources
	// This needs to happen right away, so they can be picked up by any cached shader bindings
	NewInstance->UpdateRenderState(true);

	if (bUpdateScene)
	{
		// Update the scene's list of instances, needs to happen to prevent a race condition with GC 
		// (rendering thread still uses the FMaterialParameterCollectionInstanceResource when GC deletes the UMaterialParameterCollectionInstance)
		// However, if UpdateParameterCollectionInstances is going to be called after many AddParameterCollectionInstance's, this can be skipped for now.
		UpdateParameterCollectionInstances(false, false);
	}

	return NewInstance;
}


void UWorld::OnPostGC()
{
	for (int32 InstanceIndex = ParameterCollectionInstances.Num()-1; InstanceIndex >= 0; InstanceIndex--)
	{
		if (!ParameterCollectionInstances[InstanceIndex]->IsCollectionValid())
		{
			ParameterCollectionInstances.RemoveAt(InstanceIndex);
		}
	}
}



UCanvas* UWorld::GetCanvasForRenderingToTarget()
{
	if (!CanvasForRenderingToTarget)
	{
		CanvasForRenderingToTarget = NewObject<UCanvas>(GetTransientPackage(), NAME_None);
	}

	return CanvasForRenderingToTarget;
}

UCanvas* UWorld::GetCanvasForDrawMaterialToRenderTarget()
{
	if (!CanvasForDrawMaterialToRenderTarget)
	{
		CanvasForDrawMaterialToRenderTarget = NewObject<UCanvas>(GetTransientPackage(), NAME_None);
	}

	return CanvasForDrawMaterialToRenderTarget;
}

void UWorld::GetCollisionProfileChannelAndResponseParams(FName ProfileName, ECollisionChannel& CollisionChannel, FCollisionResponseParams& ResponseParams)
{
	if (UCollisionProfile::GetChannelAndResponseParams(ProfileName, CollisionChannel, ResponseParams))
	{
		return;
	}

	// No profile found
	UE_LOG(LogPhysics, Warning, TEXT("COLLISION PROFILE [%s] is not found"), *ProfileName.ToString());

	CollisionChannel = ECC_WorldStatic;
	ResponseParams = FCollisionResponseParams::DefaultResponseParam;
}

UAISystemBase* UWorld::CreateAISystem()
{
	// create navigation system for editor and server targets, but remove it from game clients
	if (AISystem == NULL && UAISystemBase::ShouldInstantiateInNetMode(GetNetMode()) && PersistentLevel)
	{
		const FName AIModuleName = UAISystemBase::GetAISystemModuleName();
		const AWorldSettings* WorldSettings = PersistentLevel->GetWorldSettings(false);
		if (AIModuleName.IsNone() == false && WorldSettings && WorldSettings->IsAISystemEnabled())
		{
			IAISystemModule* AISystemModule = FModuleManager::LoadModulePtr<IAISystemModule>(AIModuleName);
			if (AISystemModule)
			{
				AISystem = AISystemModule->CreateAISystemInstance(this);
			}
		}
	}

	return AISystem; 
}

void UWorld::RepairChaosActors()
{
	if (!PhysicsScene_Chaos)
	{
		// Streamed levels need to find the persistent level's owning world to fetch chaos scene.
		UWorld *OwningWorld = ULevel::StreamedLevelsOwningWorld.FindRef(PersistentLevel->GetOutermost()->GetFName()).Get();
		if (OwningWorld)
		{
			PersistentLevel->OwningWorld = OwningWorld;
			PhysicsScene_Chaos = PersistentLevel->OwningWorld->PhysicsScene_Chaos;
		}
	}

	if (!PhysicsScene_Chaos)
	{
		FChaosSolversModule* ChaosModule = FChaosSolversModule::GetModule();
		check(ChaosModule);
		bool bHasChaosActor = false;
		for (int32 i = 0; i < PersistentLevel->Actors.Num(); ++i)
		{
			if (PersistentLevel->Actors[i] && ChaosModule->IsValidSolverActorClass(PersistentLevel->Actors[i]->GetClass()))
			{
				bHasChaosActor = true;

				bool bClearOwningWorld = false;

				if (PersistentLevel->OwningWorld == nullptr)
				{
					bClearOwningWorld = true;
					PersistentLevel->OwningWorld = this;
				}

				PersistentLevel->Actors[i]->SetHasActorRegisteredAllComponents();
				PersistentLevel->Actors[i]->PostRegisterAllComponents();

				if (bClearOwningWorld)
				{
					PersistentLevel->OwningWorld = nullptr;
				}

				break;
			}
		}
		if (!bHasChaosActor)
		{
			bool bClearOwningWorld = false;

			if (PersistentLevel->OwningWorld == nullptr)
			{
				bClearOwningWorld = true;
				PersistentLevel->OwningWorld = this;
			}

			FActorSpawnParameters ChaosSpawnInfo;
			ChaosSpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ChaosSpawnInfo.Name = TEXT("DefaultChaosActor");
			SpawnActor(ChaosModule->GetSolverActorClass(), nullptr, nullptr, ChaosSpawnInfo);
			check(PhysicsScene_Chaos);

			if (bClearOwningWorld)
			{
				PersistentLevel->OwningWorld = nullptr;
			}
		}
	}

	// make the current scene the default scene
	DefaultPhysicsScene_Chaos = PhysicsScene_Chaos;
}

UChaosEventRelay* UWorld::GetChaosEventRelay()
{
	if (PhysicsScene)
	{
		return PhysicsScene->GetChaosEventRelay();
	}
	return nullptr;
}

void UWorld::RepairStreamingLevels()
{
	for (int32 Index = 0; Index < StreamingLevels.Num(); )
	{
		ULevelStreaming* StreamingLevel = StreamingLevels[Index];
		if (StreamingLevel && !StreamingLevel->IsValidStreamingLevel())
		{
			StreamingLevels.RemoveAtSwap(Index);
		}
		else
		{
			++Index;
		}
	}
}

void UWorld::RepairSingletonActorOfClass(TSubclassOf<AActor> ActorClass)
{
	AActor* FoundActor = nullptr;

	for (int32 i=0; i<PersistentLevel->Actors.Num(); i++)
	{
		if (AActor* CurrentActor = PersistentLevel->Actors[i])
		{
			if (CurrentActor->IsA(ActorClass))
			{
				if (FoundActor)
				{
					if (FoundActor == CurrentActor)
					{
						UE_LOG(LogWorld, Log, TEXT("Extra '%s' actor found. Resave level %s to clean up."), *CurrentActor->GetPathName(), *PersistentLevel->GetPathName());
						PersistentLevel->Actors[i] = nullptr;
					}
					else
					{
						UE_LOG(LogWorld, Log, TEXT("Extra '%s' actor found. Resave level %s and actor to cleanup."), *CurrentActor->GetPathName(), *PersistentLevel->GetPathName());
						CurrentActor->Destroy();						
					}
				}
				else
				{
					FoundActor = CurrentActor;
				}
			}
		}
	}
}

void UWorld::RepairWorldSettings()
{
	AWorldSettings* ExistingWorldSettings = PersistentLevel->GetWorldSettings(false);

	if (ExistingWorldSettings == nullptr && PersistentLevel->Actors.Num() > 0)
	{
		ExistingWorldSettings = Cast<AWorldSettings>(PersistentLevel->Actors[0]);
		if (ExistingWorldSettings)
		{
			// This means the WorldSettings member just wasn't initialized, get that resolved
			PersistentLevel->SetWorldSettings(ExistingWorldSettings);
		}
	}

	// If for some reason we don't have a valid WorldSettings object go ahead and spawn one to avoid crashing.
	// This will generally happen if a map is being moved from a different project.
	if (ExistingWorldSettings == nullptr || ExistingWorldSettings->GetClass() != GEngine->WorldSettingsClass)
	{
		// Rename invalid WorldSettings to avoid name collisions
		if (ExistingWorldSettings)
		{
			ExistingWorldSettings->Rename(nullptr, PersistentLevel, REN_ForceNoResetLoaders);
		}
		
		bool bClearOwningWorld = false;

		if (PersistentLevel->OwningWorld == nullptr)
		{
			bClearOwningWorld = true;
			PersistentLevel->OwningWorld = this;
		}

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.Name = GEngine->WorldSettingsClass->GetFName();
		AWorldSettings* const NewWorldSettings = SpawnActor<AWorldSettings>( GEngine->WorldSettingsClass, SpawnInfo );

		// If there was an existing actor, copy its properties to the new actor (the it will be destroyed by SetWorldSettings)
		if (ExistingWorldSettings)
		{
			NewWorldSettings->UnregisterAllComponents();
			UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
			CopyParams.bNotifyObjectReplacement = true;
			UEngine::CopyPropertiesForUnrelatedObjects(ExistingWorldSettings, NewWorldSettings, CopyParams);
			NewWorldSettings->RegisterAllComponents();
		}

		PersistentLevel->SetWorldSettings(NewWorldSettings);

		// Re-sort actor list as we just shuffled things around.
		PersistentLevel->SortActorList();

		if (bClearOwningWorld)
		{
			PersistentLevel->OwningWorld = nullptr;
		}
	}

	// Now that we have set the proper world settings, clean up any other stay that may have accumulated due to legacy behaviors
	if (PersistentLevel->Actors.Num() > 1)
	{
		for (int32 Index = 1, ActorNum = PersistentLevel->Actors.Num(); Index < ActorNum; ++Index)
		{
			AActor* Actor = PersistentLevel->Actors[Index];
			if (Actor != nullptr && Actor->IsA<AWorldSettings>())
			{
				UE_LOG(LogWorld, Warning, TEXT("Extra World Settings '%s' actor found. Resave level %s to clean up."), *Actor->GetPathName(), *PersistentLevel->GetPathName());
				Actor->Destroy();
			}
		}
	}

	check(GetWorldSettings());
}

void UWorld::RepairSingletonActors()
{
	RepairWorldSettings();
}

#if WITH_EDITOR
void UWorld::RepairDefaultBrush()
{
	// See whether we're missing the default brush. It was possible in earlier builds to accidentally delete the default
	// brush of sublevels so we simply spawn a new one if we encounter it missing.
	ABrush* DefaultBrush = PersistentLevel->Actors.Num()<2 ? NULL : Cast<ABrush>(PersistentLevel->Actors[1]);
	if (GIsEditor)
	{
		if (!DefaultBrush || !DefaultBrush->IsStaticBrush() || DefaultBrush->BrushType != Brush_Default || !DefaultBrush->GetBrushComponent() ||
			!DefaultBrush->Brush || !DefaultBrush->Brush->Polys || DefaultBrush->Brush->Polys->Element.IsEmpty())
		{
			// Spawn the default brush.
			DefaultBrush = SpawnBrush();
			check(DefaultBrush->GetBrushComponent());
			DefaultBrush->Brush = NewObject<UModel>(DefaultBrush->GetOuter(), TEXT("Brush"));
			DefaultBrush->Brush->Initialize(DefaultBrush, true);
			DefaultBrush->GetBrushComponent()->Brush = DefaultBrush->Brush;
			DefaultBrush->SetNotForClientOrServer();
			DefaultBrush->Brush->SetFlags( RF_Transactional );
			DefaultBrush->Brush->Polys->SetFlags( RF_Transactional );

			// Create cube geometry.
			// We effectively replicate what is done in UEditorEngine::InitBuilderBrush() since we can't just use this function or the underlying
			// function UCubeBuilder::BuildCube() here directly.
			{
				constexpr float HalfSize = 128.0f;
				static const FVector3f Vertices[8] = {
					{-HalfSize, -HalfSize, -HalfSize},
					{-HalfSize, -HalfSize,  HalfSize},
					{-HalfSize,  HalfSize, -HalfSize},
					{-HalfSize,  HalfSize,  HalfSize},
					{ HalfSize, -HalfSize, -HalfSize},
					{ HalfSize, -HalfSize,  HalfSize},
					{ HalfSize,  HalfSize, -HalfSize},
					{ HalfSize,  HalfSize,  HalfSize}
				};

				constexpr int32 Indices[24] = {
					0, 1, 3, 2,
					2, 3, 7, 6,
					6, 7, 5, 4,
					4, 5, 1, 0,
					3, 1, 5, 7,
					0, 2, 6, 4
				};

				for (int32 i = 0; i < 6; ++i)
				{
					FPoly Poly;
					Poly.Init();
					Poly.Base = Vertices[0];

					for (int32 j = 0; j < 4; ++j)
					{
						new(Poly.Vertices) FVector3f(Vertices[Indices[i * 4 + j]]);
					}
					if (Poly.Finalize(DefaultBrush, 1) == 0)
					{
						new(DefaultBrush->Brush->Polys->Element)FPoly(Poly);
					}
				}

				DefaultBrush->Brush->BuildBound();
			}

			// The default brush is legacy but has to exist for some old bsp operations.  However it should not be interacted with in the editor. 
			DefaultBrush->SetIsTemporarilyHiddenInEditor(true);

			// Find the index in the array the default brush has been spawned at. Not necessarily
			// the last index as the code might spawn the default physics volume afterwards.
			const int32 DefaultBrushActorIndex = PersistentLevel->Actors.Find( DefaultBrush );

			// The default brush needs to reside at index 1.
			Exchange(PersistentLevel->Actors[1],PersistentLevel->Actors[DefaultBrushActorIndex]);

			// Re-sort actor list as we just shuffled things around.
			PersistentLevel->SortActorList();
		}
		else
		{
			// Ensure that the Brush and BrushComponent both point to the same model
			DefaultBrush->GetBrushComponent()->Brush = DefaultBrush->Brush;
		}

		// Reset the lightmass settings on the default brush; they can't be edited by the user but could have
		// been tainted if the map was created during a window where the memory was uninitialized
		if (DefaultBrush->Brush != NULL)
		{
			UModel* Model = DefaultBrush->Brush;

			const FLightmassPrimitiveSettings DefaultSettings;

			for (int32 i = 0; i < Model->LightmassSettings.Num(); ++i)
			{
				Model->LightmassSettings[i] = DefaultSettings;
			}

			if (Model->Polys != NULL) 
			{
				for (int32 i = 0; i < Model->Polys->Element.Num(); ++i)
				{
					Model->Polys->Element[i].LightmassSettings = DefaultSettings;
				}
			}
		}
	}
}
#endif

void UWorld::InitWorld(const InitializationValues IVS)
{
	if (!ensure(!bIsWorldInitialized))
	{
		return;
	}

	// Reset flags in case of world reuse
	bIsLevelStreamingFrozen = false;
	bShouldForceUnloadStreamingLevels = false;

	FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UWorld::OnPostGC);

	InitializeSubsystems();

	FWorldDelegates::OnPreWorldInitialization.Broadcast(this, IVS);

	AWorldSettings* WorldSettings = GetWorldSettings();
	if (IVS.bInitializeScenes)
	{

	#if WITH_EDITOR
		bEnableTraceCollision = IVS.bEnableTraceCollision;
		bForceUseMovementComponentInNonGameWorld = IVS.bForceUseMovementComponentInNonGameWorld;
	#endif


		if (IVS.bCreatePhysicsScene)
		{
			// Create the physics scene
			CreatePhysicsScene(WorldSettings);
		}

		bShouldSimulatePhysics = IVS.bShouldSimulatePhysics;
		
		// Save off the value used to create the scene, so this UWorld can recreate its scene later
		bRequiresHitProxies = IVS.bRequiresHitProxies;
		GetRendererModule().AllocateScene(this, bRequiresHitProxies, IVS.bCreateFXSystem, GetFeatureLevel());
	}

	// Prepare AI systems
	if (WorldSettings)
	{
		if (IVS.bCreateNavigation || IVS.bCreateAISystem)
		{
			if (IVS.bCreateNavigation)
			{
				FNavigationSystem::AddNavigationSystemToWorld(*this, FNavigationSystemRunMode::InvalidMode, WorldSettings->GetNavigationSystemConfig(), /*bInitializeForWorld=*/false);
			}
			if (IVS.bCreateAISystem && WorldSettings->IsAISystemEnabled())
			{
				CreateAISystem();
			}
		}
	}
	
	if (GEngine->AvoidanceManagerClass != NULL)
	{
		AvoidanceManager = NewObject<UAvoidanceManager>(this, GEngine->AvoidanceManagerClass);
	}

	SetupParameterCollectionInstances();

	if (PersistentLevel->GetOuter() != this)
	{
		// Move persistent level into world so the world object won't get garbage collected in the multi- level
		// case as it is still referenced via the level's outer. This is required for multi- level editing to work.
		PersistentLevel->Rename( *PersistentLevel->GetName(), this, REN_ForceNoResetLoaders );
	}

	Levels.Empty(1);
	Levels.Add( PersistentLevel );
	
	// If we are not Seamless Traveling remove PersistentLevel from LevelCollection if it is in a collection
	// The Level Collections will be filled already during Seamless Travel in 
	// UWorld::AsyncLoadAlwaysLoadedLevelsForSeamlessTravel()
	if (GEngine->GetWorldContextFromWorld(this) && !IsInSeamlessTravel())  
	{
		if (FLevelCollection* Collection = PersistentLevel->GetCachedLevelCollection())
		{
			Collection->RemoveLevel(PersistentLevel);
		}
	}
	
	PersistentLevel->OwningWorld = this;
	PersistentLevel->bIsVisible = true;

#if WITH_EDITOR
	RepairSingletonActors();
	RepairStreamingLevels();
#endif

	// initialize DefaultPhysicsVolume for the world
	// Spawned on demand by this function.
	DefaultPhysicsVolume = GetDefaultPhysicsVolume();

	// Find gravity
	if (GetPhysicsScene())
	{
		FVector Gravity = FVector( 0.f, 0.f, GetGravityZ() );
		GetPhysicsScene()->SetUpForFrame( &Gravity, 0, 0, 0, 0, 0, false);
	}

	// Create physics collision handler, if we have a physics scene
	if (IVS.bCreatePhysicsScene)
	{
		// First look for world override
		TSubclassOf<UPhysicsCollisionHandler> PhysHandlerClass = (WorldSettings ? WorldSettings->GetPhysicsCollisionHandlerClass() : nullptr);
		// Then fall back to engine default
		if(PhysHandlerClass == NULL)
		{
			PhysHandlerClass = GEngine->PhysicsCollisionHandlerClass;
		}

		if (PhysHandlerClass != NULL)
		{
			PhysicsCollisionHandler = NewObject<UPhysicsCollisionHandler>(this, PhysHandlerClass);
			PhysicsCollisionHandler->InitCollisionHandler();
		}
	}

	URL					= PersistentLevel->URL;
#if WITH_EDITORONLY_DATA
	CurrentLevel		= PersistentLevel;
#endif

	bAllowAudioPlayback = IVS.bAllowAudioPlayback;
	bDoDelayedUpdateCullDistanceVolumes = false;

#if WITH_EDITOR
	RepairDefaultBrush();

	if (!IsRunningCookCommandlet())
	{
		// invalidate lighting if VT is enabled but no valid VT data is present or VT is disabled and no valid non-VT data is present.
		for (auto Level : Levels) //Note: PersistentLevel is part of this array
		{
			if (Level && Level->MapBuildData)
			{
				if (Level->MapBuildData->IsLightingValid(GetFeatureLevel()) == false)
				{
					Level->MapBuildData->InvalidateSurfaceLightmaps(this);
				}
			}
		}
	}

#endif // WITH_EDITOR

	// update it's bIsDefaultLevel
	bIsDefaultLevel = (FPaths::GetBaseFilename(GetMapName()) == FPaths::GetBaseFilename(UGameMapsSettings::GetGameDefaultMap()));

	ConditionallyCreateDefaultLevelCollections();

	// We're initialized now.
	bIsWorldInitialized = true;
	bHasEverBeenInitialized = true;

	// Call the general post initialization delegates
	FWorldDelegates::OnPostWorldInitialization.Broadcast(this, IVS);

	PersistentLevel->PrecomputedVisibilityHandler.UpdateScene(Scene);
	PersistentLevel->PrecomputedVolumeDistanceField.UpdateScene(Scene);
	PersistentLevel->InitializeRenderingResources();
	PersistentLevel->OnLevelLoaded();

	IStreamingManager::Get().AddLevel(PersistentLevel);

	PostInitializeSubsystems();

	BroadcastLevelsChanged();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().AssetTagsFinalized(*this);
}

#if WITH_EDITOR
void UWorld::ReInitWorld()
{
	if (!IsInitialized())
	{
		UE_LOG(LogWorld, Warning, TEXT("ReInitWorld called on World %s, but it is not yet initialized. ReInitWorld will be ignored."), *GetPathName());
		return;
	}

	InitializationValues IVS;
	IVS.bInitializeScenes = Scene != nullptr;
	IVS.bAllowAudioPlayback = bAllowAudioPlayback;
	IVS.bRequiresHitProxies = bRequiresHitProxies;
	IVS.bCreatePhysicsScene = PhysicsScene != nullptr;
	IVS.bCreateNavigation = NavigationSystem != nullptr;
	IVS.bCreateAISystem = AISystem != nullptr;
	IVS.bShouldSimulatePhysics = bShouldSimulatePhysics;
	IVS.bEnableTraceCollision = bEnableTraceCollision;
	IVS.bForceUseMovementComponentInNonGameWorld = bForceUseMovementComponentInNonGameWorld;
	IVS.bTransactional = HasAnyFlags(RF_Transactional);
	IVS.bCreateFXSystem = FXSystem != nullptr;
	// IVS.bCreateWorldPartition; // Only used in InitializeNewWorld, not needed by InitWorld
	// IVS.DefaultGameMode; // Only used inInitializeNewWorld, not needed by InitWorld

	CleanupWorld();
	InitWorld(IVS);
	RefreshStreamingLevels();
	UpdateWorldComponents(false /* bRerunConstructionScripts */, false /* bCurrentLevelOnly */);
}

const FName UWorld::KeepInitializedDuringLoadTag(TEXT("KeepInitializedDuringLoadTag"));
#endif

void UWorld::ConditionallyCreateDefaultLevelCollections()
{
	LevelCollections.Reserve((int32)ELevelCollectionType::MAX);

	// Create main level collection. The persistent level will always be considered dynamic.
	if (!FindCollectionByType(ELevelCollectionType::DynamicSourceLevels))
	{
		// Default to the dynamic source collection
		ActiveLevelCollectionIndex = FindOrAddCollectionByType_Index(ELevelCollectionType::DynamicSourceLevels);
		LevelCollections[ActiveLevelCollectionIndex].SetPersistentLevel(PersistentLevel);
		
		// Don't add the persistent level if it is already a member of another collection.
		// This may be the case if, for example, this world is the outer of a streaming level,
		// in which case the persistent level may be in one of the collections in the streaming level's OwningWorld.
		if (PersistentLevel->GetCachedLevelCollection() == nullptr)
		{
			LevelCollections[ActiveLevelCollectionIndex].AddLevel(PersistentLevel);
		}
	}

	if (!FindCollectionByType(ELevelCollectionType::StaticLevels))
	{
		FLevelCollection& StaticCollection = FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
		StaticCollection.SetPersistentLevel(PersistentLevel);
	}
}

void UWorld::InitializeNewWorld(const InitializationValues IVS, bool bInSkipInitWorld)
{
	if (!IVS.bTransactional)
	{
		ClearFlags(RF_Transactional);
	}

	PersistentLevel = NewObject<ULevel>(this, TEXT("PersistentLevel"));
	PersistentLevel->Initialize(FURL(nullptr));
	PersistentLevel->Model = NewObject<UModel>(PersistentLevel);
	PersistentLevel->Model->Initialize(nullptr, 1);
	PersistentLevel->OwningWorld = this;

	// Create the WorldInfo actor.
	FActorSpawnParameters SpawnInfo; 

	// Mark objects are transactional for undo/ redo.
	if (IVS.bTransactional)
	{
		SpawnInfo.ObjectFlags |= RF_Transactional;
		PersistentLevel->SetFlags( RF_Transactional );
		PersistentLevel->Model->SetFlags( RF_Transactional );
	}
	else
	{
		SpawnInfo.ObjectFlags &= ~RF_Transactional;
		PersistentLevel->ClearFlags( RF_Transactional );
		PersistentLevel->Model->ClearFlags( RF_Transactional );
	}

#if WITH_EDITORONLY_DATA
	// Need to associate current level so SpawnActor doesn't complain.
	CurrentLevel = PersistentLevel;
#endif

	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Set constant name for WorldSettings to make a network replication work between new worlds on host and client
	SpawnInfo.Name = GEngine->WorldSettingsClass->GetFName();
	AWorldSettings* WorldSettings = SpawnActor<AWorldSettings>(GEngine->WorldSettingsClass, SpawnInfo );

	// Allow the world creator to override the default game mode in case they do not plan to load a level.
	if (IVS.DefaultGameMode)
	{
		WorldSettings->DefaultGameMode = IVS.DefaultGameMode;
	}

	PersistentLevel->SetWorldSettings(WorldSettings);
	check(GetWorldSettings());
#if WITH_EDITOR
	WorldSettings->SetIsTemporarilyHiddenInEditor(true);

	// Check if newly created world should be partitioned
	if (IVS.bCreateWorldPartition)
	{
		// World partition always uses actor folder objects
		FLevelActorFoldersHelper::SetUseActorFolders(PersistentLevel, true);
		PersistentLevel->ConvertAllActorsToPackaging(true);
		
		check(!GetStreamingLevels().Num());
		
		UWorldPartition* WorldPartition = UWorldPartition::CreateOrRepairWorldPartition(WorldSettings);
		WorldPartition->bEnableStreaming = IVS.bEnableWorldPartitionStreaming;
	}
#endif

	if (!bInSkipInitWorld)
	{
		// If this isn't set, the PerfTrackers are already allocated in the constructor
		if (bDisableInGamePerfTrackersForUninitializedWorlds && !PerfTrackers)
		{
			PerfTrackers = new FWorldInGamePerformanceTrackers();
		}

		// Initialize the world
		InitWorld(IVS);

		// Update components.
		const bool bRerunConstructionScripts = !FPlatformProperties::RequiresCookedData();
		UpdateWorldComponents(bRerunConstructionScripts, false);
	}
}


void UWorld::DestroyWorld( bool bInformEngineOfWorld, UWorld* NewWorld )
{
	// Clean up existing world and remove it from root set so it can be garbage collected.
	bIsLevelStreamingFrozen = false;
	SetShouldForceUnloadStreamingLevels(true);
	FlushLevelStreaming();
	CleanupWorld(true, true, NewWorld);

	ForEachNetDriver(GEngine, this, [](UNetDriver* const Driver)
	{
		if (Driver != nullptr)
		{
			check(Driver->GetNetworkObjectList().GetAllObjects().Num() == 0);
			check(Driver->GetNetworkObjectList().GetActiveObjects().Num() == 0);
		}
	});

	// Tell the engine we are destroying the world.(unless we are asked not to)
	if( ( GEngine ) && ( bInformEngineOfWorld == true ) )
	{
		GEngine->WorldDestroyed( this );
	}		
	RemoveFromRoot();
	ClearFlags(RF_Standalone);
	
	for (int32 LevelIndex=0; LevelIndex < GetNumLevels(); ++LevelIndex)
	{
		UWorld* World = CastChecked<UWorld>(GetLevel(LevelIndex)->GetOuter());
		if (World != this && World != NewWorld)
		{
			World->ClearFlags(RF_Standalone);
		}
	}
}

void UWorld::MarkObjectsPendingKill()
{
	auto MarkObjectPendingKill = [](UObject* Object)
	{
		Object->MarkAsGarbage();

		if (ULevel* Level = Cast<ULevel>(Object))
		{
			Level->CleanupReferences();
		}
	};
	ForEachObjectWithOuter(this, MarkObjectPendingKill, true, RF_NoFlags, EInternalObjectFlags::Garbage);

	MarkAsGarbage();
	bMarkedObjectsPendingKill = true;
}

UWorld* UWorld::CreateWorld(const EWorldType::Type InWorldType, bool bInformEngineOfWorld, FName WorldName, UPackage* InWorldPackage, bool bAddToRoot, ERHIFeatureLevel::Type InFeatureLevel, const InitializationValues* InIVS, bool bInSkipInitWorld)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::CreateWorld);

	if (InFeatureLevel >= ERHIFeatureLevel::Num)
	{
		InFeatureLevel = GMaxRHIFeatureLevel;
	}

	UPackage* WorldPackage = InWorldPackage;
	if ( !WorldPackage )
	{
		WorldPackage = CreatePackage(nullptr);
	}

	if (InWorldType == EWorldType::PIE)
	{
		WorldPackage->SetPackageFlags(PKG_PlayInEditor);
	}

	// Mark the package as containing a world.  This has to happen here rather than at serialization time,
	// so that e.g. the referenced assets browser will work correctly.
	if ( WorldPackage != GetTransientPackage() )
	{
		WorldPackage->ThisContainsMap();
	}

	// Create new UWorld, ULevel and UModel.
	const FString WorldNameString = (WorldName != NAME_None) ? WorldName.ToString() : TEXT("Untitled");
	UWorld* NewWorld = NewObject<UWorld>(WorldPackage, *WorldNameString);
	NewWorld->SetFlags(RF_Transactional);
	NewWorld->WorldType = InWorldType;
	NewWorld->SetFeatureLevel(InFeatureLevel);
	NewWorld->InitializeNewWorld(InIVS ? *InIVS : UWorld::InitializationValues().CreatePhysicsScene(InWorldType != EWorldType::Inactive).ShouldSimulatePhysics(false).EnableTraceCollision(true).CreateNavigation(InWorldType == EWorldType::Editor).CreateAISystem(InWorldType == EWorldType::Editor), bInSkipInitWorld);

	// Clear the dirty flags set during SpawnActor and UpdateLevelComponents
	WorldPackage->SetDirtyFlag(false);
	for (UPackage* ExternalPackage : WorldPackage->GetExternalPackages())
	{
		ExternalPackage->SetDirtyFlag(false);
	}

	if ( bAddToRoot )
	{
		// Add to root set so it doesn't get garbage collected.
		NewWorld->AddToRoot();
	}

	// Tell the engine we are adding a world (unless we are asked not to)
	if( ( GEngine ) && ( bInformEngineOfWorld == true ) )
	{
		GEngine->WorldAdded( NewWorld );
	}

	return NewWorld;
}

void UWorld::RemoveActor(AActor* Actor, bool bShouldModifyLevel) const
{
	if (ULevel* CheckLevel = Actor->GetLevel())
	{
		const int32 ActorListIndex = CheckLevel->Actors.Find(Actor);
		// Search the entire list.
		if (ActorListIndex != INDEX_NONE)
		{
			if (bShouldModifyLevel && GUndo)
			{
				ModifyLevel(CheckLevel);
			}

			if (!IsGameWorld())
			{
				CheckLevel->Actors[ActorListIndex]->Modify();
			}

			CheckLevel->Actors[ActorListIndex] = nullptr;

			CheckLevel->ActorsForGC.RemoveSwap(Actor);
		}
	}

	// Remove actor from network list
	RemoveNetworkActor( Actor );
}


bool UWorld::ContainsActor( AActor* Actor ) const
{
	return (Actor && Actor->GetWorld() == this);
}

bool UWorld::AllowAudioPlayback() const
{
	return bAllowAudioPlayback;
}

#if WITH_EDITOR
void UWorld::ShrinkLevel()
{
	GetModel()->ShrinkModel();
}
#endif // WITH_EDITOR

void UWorld::ClearWorldComponents()
{
	for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = Levels[LevelIndex];
		Level->ClearLevelComponents();
	}

	if(LineBatcher && LineBatcher->IsRegistered())
	{
		LineBatcher->UnregisterComponent();
	}

	if(PersistentLineBatcher && PersistentLineBatcher->IsRegistered())
	{
		PersistentLineBatcher->UnregisterComponent();
	}

	if(ForegroundLineBatcher && ForegroundLineBatcher->IsRegistered())
	{
		ForegroundLineBatcher->UnregisterComponent();
	}

	if (PhysicsField && PhysicsField->IsRegistered())
	{
		PhysicsField->UnregisterComponent();
	}
}


void UWorld::UpdateWorldComponents(bool bRerunConstructionScripts, bool bCurrentLevelOnly, FRegisterComponentContext* Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::UpdateWorldComponents);

#if !WITH_EDITOR
	ensure(!bRerunConstructionScripts);
#endif

	if ( !IsRunningDedicatedServer() )
	{
		if(!LineBatcher)
		{
			LineBatcher = NewObject<ULineBatchComponent>();
			LineBatcher->bCalculateAccurateBounds = false;
		}

		if(!LineBatcher->IsRegistered())
		{	
			LineBatcher->RegisterComponentWithWorld(this, Context);
		}

		if(!PersistentLineBatcher)
		{
			PersistentLineBatcher = NewObject<ULineBatchComponent>();
			PersistentLineBatcher->bCalculateAccurateBounds = false;
		}

		if(!PersistentLineBatcher->IsRegistered())	
		{
			PersistentLineBatcher->RegisterComponentWithWorld(this, Context);
		}

		if(!ForegroundLineBatcher)
		{
			ForegroundLineBatcher = NewObject<ULineBatchComponent>();
			ForegroundLineBatcher->bCalculateAccurateBounds = false;
		}

		if(!ForegroundLineBatcher->IsRegistered())	
		{
			ForegroundLineBatcher->RegisterComponentWithWorld(this, Context);
		}

		static IConsoleVariable* PhysicsFieldEnableClipmapCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PhysicsField.EnableField"));
		if (PhysicsFieldEnableClipmapCVar && PhysicsFieldEnableClipmapCVar->GetInt() == 1 && Scene && !IsMobilePlatform(Scene->GetShaderPlatform()))
		{
			if (!PhysicsField)
			{
				PhysicsField = NewObject<UPhysicsFieldComponent>();
			}

			if (!PhysicsField->IsRegistered())
			{
				PhysicsField->RegisterComponentWithWorld(this, Context);
			}
		}
	}

	if ( bCurrentLevelOnly )
	{
#if !WITH_EDITORONLY_DATA
		ULevel* CurrentLevel = PersistentLevel;
#endif
		check( CurrentLevel );
		CurrentLevel->UpdateLevelComponents(bRerunConstructionScripts, Context);
	}
	else
	{
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(Level);
			// Update the level only if it is visible (or not a streamed level)
			if(!StreamingLevel || Level->bIsVisible)
			{
				Level->UpdateLevelComponents(bRerunConstructionScripts, Context);
				IStreamingManager::Get().AddLevel(Level);
			}
		}
	}

	const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
	for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
	{
		WorldSubsystem->OnWorldComponentsUpdated(*this);
	}

	UpdateCullDistanceVolumes();
}


bool UWorld::UpdateCullDistanceVolumes(AActor* ActorToUpdate, UPrimitiveComponent* ComponentToUpdate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::UpdateCullDistanceVolumes);

	// Map that will store new max draw distance for every primitive
	TMap<UPrimitiveComponent*,float> CompToNewMaxDrawMap;
	bool bUpdatedDrawDistances = false;

	// Keep track of time spent.
	double Duration = 0.0;
	{
		SCOPE_SECONDS_COUNTER(Duration);

		TArray<ACullDistanceVolume*> CullDistanceVolumes;

		auto EvaluateComponent = [&bUpdatedDrawDistances, &CompToNewMaxDrawMap](UPrimitiveComponent* PrimitiveComponent)
		{
			if (ACullDistanceVolume::CanBeAffectedByVolumes(PrimitiveComponent))
			{
				CompToNewMaxDrawMap.Add(PrimitiveComponent, PrimitiveComponent->LDMaxDrawDistance);
				if (!bUpdatedDrawDistances && !FMath::IsNearlyEqual(PrimitiveComponent->LDMaxDrawDistance, PrimitiveComponent->CachedMaxDrawDistance))
				{
					bUpdatedDrawDistances = true;
				}
			}
		};

		// Establish base line of LD specified cull distances.
		if (ActorToUpdate || ComponentToUpdate)
		{
			if (ComponentToUpdate)
			{
				check((ActorToUpdate == nullptr) || (ActorToUpdate == ComponentToUpdate->GetOwner()));
				EvaluateComponent(ComponentToUpdate);
			}
			else
			{
				for (UActorComponent* ActorComponent : ActorToUpdate->GetComponents())
				{
					if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ActorComponent))
					{
						EvaluateComponent(PrimitiveComponent);
					}
				}
			}

			if (CompToNewMaxDrawMap.Num() > 0)
			{
				for (TActorIterator<ACullDistanceVolume> It(this); It; ++It)
				{
					CullDistanceVolumes.Add(*It);
				}
			}
		}
		else
		{
			for( AActor* Actor : FActorRange(this) )
			{
				for (UActorComponent* ActorComponent : Actor->GetComponents())
				{
					if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ActorComponent))
					{
						EvaluateComponent(PrimitiveComponent);
					}
				}

				ACullDistanceVolume* CullDistanceVolume = Cast<ACullDistanceVolume>(Actor);
				if (CullDistanceVolume)
				{
					CullDistanceVolumes.Add(CullDistanceVolume);
				}
			}
		}

		if (CullDistanceVolumes.Num() > 0 && CompToNewMaxDrawMap.Num() > 0)
		{
			// Iterate over all cull distance volumes and get new cull distances.
			for (ACullDistanceVolume* CullDistanceVolume : CullDistanceVolumes)
			{
				CullDistanceVolume->GetPrimitiveMaxDrawDistances(CompToNewMaxDrawMap);
			}

			bUpdatedDrawDistances = true;
		}

		// Only perform the update if we actually have cull distance volumes 
		// or we found a component that had a cached value different than the LD set value
		if (bUpdatedDrawDistances)
		{
			// Finally, go over all primitives, and see if they need to change.
			// Only if they do do we reregister them, as that's slow.
			for (TMap<UPrimitiveComponent*, float>::TIterator It(CompToNewMaxDrawMap); It; ++It)
			{
				UPrimitiveComponent* PrimComp = It.Key();
				const float NewMaxDrawDist = It.Value();

				PrimComp->SetCachedMaxDrawDistance(NewMaxDrawDist);
			}
		}
	}

	if( Duration > 1.f )
	{
		UE_LOG(LogWorld, Log, TEXT("Updating cull distance volumes took %5.2f seconds"),Duration);
	}

	return bUpdatedDrawDistances;
}

void UWorld::ModifyLevel(ULevel* Level) const
{
	if( Level && Level->HasAnyFlags(RF_Transactional))
	{
		Level->Modify( false );
		//Level->Actors.ModifyAllItems();
		Level->Model->Modify( false );
	}
}

void UWorld::EnsureCollisionTreeIsBuilt()
{
	if (bInTick || bIsBuilt)
	{
		// Current implementation of collision tree rebuild ticks physics scene and can not be called during world tick
		return;
	}

	if (GIsEditor && !IsPlayInEditor())
	{
		// Don't simulate physics in the editor
		return;
	}
	
	// Set physics to static loading mode
	if (PhysicsScene)
	{
		PhysicsScene->EnsureCollisionTreeIsBuilt(this);
	}

	bIsBuilt = true;
}

void UWorld::InvalidateModelGeometry(ULevel* InLevel)
{
	if ( InLevel )
	{
		InLevel->InvalidateModelGeometry();
	}
	else
	{
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			Level->InvalidateModelGeometry();
		}
	}
}


void UWorld::InvalidateModelSurface(bool bCurrentLevelOnly)
{
	if ( bCurrentLevelOnly )
	{
#if !WITH_EDITORONLY_DATA
		ULevel* CurrentLevel = PersistentLevel;
#endif
		check( bCurrentLevelOnly );
		CurrentLevel->InvalidateModelSurface();
	}
	else
	{
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = Levels[LevelIndex];
			Level->InvalidateModelSurface();
		}
	}
}


void UWorld::CommitModelSurfaces()
{
	for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel* Level = Levels[LevelIndex];
		Level->CommitModelSurfaces();
	}
}


void UWorld::TransferBlueprintDebugReferences(UWorld* NewWorld)
{
#if WITH_EDITOR
	// First create a list of blueprints that already exist in the new world
	TArray< FString > NewWorldExistingBlueprintNames;
	for (FBlueprintToDebuggedObjectMap::TIterator It(NewWorld->BlueprintObjectsBeingDebugged); It; ++It)
	{
		if (UBlueprint* TargetBP = It.Key().Get())
		{	
			NewWorldExistingBlueprintNames.AddUnique( TargetBP->GetName() );
		}
	}

	// Move debugging object associations across from the old world to the new world that are not already there
	for (FBlueprintToDebuggedObjectMap::TIterator It(BlueprintObjectsBeingDebugged); It; ++It)
	{
		if (UBlueprint* TargetBP = It.Key().Get())
		{	
			FString SourceName = TargetBP->GetName();
			// If this blueprint is not already listed in the ones bieng debugged in the new world, add it.
			if( NewWorldExistingBlueprintNames.Find( SourceName ) == INDEX_NONE )			
			{
				TWeakObjectPtr<UObject>& WeakTargetObject = It.Value();
				UObject* NewTargetObject = nullptr;
				bool bForceClear = false;

				if (WeakTargetObject.IsValid())
				{
					UObject* OldTargetObject = WeakTargetObject.Get();
					check(OldTargetObject);

					// We don't map from PIE objects in a client world back to editor objects, as that will transfer to the server on the next execution
					if (GetNetMode() != NM_Client)
					{
						NewTargetObject = FindObject<UObject>(NewWorld, *OldTargetObject->GetPathName(this));

						// if we didn't find the object in the Persistent level, we may need to look in WorldPartition sublevels
						if (!NewTargetObject && NewWorld->GetWorldSettings()->IsPartitionedWorld())
						{
							const FString OldTargetPathName = OldTargetObject->GetPathName(PersistentLevel);
							for (TObjectPtr<ULevelStreaming>& StreamingLevel : NewWorld->StreamingLevels)
							{
								if (StreamingLevel && StreamingLevel->GetLoadedLevel())
								{
									NewTargetObject = FindObject<UObject>(StreamingLevel->GetLoadedLevel(), *OldTargetPathName);
									if (NewTargetObject)
									{
										break;
									}
								}
							}
						}
					}
				}

				if (NewTargetObject != nullptr)
				{
					// Check to see if the object we found to transfer to is of a different class.  LevelScripts are always exceptions, because a new level may have been loaded in PIE, and we have special handling for LSA debugging objects
					if (!NewTargetObject->IsA(TargetBP->GeneratedClass))
					{
						bForceClear = true;
						const FString BlueprintFullPath = TargetBP->GetPathName();

						if (BlueprintFullPath.StartsWith(TEXT("/Temp/Autosaves")) || BlueprintFullPath.StartsWith(TEXT("/Temp//Autosaves")))
						{
							// This map was an autosave for networked PIE; it's OK to fail to fix
							// up the blueprint object being debugged reference as the whole blueprint
							// is going away.
						}
						else if(!NewTargetObject->IsA(ALevelScriptActor::StaticClass()))
						{
							// Let the ensure fire
							UE_LOG(LogWorld, Warning, TEXT("Found object to debug in main world that isn't the correct type"));
							UE_LOG(LogWorld, Warning, TEXT("  TargetBP path is %s"), *TargetBP->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  TargetBP gen class path is %s"), *TargetBP->GeneratedClass->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  NewTargetObject path is %s"), *NewTargetObject->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  NewTargetObject class path is %s"), *NewTargetObject->GetClass()->GetPathName());

							UObject* OldTargetObject = WeakTargetObject.Get();
							UE_LOG(LogWorld, Warning, TEXT("  OldObject path is %s"), *OldTargetObject->GetPathName());
							UE_LOG(LogWorld, Warning, TEXT("  OldObject class path is %s"), *OldTargetObject->GetClass()->GetPathName());

							ensureMsgf(false, TEXT("Failed to find an appropriate object to debug back in the editor world"));
						}

						NewTargetObject = nullptr;
					}
				}

				if (NewTargetObject || bForceClear)
				{
					TargetBP->SetObjectBeingDebugged(NewTargetObject);
				}
				else
				{
					// We do not explicitly clear a null target, because ObjectPathToDebug may refer to late spawned actor
					TargetBP->UnregisterObjectBeingDebugged();
				}
			}
		}
	}

	// Empty the map, anything useful got moved over the map in the new world
	BlueprintObjectsBeingDebugged.Empty();
#endif	//#if WITH_EDITOR
}


void UWorld::NotifyOfBlueprintDebuggingAssociation(class UBlueprint* Blueprint, UObject* DebugObject)
{
#if WITH_EDITOR
	TWeakObjectPtr<UBlueprint> Key(Blueprint);

	if (DebugObject)
	{
		BlueprintObjectsBeingDebugged.FindOrAdd(Key) = DebugObject;
	}
	else
	{
		BlueprintObjectsBeingDebugged.Remove(Key);
	}
#endif	//#if WITH_EDITOR
}

void UWorld::BroadcastLevelsChanged()
{
	LevelsChangedEvent.Broadcast();
#if WITH_EDITOR
	FWorldDelegates::RefreshLevelScriptActions.Broadcast(this);
#endif	//#if WITH_EDITOR
}

DEFINE_STAT(STAT_AddToWorldTime);
DEFINE_STAT(STAT_RemoveFromWorldTime);
DEFINE_STAT(STAT_UpdateLevelStreamingTime);
DEFINE_STAT(STAT_ManageLevelsToConsider);
DEFINE_STAT(STAT_UpdateStreamingState);

/**
 * Static helper function for Add/RemoveToWorld to determine whether we've already spent all the allotted time.
 *
 * @param	CurrentTask		Description of last task performed
 * @param	StartTime		StartTime, used to calculate time passed
 * @param	Level			Level work has been performed on
 * @param	TimeLimit		The amount of time that is allowed to be used
 *
 * @return true if time limit has been exceeded, false otherwise
 */
static bool IsTimeLimitExceeded( const TCHAR* CurrentTask, double StartTime, ULevel* Level, double TimeLimit )
{
	bool bIsTimeLimitExceed = false;

	double CurrentTime	= FPlatformTime::Seconds();
	// Delta time in ms.
	double DeltaTime	= (CurrentTime - StartTime) * 1000;
	if( DeltaTime > TimeLimit )
	{
		// Log if a single event took way too much time.
		if( DeltaTime > 20.0 )
		{
			UE_LOG(LogStreaming, Display, TEXT("UWorld::AddToWorld: %s for %s took (less than) %5.2f ms"), CurrentTask, *Level->GetOutermost()->GetName(), DeltaTime );
		}
		bIsTimeLimitExceed = true;
	}

	return bIsTimeLimitExceed;
}

#if PERF_TRACK_DETAILED_ASYNC_STATS
// Variables for tracking how long each part of the AddToWorld process takes
double MoveActorTime = 0.0;
double ShiftActorsTime = 0.0;
double UpdateComponentsTime = 0.0;
double InitBSPPhysTime = 0.0;
double InitActorPhysTime = 0.0;
double InitActorTime = 0.0;
double RouteActorInitializeTime = 0.0;
double CrossLevelRefsTime = 0.0;
double SortActorListTime = 0.0;
double PerformLastStepTime = 0.0;

/** Helper class, to add the time between this objects creating and destruction to passed in variable. */
class FAddWorldScopeTimeVar
{
public:
	FAddWorldScopeTimeVar(double* Time)
	{
		TimeVar = Time;
		Start = FPlatformTime::Seconds();
	}

	~FAddWorldScopeTimeVar()
	{
		*TimeVar += (FPlatformTime::Seconds() - Start);
	}

private:
	/** Pointer to value to add to when object is destroyed */
	double* TimeVar;
	/** The time at which this object was created */
	double	Start;
};

/** Macro to create a scoped timer for the supplied var */
#define SCOPE_TIME_TO_VAR(V) FAddWorldScopeTimeVar TimeVar(V)

#else

/** Empty macro, when not doing timing */
#define SCOPE_TIME_TO_VAR(V)

#endif // PERF_TRACK_DETAILED_ASYNC_STATS

// Cumulated time passed in UWorld::AddToWorld since last call to UWorld::UpdateLevelStreaming.
static double GAddToWorldTimeCumul = 0.0;

// Only called internally by UWorld::AddToWorld and UWorld::InitializeActorsForPlay
static void ResetLevelFlagsOnLevelAddedToWorld(ULevel* Level)
{
	Level->bAlreadyShiftedActors = false;
	Level->bAlreadyUpdatedComponents = false;
	Level->bAlreadySortedActorList = false;
	Level->bAlreadyClearedActorsSeamlessTravelFlag = false;
	Level->ResetRouteActorInitializationState();
}

bool UWorld::SupportsMakingVisibleTransactionRequests() const
{
	if (GetWorld()->IsGameWorld())
	{
		// We don't support this flag to change dynamically (read ULevelStreaming CVar once)
		if (!bSupportsMakingVisibleTransactionRequests.IsSet())
		{
			bSupportsMakingVisibleTransactionRequests = ULevelStreaming::DefaultAllowClientUseMakingVisibleTransactionRequests() || (IsPartitionedWorld() && GetWorldPartition()->UseMakingVisibleTransactionRequests());
		}
		return bSupportsMakingVisibleTransactionRequests.GetValue();
	}
	return false;
}

bool UWorld::SupportsMakingInvisibleTransactionRequests() const
{
	if (GetWorld()->IsGameWorld())
	{
		// We don't support this flag to change dynamically (read ULevelStreaming CVar once)
		if (!bSupportsMakingInvisibleTransactionRequests.IsSet())
		{
			bSupportsMakingInvisibleTransactionRequests = ULevelStreaming::DefaultAllowClientUseMakingInvisibleTransactionRequests() || (IsPartitionedWorld() && GetWorldPartition()->UseMakingInvisibleTransactionRequests());
		}
		return bSupportsMakingInvisibleTransactionRequests.GetValue();
	}
	return false;
}

const AServerStreamingLevelsVisibility* UWorld::GetServerStreamingLevelsVisibility() const
{
	check(!IsValid(ServerStreamingLevelsVisibility) || SupportsMakingVisibleTransactionRequests());
	return ServerStreamingLevelsVisibility;
}

void UWorld::AddToWorld( ULevel* Level, const FTransform& LevelTransform, bool bConsiderTimeLimit, FNetLevelVisibilityTransactionId TransactionId, ULevelStreaming* InOwningLevelStreaming)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AddToWorld);
	SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(AddToWorld);

	check(IsValid(Level));
	check(!Level->IsUnreachable());

	FScopeCycleCounterUObject ContextScope(Level);

	// If not provided, find the owning streaming level
	ULevelStreaming* OwningLevelStreaming = InOwningLevelStreaming ? InOwningLevelStreaming : FLevelUtils::FindStreamingLevel(Level);

	// Set flags to indicate that we are associating a level with the world to e.g. perform slower/ better octree insertion 
	// and such, as opposed to the fast path taken for run-time/ gameplay objects.
	Level->bIsAssociatingLevel = true;

	const double StartTime = FPlatformTime::Seconds();

	bool bExecuteNextStep = CurrentLevelPendingVisibility == Level || CurrentLevelPendingVisibility == NULL;
	bool bPerformedLastStep	= false;
	const bool bIsGameWorld = IsGameWorld();

	if (bExecuteNextStep)
	{
		// Once CurrentLevelPendingVisibility is set, don't call CanAddLoadedLevelToWorld as we must finish processing CurrentLevelPendingVisibility
		if ((CurrentLevelPendingVisibility == nullptr) && !CanAddLoadedLevelToWorld(Level))
		{
			bExecuteNextStep = false;
		}
	}

	// Don't consider the time limit if the match hasn't started as we need to ensure that the levels are fully loaded
	bConsiderTimeLimit &= bMatchStarted && bIsGameWorld;
	double TimeLimit = 0.0;

	if (bExecuteNextStep && bConsiderTimeLimit)
	{
		float ExtraTimeLimit = 0.0f;
		// Give the actor initialization code more time if we're performing a high priority load or are in seamless travel
		if (AWorldSettings* WorldSettings = GetWorldSettings(false, false))
		{
			if (WorldSettings->bHighPriorityLoading || WorldSettings->bHighPriorityLoadingLocal || IsInSeamlessTravel())
			{
				ExtraTimeLimit = GPriorityLevelStreamingActorsUpdateExtraTime;
			}
		}
		if (GAdaptiveAddToWorld.IsEnabled())
		{
			GAdaptiveAddToWorld.SetExtraTimeSlice(ExtraTimeLimit);
			TimeLimit = GAdaptiveAddToWorld.GetTimeSlice();
		}
		else
		{
			TimeLimit = GLevelStreamingActorsUpdateTimeLimit + ExtraTimeLimit;
		}

		// Remove cumulated time since UpdateLevelStreaming
		TimeLimit = FMath::Max<double>(0.0, TimeLimit - GAddToWorldTimeCumul);
	}

	// Don't make this level visible if it's currently being made invisible
	if( bExecuteNextStep && CurrentLevelPendingVisibility == NULL && CurrentLevelPendingInvisibility != Level )
	{
		TRACE_BEGIN_REGION(*WriteToString<256>(TEXT("AddToWorld: "), Level->GetOutermost()->GetName()));

		Level->OwningWorld = this;

		if (OwningLevelStreaming)
		{
			FLevelStreamingDelegates::OnLevelBeginMakingVisible.Broadcast(this, OwningLevelStreaming, Level);
		}
		
		// Mark level as being the one in process of being made visible.
		CurrentLevelPendingVisibility = Level;

		// Add to the UWorld's array of levels, which causes it to be rendered et al.
		Levels.AddUnique( Level );
		
#if PERF_TRACK_DETAILED_ASYNC_STATS
		MoveActorTime = 0.0;
		ShiftActorsTime = 0.0;
		UpdateComponentsTime = 0.0;
		InitBSPPhysTime = 0.0;
		InitActorPhysTime = 0.0;
		InitActorTime = 0.0;
		RouteActorInitializeTime = 0.0;
		CrossLevelRefsTime = 0.0;
		SortActorListTime = 0.0;
		PerformLastStepTime = 0.0;
#endif

		bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded(TEXT("begin making visible"), StartTime, Level, TimeLimit));
	}

	int32 StartWorkUnitsRemaining = Level->GetEstimatedAddToWorldWorkUnitsRemaining();

	if( bExecuteNextStep && !Level->bAlreadyMovedActors )
	{
		CSV_SCOPED_TIMING_STAT(LevelStreamingDetail, AddToWorld_MoveActors);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_MoveActors);
		SCOPE_TIME_TO_VAR(&MoveActorTime);

		FLevelUtils::FApplyLevelTransformParams TransformParams(Level, LevelTransform);
		TransformParams.bSetRelativeTransformDirectly = true;
		FLevelUtils::ApplyLevelTransform(TransformParams);

		Level->bAlreadyMovedActors = true;
		bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("moving actors"), StartTime, Level, TimeLimit));
	}

	if( bExecuteNextStep && !Level->bAlreadyShiftedActors )
	{
		CSV_SCOPED_TIMING_STAT(LevelStreamingDetail, AddToWorld_ShiftActors);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_ShiftActors);
		SCOPE_TIME_TO_VAR(&ShiftActorsTime);

		// Notify world composition: will place level actors according to current world origin
		if (WorldComposition)
		{
			WorldComposition->OnLevelAddedToWorld(Level);
		}
		
		Level->bAlreadyShiftedActors = true;
		bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("shifting actors"), StartTime, Level, TimeLimit ));
	}

#if WITH_EDITOR
	AssetCompilation::FProcessAsyncTaskParams ProcessAsyncTasksParams;
	ProcessAsyncTasksParams.bPlayInEditorAssetsOnly = true;

	if (bExecuteNextStep)
	{
		// Wait on any async DDC handles
		if (AsyncPreRegisterDDCRequests.Num())
		{
			if (!bConsiderTimeLimit)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_World_AddToWorld_WaitFor_AsyncPreRegisterLevelStreamingTasks);

				for (TSharedPtr<FAsyncPreRegisterDDCRequest>& Request : AsyncPreRegisterDDCRequests)
				{
					Request->WaitAsynchronousCompletion();
				}
				AsyncPreRegisterDDCRequests.Empty();
			}
			else
			{
				for (int32 Index = 0; Index < AsyncPreRegisterDDCRequests.Num(); Index++)
				{
					if (AsyncPreRegisterDDCRequests[Index]->PollAsynchronousCompletion())
					{
						AsyncPreRegisterDDCRequests.RemoveAtSwap(Index--);
					}
					else
					{
						bExecuteNextStep = false;
						break;
					}
				}
			}
		}

		// Gives a chance to any assets being used for PIE/game to complete
		FAssetCompilingManager::Get().ProcessAsyncTasks(ProcessAsyncTasksParams);
	}
#endif

	// Updates the level components (Actor components and UModelComponents).
	if( bExecuteNextStep && !Level->bAlreadyUpdatedComponents )
	{
		CSV_SCOPED_TIMING_STAT(LevelStreamingDetail, AddToWorld_UpdateComponents);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_UpdatingComponents);
		SCOPE_TIME_TO_VAR(&UpdateComponentsTime);

		// Make sure code thinks components are not currently attached.
		Level->bAreComponentsCurrentlyRegistered = false;

#if WITH_EDITOR
			// Pretend here that we are loading package to avoid package dirtying during components registration
		TGuardValue<bool> IsEditorLoadingPackage(GIsEditorLoadingPackage, (GIsEditor ? true : GIsEditorLoadingPackage));
#endif

		// We don't need to rerun construction scripts if we have cooked data or we are playing in editor unless the PIE world was loaded
		// from disk rather than duplicated
		const bool bRerunConstructionScript = !FPlatformProperties::RequiresCookedData() && (!bIsGameWorld || !Level->bHasRerunConstructionScripts);

		// Prepare context used to store batch/parallelize AddPrimitive calls
		FRegisterComponentContext Context(this);
		FRegisterComponentContext* ContextPtr = GLevelStreamingAddPrimitiveGranularity == 0 ? nullptr : &Context;

		// Incrementally update components.
		int32 NumComponentsToUpdate = (!bConsiderTimeLimit || !bIsGameWorld || IsRunningCommandlet() ? 0 : GLevelStreamingComponentsRegistrationGranularity);
		do
		{
			Level->IncrementalUpdateComponents( NumComponentsToUpdate, bRerunConstructionScript, ContextPtr);
			// Process AddPrimitives if threshold is reached
			if (Context.Count() > GLevelStreamingAddPrimitiveGranularity)
			{
				Context.Process();
			}
		}
		while (!Level->bAreComponentsCurrentlyRegistered && !IsTimeLimitExceeded(TEXT("updating components"), StartTime, Level, TimeLimit));
		
		// Process remaining AddPrimitives
		Context.Process();

		// We are done once all components are attached.
		Level->bAlreadyUpdatedComponents	= Level->bAreComponentsCurrentlyRegistered;
		bExecuteNextStep					= Level->bAreComponentsCurrentlyRegistered && (!bConsiderTimeLimit || !IsTimeLimitExceeded(TEXT("updating components"), StartTime, Level, TimeLimit));
	}

#if WITH_EDITOR
	// Gives a chance to any assets being used for PIE/game to complete before calling
	// BeginPlay on all actors
	FAssetCompilingManager::Get().ProcessAsyncTasks(ProcessAsyncTasksParams);
#endif

	if( bIsGameWorld && AreActorsInitialized() )
	{
		// Initialize all actors and start execution.
		if (bExecuteNextStep && !(Level->bAlreadyInitializedNetworkActors && Level->bAlreadyClearedActorsSeamlessTravelFlag))
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_InitializeNetworkActors);
			CSV_SCOPED_TIMING_STAT(LevelStreamingDetail, AddToWorld_InitNetworkActors);
			SCOPE_TIME_TO_VAR(&InitActorTime);

			// InitializeNetworkActors only needs to be called the first time a level is loaded,
			// (not on visibility changes). However, we always need to clear the seamless travel flag.
			// InitializeNetworkActors will implicitly clear the flag though, so we should only
			// ever need to call one of these methods while making a level visible.
			if (!Level->bAlreadyInitializedNetworkActors)
			{
				Level->InitializeNetworkActors();
			}
			else
			{
				Level->ClearActorsSeamlessTraveledFlag();
			}

			const double PreventNextStepTimeLimit = GLevelStreamingForceRouteActorInitializeNextFrame ? 0.0 : TimeLimit;
			bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("initializing network actors"), StartTime, Level, PreventNextStepTimeLimit ));
		}

		// Route various initialization functions and set volumes.
		if (bExecuteNextStep && !Level->IsFinishedRouteActorInitialization())
		{
			CSV_SCOPED_TIMING_STAT(LevelStreamingDetail, AddToWorld_RouteActorInit);
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_RouteActorInitialize);
			SCOPE_TIME_TO_VAR(&RouteActorInitializeTime);
			const int32 NumActorsToProcess = (!bConsiderTimeLimit || !bIsGameWorld || IsRunningCommandlet()) ? 0 : GLevelStreamingRouteActorInitializationGranularity;
			bStartup = 1;
			do 
			{
				Level->RouteActorInitialize(NumActorsToProcess);
			} while (!Level->IsFinishedRouteActorInitialization() && !IsTimeLimitExceeded(TEXT("routing Initialize on actors"), StartTime, Level, TimeLimit));
			bStartup = 0;

			bExecuteNextStep = Level->IsFinishedRouteActorInitialization() && (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("routing Initialize on actors"), StartTime, Level, TimeLimit ));
		}

		// Sort the actor list; can't do this on save as the relevant properties for sorting might have been changed by code
		if( bExecuteNextStep && !Level->bAlreadySortedActorList )
		{
			CSV_SCOPED_TIMING_STAT(LevelStreamingDetail, AddToWorld_SortActorList);
			SCOPE_TIME_TO_VAR(&SortActorListTime);

			Level->SortActorList();
			Level->bAlreadySortedActorList = true;
			bExecuteNextStep = (!bConsiderTimeLimit || !IsTimeLimitExceeded( TEXT("sorting actor list"), StartTime, Level, TimeLimit ));
			bPerformedLastStep = true;
		}
	}
	else
	{
		bPerformedLastStep = true;
	}

	Level->bIsAssociatingLevel = false;

	// We're done.
	if( bPerformedLastStep )
	{
		SCOPE_TIME_TO_VAR(&PerformLastStepTime);
		CSV_CUSTOM_STAT(LevelStreamingDetail, AddToWorldLevelCompleteCount, 1, ECsvCustomStatOp::Accumulate);
		
		ResetLevelFlagsOnLevelAddedToWorld(Level);

		// Finished making level visible - allow other levels to be added to the world.
		CurrentLevelPendingVisibility = nullptr;

		if (bIsGameWorld && !Level->bClientOnlyVisible)
		{
			FUpdateLevelVisibilityLevelInfo LevelVisibility(Level, true);
			const FName UnmappedPackageName = LevelVisibility.PackageName;

			// notify server that the client has finished making this level visible
			for (FLocalPlayerIterator It(GEngine, this); It; ++It)
			{
				if (APlayerController* LocalPlayerController = It->GetPlayerController(this))
				{
					LevelVisibility.PackageName = LocalPlayerController->NetworkRemapPath(UnmappedPackageName, false);
					LevelVisibility.VisibilityRequestId = TransactionId;
					LocalPlayerController->ServerUpdateLevelVisibility(LevelVisibility);
				}
			}
			// update server's visible levels
			if (IsValid(ServerStreamingLevelsVisibility))
			{
				check(IsNetMode(NM_ListenServer) || IsNetMode(NM_DedicatedServer));
				ServerStreamingLevelsVisibility->SetIsVisible(OwningLevelStreaming, true);
			}
		}

		// Set visibility before adding the rendering resource and adding to streaming.
		Level->bIsVisible = true;

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_InitializeRenderingResources);
			Level->InitializeRenderingResources();
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AddToWorldTime_NotifyLevelVisible);

			// Notify the texture streaming system now that everything is set up.
			IStreamingManager::Get().AddLevel(Level);

			// send a callback that a level was added to the world
			FWorldDelegates::LevelAddedToWorld.Broadcast(Level, this);

			BroadcastLevelsChanged();

			ULevelStreaming::BroadcastLevelVisibleStatus(this, Level->GetOutermost()->GetFName(), true);
		}

		TRACE_END_REGION(*WriteToString<256>(TEXT("AddToWorld: "), Level->GetOutermost()->GetName()));
	}
#if CSV_PROFILER
	else
	{
		CSV_CUSTOM_STAT(LevelStreamingDetail, AddToWorldLevelIncompleteCount, 1, ECsvCustomStatOp::Accumulate);
	}
#endif

#if PERF_TRACK_DETAILED_ASYNC_STATS
	if (bPerformedLastStep)
	{
		// Log out all of the timing information
		double TotalTime = 
			MoveActorTime + 
			ShiftActorsTime + 
			UpdateComponentsTime + 
			InitBSPPhysTime + 
			InitActorPhysTime + 
			InitActorTime + 
			RouteActorInitializeTime + 
			CrossLevelRefsTime + 
			SortActorListTime + 
			PerformLastStepTime;

		UE_LOG(LogStreaming, Display, TEXT("Detailed AddToWorld stats for '%s' - Total %6.2fms"), *Level->GetOutermost()->GetName(), TotalTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Move Actors             : %6.2f ms"), MoveActorTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Shift Actors            : %6.2f ms"), ShiftActorsTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Update Components       : %6.2f ms"), UpdateComponentsTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Init BSP Phys           : %6.2f ms"), InitBSPPhysTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Init Actor Phys         : %6.2f ms"), InitActorPhysTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Init Actors             : %6.2f ms"), InitActorTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Initialize              : %6.2f ms"), RouteActorInitializeTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Cross Level Refs        : %6.2f ms"), CrossLevelRefsTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Sort Actor List         : %6.2f ms"), SortActorListTime * 1000 );
		UE_LOG(LogStreaming, Display, TEXT("Perform Last Step       : %6.2f ms"), PerformLastStepTime * 1000 );
	}
#endif // PERF_TRACK_DETAILED_ASYNC_STATS

	// Delta time in ms.
	double DeltaTime = (FPlatformTime::Seconds() - StartTime) * 1000; 
	GAddToWorldTimeCumul += DeltaTime;

	// Register work completed with adaptive level streaming
	if (GAdaptiveAddToWorld.IsEnabled())
	{
		GAdaptiveAddToWorld.RegisterAddToWorldWork(StartWorkUnitsRemaining, Level->GetEstimatedAddToWorldWorkUnitsRemaining(), Level->GetEstimatedAddToWorldWorkUnitsTotal());
	}
}

void UWorld::BeginTearingDown()
{
	bIsTearingDown = true;
	UE_LOG(LogWorld, Log, TEXT("BeginTearingDown for %s"), *GetOutermost()->GetName());

	//Simultaneous similar edits that caused merge conflict. Taking both for now to unblock.
	//Can likely be unified.
	FWorldDelegates::OnWorldBeginTearDown.Broadcast(this);
}

// Cumulated time doing IncrementalUnregisterComponents in UWorld::RemoveFromWorld since last call to UWorld::UpdateLevelStreaming.
static double GRemoveFromWorldUnregisterComponentTimeCumul = 0.0;

void UWorld::RemoveFromWorld( ULevel* Level, bool bAllowIncrementalRemoval, FNetLevelVisibilityTransactionId TransactionId, ULevelStreaming* InOwningLevelStreaming)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RemoveFromWorld);
	SCOPE_CYCLE_COUNTER(STAT_RemoveFromWorldTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RemoveFromWorld);

	FScopeCycleCounterUObject Context(Level);
	check(IsValid(Level));
	check(!Level->IsUnreachable());

	const bool bIsGameWorld = IsGameWorld();

	Level->bIsDisassociatingLevel = true;

	// If not provided, find the owning streaming level
	ULevelStreaming* OwningLevelStreaming = InOwningLevelStreaming ? InOwningLevelStreaming : FLevelUtils::FindStreamingLevel(Level);

	auto BeginRemoval = [Level, bIsGameWorld, this, OwningLevelStreaming]()
	{
		FWorldDelegates::PreLevelRemovedFromWorld.Broadcast(Level, this);
		Level->bIsBeingRemoved = true;

		if (OwningLevelStreaming)
		{
			FLevelStreamingDelegates::OnLevelBeginMakingInvisible.Broadcast(this, OwningLevelStreaming, Level);
		}

		if (Level->bRequireFullVisibilityToRender && Level->bIsVisible)
		{
			Level->bIsVisible = false;
			ULevelStreaming::BroadcastLevelVisibleStatus(this, Level->GetOutermost()->GetFName(), false);
		}

		// Set server level as not visible right away so that clients trying to make visible
		// will fail for this level as it is currently removed from world on the server
		if (bIsGameWorld && !Level->bClientOnlyVisible && IsValid(ServerStreamingLevelsVisibility))
		{
			// Update server's visible levels
			check(IsNetMode(NM_ListenServer) || IsNetMode(NM_DedicatedServer));
			check(OwningLevelStreaming || !ServerStreamingLevelsVisibility->Contains(Level->GetPackage()->GetFName()));
			ServerStreamingLevelsVisibility->SetIsVisible(OwningLevelStreaming, false);
		}
	};

	// To be removed from the world a world must be visible and not pending being made visible (this may be redundent, but for safety)
	// If the level may be removed incrementally then there must also be no level pending visibility
	if ( ((CurrentLevelPendingVisibility == nullptr) || (!bAllowIncrementalRemoval && (CurrentLevelPendingVisibility != Level))) && (Level->bIsVisible || Level->bIsBeingRemoved) )
	{
#if PERF_TRACK_DETAILED_ASYNC_STATS
		// Keep track of timing.
		double PerfTrackStartTime = FPlatformTime::Seconds();
#endif

		bool bFinishRemovingLevel = true;
		if ( bAllowIncrementalRemoval && GLevelStreamingUnregisterComponentsTimeLimit > 0.0 )
		{
			bFinishRemovingLevel = false;
			if (CurrentLevelPendingInvisibility == nullptr)
			{
				double StartTime = FPlatformTime::Seconds();
				// Mark level as being the one in process of being made invisible. 
				// This will prevent this level from being unloaded or made visible in the meantime
				CurrentLevelPendingInvisibility = Level;

				BeginRemoval();

				// Take into consideration time taken by BeginRemoval
				double DeltaTime = (FPlatformTime::Seconds() - StartTime) * 1000;
				GRemoveFromWorldUnregisterComponentTimeCumul += DeltaTime;
			}

			if (CurrentLevelPendingInvisibility == Level)
			{
				double TimeLimit = FMath::Max<double>(0.0, GLevelStreamingUnregisterComponentsTimeLimit - GRemoveFromWorldUnregisterComponentTimeCumul);
				if (TimeLimit > 0.0)
				{
					double StartTime = FPlatformTime::Seconds();
					// Incrementally unregister actor components. 
					// This avoids spikes on the renderthread and gamethread when we subsequently call ClearLevelComponents() further down
					check(bIsGameWorld);
					int32 NumComponentsToUnregister = GLevelStreamingComponentsUnregistrationGranularity;
					do
					{
						if (Level->IncrementalUnregisterComponents(NumComponentsToUnregister))
						{
							// We're done, so the level can be removed
							CurrentLevelPendingInvisibility = nullptr;
							bFinishRemovingLevel = true;
							break;
						}
					} while (!IsTimeLimitExceeded(TEXT("unregistering components"), StartTime, Level, TimeLimit));

					double DeltaTime = (FPlatformTime::Seconds() - StartTime) * 1000;
					GRemoveFromWorldUnregisterComponentTimeCumul += DeltaTime;
				}
			}
		}
		else if (Level == CurrentLevelPendingInvisibility)
		{
			// Finish current level pending invisibility without time limit
			check(Level->bIsBeingRemoved);
			while (!Level->IncrementalUnregisterComponents(MAX_int32)) {}
			CurrentLevelPendingInvisibility = nullptr;
			bFinishRemovingLevel = true;
		}
		else
		{
			BeginRemoval();
		}

		if ( bFinishRemovingLevel )
		{
			for (int32 ActorIdx = 0; ActorIdx < Level->Actors.Num(); ActorIdx++)
			{
				if (AActor* Actor = Level->Actors[ActorIdx])
				{
					Actor->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
				}
			}

			// Remove any pawns from the pawn list that are about to be streamed out
			for (APawn* Pawn : TActorRange<APawn>(this))
			{
				if (Pawn->IsInLevel(Level))
				{
					AController* Controller = Pawn->GetController();
					// This should have happened as part of the RouteEndPlay above, but ensuring to validate this assumption and maintain behavior
					// with RemovePawn having been deprecated
					if (!ensure(Controller == nullptr || (Controller->GetPawn() == Pawn)))
					{
						Controller->UnPossess();
					}
				}
				else if (UCharacterMovementComponent* CharacterMovement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
				{
					// otherwise force floor check in case the floor was streamed out from under it
					CharacterMovement->bForceNextFloorCheck = true;
				}
			}

			Level->ReleaseRenderingResources();

			// Remove from the world's level array and destroy actor components.
			IStreamingManager::Get().RemoveLevel( Level );
		
			Level->ClearLevelComponents();

			if (bIsGameWorld && !Level->bClientOnlyVisible)
			{
				FUpdateLevelVisibilityLevelInfo LevelVisibility(Level, false);
				const FName UnmappedPackageName = LevelVisibility.PackageName;

				// notify server that the client has removed this level
				for (FLocalPlayerIterator It(GEngine, this); It; ++It)
				{
					if (APlayerController* LocalPlayerController = It->GetPlayerController(this))
					{
						LevelVisibility.PackageName = LocalPlayerController->NetworkRemapPath(UnmappedPackageName, false);
						LevelVisibility.VisibilityRequestId = TransactionId;
						LocalPlayerController->ServerUpdateLevelVisibility(LevelVisibility);
					}
				}
			}

			// We expect level that use the bRequireFullVisibilityToRender flag to already be !visible at this point
			check(!Level->bRequireFullVisibilityToRender || !Level->bIsVisible);
			Level->bIsVisible = false;

			// Notify world composition: will place a level at original position
			if (WorldComposition)
			{
				WorldComposition->OnLevelRemovedFromWorld(Level);
			}

			// Make sure level always has OwningWorld in the editor
			if (bIsGameWorld)
			{
				Levels.Remove(Level);
				Level->OwningWorld = nullptr;
			}
				
			// let the universe know we have removed a level
			FWorldDelegates::LevelRemovedFromWorld.Broadcast(Level, this);
			BroadcastLevelsChanged();

			// If the level requires full visibility to be rendered, we already made it non visible in BeginRemoval()
			if (!Level->bRequireFullVisibilityToRender)
			{
				ULevelStreaming::BroadcastLevelVisibleStatus(this, Level->GetOutermost()->GetFName(), false);
			}

			Level->bIsBeingRemoved = false;
		} // if ( bFinishRemovingLevel )

		Level->bIsDisassociatingLevel = false;

#if PERF_TRACK_DETAILED_ASYNC_STATS
		UE_LOG(LogStreaming, Display, TEXT("UWorld::RemoveFromWorld for %s took %5.2f ms"), *Level->GetOutermost()->GetName(), (FPlatformTime::Seconds() - PerfTrackStartTime) * 1000.0);
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
	}
}


// static
FGameTime FGameTime::GetTimeSinceAppStart()
{
	return FGameTime::CreateUndilated(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime());
}

void UWorld::RenameToPIEWorld(int32 PIEInstanceID)
{
#if WITH_EDITOR
	UPackage* WorldPackage = GetOutermost();

	WorldPackage->SetPIEInstanceID(PIEInstanceID);
	WorldPackage->SetPackageFlags(PKG_PlayInEditor);

	const FString PIEPackageName = *UWorld::ConvertToPIEPackageName(WorldPackage->GetName(), PIEInstanceID);
	WorldPackage->Rename(*PIEPackageName, nullptr, REN_ForceNoResetLoaders);
	FSoftObjectPath::AddPIEPackageName(FName(*PIEPackageName));

	StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(PIEInstanceID);
	
	if (WorldComposition)
	{
		WorldComposition->ReinitializeForPIE();
	}
	
	for (ULevelStreaming* LevelStreaming : StreamingLevels)
	{
		LevelStreaming->RenameForPIE(PIEInstanceID);
	}

	PersistentLevel->FixupForPIE(PIEInstanceID);
#endif
}

bool UWorld::IsInstanced() const
{
	UPackage* Package = GetPackage();
	return !Package->GetLoadedPath().IsEmpty() && Package->GetFName() != Package->GetLoadedPath().GetPackageFName();
}

bool UWorld::GetSoftObjectPathMapping(FString& OutSourceWorldPath, FString& OutRemappedWorldPath) const
{
	if (IsInstanced())
	{
		UPackage* Package = GetPackage();
		const FString SourcePackageName = Package->GetLoadedPath().GetPackageName();
		const FString SourceWorldName = FPaths::GetBaseFilename(SourcePackageName);
		const FString RemmappedPackageName = Package->GetName();
		const FString RemappedWorldName = GetName();

		OutSourceWorldPath = SourcePackageName + TEXT(".") + SourceWorldName;

		OutRemappedWorldPath = RemmappedPackageName + TEXT(".") + RemappedWorldName;

		return true;
	}
	
	OutSourceWorldPath = OutRemappedWorldPath = GetPathName();
		
	return false;
}

FString UWorld::ConvertToPIEPackageName(const FString& PackageName, int32 PIEInstanceID)
{
	const FString PackageAssetName = FPackageName::GetLongPackageAssetName(PackageName);
	
	if (PackageAssetName.StartsWith(PLAYWORLD_PACKAGE_PREFIX))
	{
		return PackageName;
	}
	else
	{
		check(PIEInstanceID != -1);
		const FString PackageAssetPath = FPackageName::GetLongPackagePath(PackageName);
		const FString PackagePIEPrefix = BuildPIEPackagePrefix(PIEInstanceID);
		return FString::Printf(TEXT("%s/%s%s"), *PackageAssetPath, *PackagePIEPrefix, *PackageAssetName );
	}
}

FString UWorld::StripPIEPrefixFromPackageName(const FString& PrefixedName, const FString& Prefix)
{
	FString ResultName;
	FString ShortPrefixedName = FPackageName::GetLongPackageAssetName(PrefixedName);
	if (ShortPrefixedName.StartsWith(Prefix))
	{
		FString NamePath = FPackageName::GetLongPackagePath(PrefixedName);
		ResultName = NamePath + "/" + ShortPrefixedName.RightChop(Prefix.Len());
	}
	else
	{
		ResultName = PrefixedName;
	}

	return ResultName;
}

FString UWorld::BuildPIEPackagePrefix(int PIEInstanceID)
{
	check(PIEInstanceID != -1);
	return FString::Printf(TEXT("%s_%d_"), PLAYWORLD_PACKAGE_PREFIX, PIEInstanceID);
}

bool UWorld::RemapCompiledScriptActor(FString& Str) const 
{
	// We're really only interested in compiled script actors, skip everything else.
	if (bDisableRemapScriptActors != 0 || !Str.Contains(TEXT("_C_")))
	{
		return false;
	}

	// Wrap our search string as an FName. This will allow us to do a quick search to see if the object exists regardless of index.
	const FName ActorName(*Str);
	for (TArray<ULevel*>::TConstIterator it = GetLevels().CreateConstIterator(); it; ++it)
	{
		const ALevelScriptActor* const LSA = GetLevelScriptActor(*it);
		// As there should only be one instance of the persistent level script actor, if the indexes match, then this is the object name we want.
		if(LSA && LSA->GetFName().IsEqual(ActorName, ENameCase::IgnoreCase, false))
		{
			Str = LSA->GetFName().ToString();
			return true;
		}
	}

	return false;
}

UWorld* UWorld::GetDuplicatedWorldForPIE(UWorld* InWorld, UPackage* InPIEackage, int32 PIEInstanceID)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::GetDuplicatedWorldForPIE);

	check(PIEInstanceID != INDEX_NONE);

	UPackage* InPackage = InWorld->GetOutermost();
	
	FObjectDuplicationParameters Parameters(InWorld, InPIEackage);
	Parameters.DestName = InWorld->GetFName();
	Parameters.DestClass = InWorld->GetClass();
	Parameters.DuplicateMode = EDuplicateMode::PIE;
	Parameters.PortFlags = PPF_DuplicateForPIE;

	UWorld* DuplicatedWorld = CastChecked<UWorld>(StaticDuplicateObjectEx(Parameters));

	DuplicatedWorld->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(PIEInstanceID);

	return DuplicatedWorld;
}

UWorld* UWorld::DuplicateWorldForPIE(const FString& PackageName, UWorld* OwningWorld)
{
#if WITH_EDITOR
	QUICK_SCOPE_CYCLE_COUNTER(STAT_World_DuplicateWorldForPIE);
	FScopeCycleCounterUObject Context(OwningWorld);

	FName PackageFName(*PackageName);

	// Find the original (non-PIE) level package
	UPackage* EditorLevelPackage = FindObjectFast<UPackage>(nullptr, PackageFName);
	if (!EditorLevelPackage)
	{
		return nullptr;
	}

	// Find world object and use its PersistentLevel pointer.
	UWorld* EditorLevelWorld = UWorld::FindWorldInPackage(EditorLevelPackage);

	// If the world was not found, try to follow a redirector, if there is one
	if ( !EditorLevelWorld )
	{
		EditorLevelWorld = UWorld::FollowWorldRedirectorInPackage(EditorLevelPackage);
		if ( EditorLevelWorld )
		{
			EditorLevelPackage = EditorLevelWorld->GetOutermost();
		}
	}

	if (!EditorLevelWorld)
	{
		return nullptr;
	}
	
	int32 PIEInstanceID = -1;

	if (FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(OwningWorld))
	{
		PIEInstanceID = WorldContext->PIEInstance;
	}
	else if (OwningWorld)
	{
		PIEInstanceID = OwningWorld->GetOutermost()->GetPIEInstanceID();
	}
	else
	{
		checkf(false, TEXT("Unable to determine PIEInstanceID to duplicate for PIE."));
	}

	FTemporaryPlayInEditorIDOverride IDHelper(PIEInstanceID);

	FString PrefixedLevelName = ConvertToPIEPackageName(PackageName, PIEInstanceID);
	const FName PrefixedLevelFName = FName(*PrefixedLevelName);
	FSoftObjectPath::AddPIEPackageName(PrefixedLevelFName);

	UWorld::WorldTypePreLoadMap.FindOrAdd(PrefixedLevelFName) = EWorldType::PIE;
	UPackage* PIELevelPackage = CreatePackage(*PrefixedLevelName);
	// Add PKG_NewlyCreated flag to this package so we don't try to resolve its linker as it is unsaved duplicated world package
	PIELevelPackage->SetPackageFlags(PKG_PlayInEditor | PKG_NewlyCreated);
	PIELevelPackage->SetPIEInstanceID(PIEInstanceID);
	PIELevelPackage->SetLoadedPath(EditorLevelPackage->GetLoadedPath());
	PIELevelPackage->SetSavedHash( EditorLevelPackage->GetSavedHash() );
	PIELevelPackage->MarkAsFullyLoaded();

	ULevel::StreamedLevelsOwningWorld.Add(PIELevelPackage->GetFName(), OwningWorld);

	UWorld* PIELevelWorld = GetDuplicatedWorldForPIE(EditorLevelWorld, PIELevelPackage, PIEInstanceID);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FixupForPIE);
		// The owning world may contain lazy pointers to actors in the sub-level we just duplicated so make sure they are fixed up with the PIE GUIDs
		FPIEFixupSerializer FixupSerializer(OwningWorld, PIEInstanceID);
		FixupSerializer << OwningWorld;
	}


	// Ensure the feature level matches the editor's, this is required as FeatureLevel is not a UPROPERTY and is not duplicated from EditorLevelWorld.
	PIELevelWorld->SetFeatureLevel(EditorLevelWorld->GetFeatureLevel());

	// Clean up the world type list and owning world list now that PostLoad has occurred
	UWorld::WorldTypePreLoadMap.Remove(PrefixedLevelFName);
	ULevel::StreamedLevelsOwningWorld.Remove(PIELevelPackage->GetFName());
	
	{
		ULevel* EditorLevel = EditorLevelWorld->PersistentLevel;
		ULevel* PIELevel = PIELevelWorld->PersistentLevel;

		// If editor has run construction scripts or applied level offset, we dont do it again
		PIELevel->bAlreadyMovedActors = EditorLevel->bAlreadyMovedActors;
		PIELevel->bHasRerunConstructionScripts = EditorLevel->bHasRerunConstructionScripts;

		// Fixup model components. The index buffers have been created for the components in the EditorWorld and the order
		// in which components were post-loaded matters. So don't try to guarantee a particular order here, just copy the
		// elements over.
		if (PIELevel->Model != NULL
			&& PIELevel->Model == EditorLevel->Model
			&& PIELevel->ModelComponents.Num() == EditorLevel->ModelComponents.Num() )
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_World_DuplicateWorldForPIE_UpdateModelComponents);

			PIELevel->Model->ClearLocalMaterialIndexBuffersData();
			for (int32 ComponentIndex = 0; ComponentIndex < PIELevel->ModelComponents.Num(); ++ComponentIndex)
			{
				UModelComponent* SrcComponent = EditorLevel->ModelComponents[ComponentIndex];
				UModelComponent* DestComponent = PIELevel->ModelComponents[ComponentIndex];
				DestComponent->CopyElementsFrom(SrcComponent);
			}
		}

		// We have to place PIELevel at the local position in case EditorLevel was visible
		// Correct placement will occur during UWorld::AddToWorld
		if (EditorLevel->OwningWorld->WorldComposition && EditorLevel->bIsVisible)
		{
			FIntVector LevelOffset = FIntVector::ZeroValue - EditorLevel->OwningWorld->WorldComposition->GetLevelOffset(EditorLevel);
			PIELevel->ApplyWorldOffset(FVector(LevelOffset), false);
		}
	}

	PIELevelWorld->ClearFlags(RF_Standalone);
	EditorLevelWorld->TransferBlueprintDebugReferences(PIELevelWorld);

	UE_LOG(LogWorld, Verbose, TEXT("PIE: Copying PIE streaming level from %s to %s. OwningWorld: %s"),
		*EditorLevelWorld->GetPathName(),
		*PIELevelWorld->GetPathName(),
		OwningWorld ? *OwningWorld->GetPathName() : TEXT("<null>"));

	return PIELevelWorld;
#else
	return nullptr;
#endif
}

void FStreamingLevelsToConsider::Add_Internal(ULevelStreaming* StreamingLevel, bool bGuaranteedNotInContainer)
{
	if (StreamingLevel)
	{
		SCOPE_CYCLE_COUNTER(STAT_ManageLevelsToConsider);
		if (AreStreamingLevelsBeingConsidered())
		{
			// Add is a more significant reason than reevaluate, so either we are adding it to the map 
			// if not already there, or upgrading the reason if not
			EProcessReason& ProcessReason = LevelsToProcess.FindOrAdd(ObjectPtrWrap(StreamingLevel));
			ProcessReason = EProcessReason::Add;
		}
		else
		{
			if (bGuaranteedNotInContainer || !StreamingLevels.Contains(StreamingLevel))
			{
				auto PrioritySort = [](ULevelStreaming* LambdaStreamingLevel, ULevelStreaming* OtherStreamingLevel)
				{
					if (LambdaStreamingLevel && OtherStreamingLevel)
					{
						const int32 Priority = LambdaStreamingLevel->GetPriority();
						const int32 OtherPriority = OtherStreamingLevel->GetPriority();

						if (Priority == OtherPriority)
						{
							return ((UPTRINT)LambdaStreamingLevel < (UPTRINT)OtherStreamingLevel);
						}

						return (Priority < OtherPriority);
					}

					return (LambdaStreamingLevel != nullptr);
				};

				StreamingLevels.Insert(StreamingLevel, Algo::LowerBound(StreamingLevels, StreamingLevel, PrioritySort));
			}
		}
	}
}

bool FStreamingLevelsToConsider::Remove(ULevelStreaming* StreamingLevel)
{
	bool bRemoved = false;
	if (StreamingLevel)
	{
		SCOPE_CYCLE_COUNTER(STAT_ManageLevelsToConsider);
		if (AreStreamingLevelsBeingConsidered())
		{
			int32 Index;
			if (StreamingLevels.Find(StreamingLevel, Index))
			{
				// While we are considering we must null here because we are iterating the array and changing the size would be undesirable
				StreamingLevels[Index] = nullptr;
				bRemoved = true;
			}
			bRemoved |= (LevelsToProcess.Remove(ObjectPtrWrap(StreamingLevel)) > 0);
		}
		else
		{
			bRemoved = (StreamingLevels.Remove(ObjectPtrWrap(StreamingLevel)) > 0);
		}
	}
	return bRemoved;
}

void FStreamingLevelsToConsider::RemoveAt(const int32 Index)
{
	if (AreStreamingLevelsBeingConsidered())
	{
		if (ULevelStreaming* StreamingLevel = StreamingLevels[Index])
		{
			LevelsToProcess.Remove(ObjectPtrWrap(StreamingLevel));

			// While we are considering we must null here because we are iterating the array and changing the size would be undesirable
			StreamingLevels[Index] = nullptr;
		}
	}
	else
	{
		StreamingLevels.RemoveAt(Index, 1, EAllowShrinking::No);
	}
}

void FStreamingLevelsToConsider::Reevaluate(ULevelStreaming* StreamingLevel)
{
	if (StreamingLevel)
	{
		if (AreStreamingLevelsBeingConsidered())
		{
			// If the streaming level is already in the map then it doesn't need to be updated as it is either
			// already Reevaluate or the more significant Add
			if (!LevelsToProcess.Contains(ObjectPtrWrap(StreamingLevel)))
			{
				LevelsToProcess.Add(ObjectPtrWrap(StreamingLevel), EProcessReason::Reevaluate);
			}
		}
		else
		{
			// Remove and readd the element to have it inserted to the correct priority sorted location
			// If the element wasn't in the container then don't add
			if (Remove(StreamingLevel))
			{
				Add_Internal(StreamingLevel, true);
			}
		}
	}
}

bool FStreamingLevelsToConsider::Contains(ULevelStreaming* StreamingLevel) const
{
	return (StreamingLevel && (StreamingLevels.Contains(ObjectPtrWrap(StreamingLevel)) || LevelsToProcess.Contains(ObjectPtrWrap(StreamingLevel))));
}

void FStreamingLevelsToConsider::Reset()
{
	if (AreStreamingLevelsBeingConsidered())
	{
		// not safe to resize while levels are being considered, just null everything
		FMemory::Memzero(StreamingLevels.GetData(), StreamingLevels.Num() * sizeof(ULevelStreaming*));
	}
	else
	{
		StreamingLevels.Reset();
	}
	LevelsToProcess.Reset();
}

void FStreamingLevelsToConsider::BeginConsideration()
{
	StreamingLevelsBeingConsidered++;
}

void FStreamingLevelsToConsider::EndConsideration()
{
	StreamingLevelsBeingConsidered--;

	if (!AreStreamingLevelsBeingConsidered())
	{
		if (LevelsToProcess.Num() > 0)
		{
			// For any streaming level that was added or had its priority changed while we were considering the
			// streaming levels go through and ensure they are correctly in the map and sorted to the correct location
			auto LevelsToProcessCopy = MoveTemp(LevelsToProcess);
			for (const auto& LevelToProcessPair : LevelsToProcessCopy)
			{
				if (ULevelStreaming* StreamingLevel = LevelToProcessPair.Key)
				{
					// Remove the level if it is already in the list so we can use Add to place in the correct priority location
					const bool bIsBeingConsidered = Remove(StreamingLevel);

					// If the level was in the list or this is an Add, now use Add to insert in priority order
					if (bIsBeingConsidered || LevelToProcessPair.Value == EProcessReason::Add)
					{
						Add_Internal(StreamingLevel, true);
					}
				}
			}
		}

		// Removed null entries that might have been cleared during consideration.
		StreamingLevels.Remove(nullptr);
	}
}

void FStreamingLevelsToConsider::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	for (auto& LevelToProcessPair : LevelsToProcess)
	{
		if (auto& StreamingLevel = LevelToProcessPair.Key)
		{
			Collector.AddReferencedObject(StreamingLevel, InThis);
		}
	}
}

void UWorld::BlockTillLevelStreamingCompleted()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::BlockTillLevelStreamingCompleted);

	const double StartTime = FPlatformTime::Seconds();
	TScopeCounter<uint32> IsInBlockTillLevelStreamingCompletedCounter(IsInBlockTillLevelStreamingCompleted);

	if (IsInBlockTillLevelStreamingCompleted == 1)
	{
		++BlockTillLevelStreamingCompletedEpoch;
	}

	bool bIsStreamingPaused = false;

	// In case a FlushAsyncLoading() happens inside loop we need to tick an extra loop.
	int32 WorkToDo = 2; 
	do
	{
		// Update world's required streaming levels
		InternalUpdateStreamingState();

		// Probe if we have anything to do
		UpdateLevelStreaming();
		
		// Everytime we have work to do, add an extra loop to handle FlushAsyncLoading calls
		if (IsVisibilityRequestPending() || IsAsyncLoading())
		{
			WorkToDo = 2;
		}

		if (!bIsStreamingPaused && GEngine->GameViewport && GEngine->BeginStreamingPauseDelegate && GEngine->BeginStreamingPauseDelegate->IsBound())
		{
			GEngine->BeginStreamingPauseDelegate->Execute(GEngine->GameViewport->Viewport);
			bIsStreamingPaused = true;
		}

		// Flush level streaming requests, blocking till completion.
		FlushLevelStreaming(EFlushLevelStreamingType::Full);
	} while (--WorkToDo > 0);

	if (bIsStreamingPaused && GEngine->EndStreamingPauseDelegate && GEngine->EndStreamingPauseDelegate->IsBound())
	{
		GEngine->EndStreamingPauseDelegate->Execute();
	}

	const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogWorld, Verbose, TEXT("BlockTillLevelStreamingCompleted took %s seconds (MatchStarted %d)"), *FText::AsNumber(ElapsedTime).ToString(), bMatchStarted);
}

void UWorld::InternalUpdateStreamingState()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UpdateStreamingState);
	SCOPE_CYCLE_COUNTER(STAT_UpdateStreamingState);

	// Update streaming levels state using streaming volumes.
	// Issues level streaming load/unload requests based on local players being inside/outside level streaming volumes.
	ProcessLevelStreamingVolumes();

	// Update WorldComposition required streaming levels
	if (WorldComposition)
	{
		WorldComposition->UpdateStreamingState();
	}

	// Update World Subsystems required streaming levels
	const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
	for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
	{
	    WorldSubsystem->UpdateStreamingState();
	}
}

bool UWorld::CanAddLoadedLevelToWorld(ULevel* Level) const
{
	if (CurrentLevelPendingVisibility == nullptr)
	{
		// Allow world partition to decide wether a level should be added or not to the world
		if (const IWorldPartitionCell* Cell = Level->GetWorldPartitionRuntimeCell())
		{
			const UWorld* OuterWorld = Cell->GetOuterWorld();
			if (UWorldPartition* OuterWorldPartition = OuterWorld ? OuterWorld->GetWorldPartition() : nullptr)
			{
				return OuterWorldPartition->CanAddCellToWorld(Cell);
			}
		}
	}
	return true;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UWorld::SetBegunPlay(bool bHasBegunPlay)
{
	if(bBegunPlay == bHasBegunPlay)
	{
		return;
	}
	
	bBegunPlay = bHasBegunPlay;
	if(OnBeginPlay.IsBound())
	{
		OnBeginPlay.Broadcast(bBegunPlay);
	}
}

bool UWorld::GetBegunPlay() const
{
	return bBegunPlay;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS


extern ENGINE_API bool GIsLowMemory;

void UWorld::UpdateLevelStreaming()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::UpdateLevelStreaming);
	SCOPE_CYCLE_COUNTER(STAT_UpdateLevelStreamingTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UpdateLevelStreaming);
	LLM_SCOPE(ELLMTag::LoadMapMisc);

	// Reset counters
	GAddToWorldTimeCumul = 0.0;
	GRemoveFromWorldUnregisterComponentTimeCumul = 0.0;

	// do nothing if level streaming is frozen
	if (bIsLevelStreamingFrozen)
	{
		return;
	}

	GAdaptiveAddToWorld.SetEnabled(GAdaptiveAddToWorldEnabled == 1);

	if ( GAdaptiveAddToWorld.IsEnabled() )
	{
		GAdaptiveAddToWorld.BeginUpdate();
	}

	// Store current number of pending unload levels, it may change in loop bellow
	const int32 NumLevelsPendingPurge = FLevelStreamingGCHelper::GetNumLevelsPendingPurge();

	StreamingLevelsToConsider.BeginConsideration();

	for (int32 Index = StreamingLevelsToConsider.GetStreamingLevels().Num() - 1; Index >= 0; --Index)
	{
		// Call the blocking tick on the movie player periodically.
		if ((Index & 0x7) == 7)
		{
			FMoviePlayerProxy::BlockingTick();
		}

		if (ULevelStreaming* StreamingLevel = StreamingLevelsToConsider.GetStreamingLevels()[Index])
		{
			bool bUpdateAgain = true;
			bool bShouldContinueToConsider = true;
			while (bUpdateAgain && bShouldContinueToConsider)
			{
				bool bRedetermineTarget = false;
				FStreamingLevelPrivateAccessor::UpdateStreamingState(StreamingLevel, bUpdateAgain, bRedetermineTarget);

				if (bRedetermineTarget)
				{
					bShouldContinueToConsider = FStreamingLevelPrivateAccessor::UpdateTargetState(StreamingLevel);
				}
			}

			if (!bShouldContinueToConsider)
			{
				StreamingLevelsToConsider.RemoveAt(Index);
			}
		}
		else
		{
			StreamingLevelsToConsider.RemoveAt(Index);
		}
	}

	AllLevelsChangedEvent.Broadcast();
	StreamingLevelsToConsider.EndConsideration();

	const int32 CurrentNumLevelsPendingPurge = FLevelStreamingGCHelper::GetNumLevelsPendingPurge();
	const int32 LevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurge = GLevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurgeOverride ? 1 : GLevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurge;
	const bool bShouldPurgeLevels = LevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurge && CurrentNumLevelsPendingPurge >= LevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurge;

	CSV_CUSTOM_STAT(LevelStreamingPendingPurge, NumlevelsPendingPurge, CurrentNumLevelsPendingPurge, ECsvCustomStatOp::Set);

	// Are we currently in a low memory situation and number of pending levels to purge meets or exceeds our threshold?
	const bool bShouldDoLowMemoryGC = GIsLowMemory && CurrentNumLevelsPendingPurge >= GLevelStreamingLowMemoryPendingPurgeCount;

	// Low memory GC takes precedence over Continuous GC condition
	if (bShouldDoLowMemoryGC)
	{
		GEngine->ForceGarbageCollection(false);
	}
	else if (bShouldPurgeLevels)
	{
		// Request a 'soft' GC if there are levels pending purge and there are levels to be loaded. In the case of a blocking
		// load this is going to guarantee GC firing first thing afterwards and otherwise it is going to sneak in right before
		// kicking off the async load.
		GEngine->ForceGarbageCollection(false);
	}

	// In case more levels has been requested to unload, force GC on next tick 
	if (GLevelStreamingForceGCAfterLevelStreamedOut != 0)
	{
		if (NumLevelsPendingPurge < FLevelStreamingGCHelper::GetNumLevelsPendingPurge())
		{
			GEngine->ForceGarbageCollection(true); 
		}
	}
	if ( GAdaptiveAddToWorld.IsEnabled() )
	{
		GAdaptiveAddToWorld.EndUpdate();
	}
}

void UWorld::SetShouldForceUnloadStreamingLevels(const bool bInShouldForceUnloadStreamingLevels)
{
	if (bInShouldForceUnloadStreamingLevels != bShouldForceUnloadStreamingLevels)
	{
		bShouldForceUnloadStreamingLevels = bInShouldForceUnloadStreamingLevels;
		if (bShouldForceUnloadStreamingLevels)
		{
			PopulateStreamingLevelsToConsider();
		}
	}
}

void UWorld::SetShouldForceVisibleStreamingLevels(const bool bInShouldForceVisibleStreamingLevels)
{
	if (bInShouldForceVisibleStreamingLevels != bShouldForceVisibleStreamingLevels)
	{
		bShouldForceVisibleStreamingLevels = bInShouldForceVisibleStreamingLevels;
		if (bShouldForceVisibleStreamingLevels)
		{
			PopulateStreamingLevelsToConsider();
		}
	}
}

void UWorld::AddStreamingLevel(ULevelStreaming* StreamingLevelToAdd)
{
	if (StreamingLevelToAdd)
	{
		if (ensure(StreamingLevelToAdd->GetWorld() == this))
		{
			if (ensure(StreamingLevelToAdd->GetLevelStreamingState() == ELevelStreamingState::Removed))
			{
				StreamingLevels.Add(StreamingLevelToAdd);
				FStreamingLevelPrivateAccessor::OnLevelAdded(StreamingLevelToAdd);
				if (FStreamingLevelPrivateAccessor::UpdateTargetState(StreamingLevelToAdd))
				{
					StreamingLevelsToConsider.Add(StreamingLevelToAdd);
				}
			}
		}
	}
}

void UWorld::AddStreamingLevels(TArrayView<ULevelStreaming* const> StreamingLevelsToAdd)
{
	for (ULevelStreaming* StreamingLevelToAdd : StreamingLevelsToAdd)
	{
		AddStreamingLevel(StreamingLevelToAdd);
	}
}

void UWorld::AddUniqueStreamingLevel(ULevelStreaming* StreamingLevelToAdd)
{
	if (!StreamingLevels.Contains(StreamingLevelToAdd))
	{
		AddStreamingLevel(StreamingLevelToAdd);
	}
}

void UWorld::AddUniqueStreamingLevels(TArrayView<ULevelStreaming* const> StreamingLevelsToAdd)
{
	for (ULevelStreaming* StreamingLevelToAdd : StreamingLevelsToAdd)
	{
		AddUniqueStreamingLevel(StreamingLevelToAdd);
	}
}

void UWorld::SetStreamingLevels(TArrayView<ULevelStreaming* const> InStreamingLevels)
{
	StreamingLevels.Reset(InStreamingLevels.Num());
	StreamingLevels.Append(InStreamingLevels.GetData(), InStreamingLevels.Num());

	PopulateStreamingLevelsToConsider();
}

void UWorld::SetStreamingLevels(TArray<ULevelStreaming*>&& InStreamingLevels)
{
	StreamingLevels = MoveTemp(InStreamingLevels);

	PopulateStreamingLevelsToConsider();
}

bool UWorld::RemoveStreamingLevelAt(const int32 IndexToRemove)
{
	if (IndexToRemove >= 0 && IndexToRemove < StreamingLevels.Num())
	{
		ULevelStreaming* StreamingLevel = StreamingLevels[IndexToRemove];
		if (StreamingLevel && StreamingLevel->GetLevelStreamingState() == ELevelStreamingState::Loading)
		{
			--NumStreamingLevelsBeingLoaded;
		}
		StreamingLevels.RemoveAt(IndexToRemove);
		StreamingLevelsToConsider.Remove(StreamingLevel);
		FStreamingLevelPrivateAccessor::OnLevelRemoved(StreamingLevel);
		return true;
	}

	return false;
}

bool UWorld::RemoveStreamingLevel(ULevelStreaming* StreamingLevelToRemove)
{
	const int32 Index = StreamingLevels.Find(StreamingLevelToRemove);
	return RemoveStreamingLevelAt(Index);
}

int32 UWorld::RemoveStreamingLevels(TArrayView<ULevelStreaming* const> StreamingLevelsToRemove)
{
	int32 RemovedLevels = 0;
	for (ULevelStreaming* StreamingLevelToRemove : StreamingLevelsToRemove)
	{
		if (RemoveStreamingLevel(StreamingLevelToRemove))
		{
			++RemovedLevels;
		}
	}

	return RemovedLevels;
}

void UWorld::ClearStreamingLevels()
{
	StreamingLevels.Reset();
	StreamingLevelsToConsider.Reset();
	NumStreamingLevelsBeingLoaded = 0;
}

void UWorld::PopulateStreamingLevelsToConsider()
{
	NumStreamingLevelsBeingLoaded = 0;
	StreamingLevelsToConsider.Reset();
	for (ULevelStreaming* StreamingLevel : StreamingLevels)
	{
		// The streaming level may have just gotten added to the list so have it update its state now
		if (StreamingLevel->GetLevelStreamingState() == ELevelStreamingState::Removed)
		{
			FStreamingLevelPrivateAccessor::OnLevelAdded(StreamingLevel);
		}
		else if (StreamingLevel->GetLevelStreamingState() == ELevelStreamingState::Loading)
		{
			++NumStreamingLevelsBeingLoaded;
		}

		if (FStreamingLevelPrivateAccessor::UpdateTargetState(StreamingLevel))
		{
			StreamingLevelsToConsider.Add(StreamingLevel);
		}
	}
}

void UWorld::UpdateStreamingLevelShouldBeConsidered(ULevelStreaming* StreamingLevelToConsider)
{
	if (StreamingLevelToConsider && ensure(StreamingLevelToConsider->GetWorld() == this) && StreamingLevelToConsider->GetLevelStreamingState() != ELevelStreamingState::Removed)
	{
		if (FStreamingLevelPrivateAccessor::UpdateTargetState(StreamingLevelToConsider))
		{
			StreamingLevelsToConsider.Add(StreamingLevelToConsider);
		}
	}
}

void UWorld::UpdateStreamingLevelPriority(ULevelStreaming* StreamingLevel)
{
	if (StreamingLevel)
	{
		StreamingLevelsToConsider.Reevaluate(StreamingLevel);
	}
}

void UWorld::FlushLevelStreaming(EFlushLevelStreamingType FlushType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorld::FlushLevelStreaming);

	if (FlushType == FlushLevelStreamingType
		|| FlushLevelStreamingType == EFlushLevelStreamingType::Full
		|| FlushType == EFlushLevelStreamingType::None)
	{
		// We're already flushing at the correct level, or none was passed in
		// No need to flush
		return;
	}
	else if (FlushType == EFlushLevelStreamingType::Full && FlushLevelStreamingType == EFlushLevelStreamingType::Visibility)
	{
		// If FlushLevelStreaming is called for a full update while we are already doing a visibility update, 
		// upgrade the stored type and let it go back to the loop that was already running
		FlushLevelStreamingType = EFlushLevelStreamingType::Full;
		return;
	}

	AWorldSettings* WorldSettings = GetWorldSettings();

	TGuardValue<EFlushLevelStreamingType> FlushingLevelStreamingGuard(FlushLevelStreamingType, FlushType);

	if (FlushLevelStreamingType == EFlushLevelStreamingType::Full)
	{
		// Update internals with current loaded/ visibility flags.
		UpdateLevelStreaming();
	}

	auto TickLevelStreaming = [this]()
	{
		// Only flush async loading if we're performing a full flush.
		if (FlushLevelStreamingType == EFlushLevelStreamingType::Full)
		{
			// Make sure all outstanding loads are taken care of, other than ones associated with the excluded type
			FlushAsyncLoading();
		}

		// Kick off making levels visible if loading finished by flushing.
		UpdateLevelStreaming();
	};

	TickLevelStreaming();

	// Making levels visible is spread across several frames so we simply loop till it is done.
	bool bLevelsPendingVisibility = IsVisibilityRequestPending();
	while( bLevelsPendingVisibility )
	{
		const EFlushLevelStreamingType LastStreamingType = FlushLevelStreamingType;

		// Tick level streaming to make levels visible.
		TickLevelStreaming();

		// If FlushLevelStreaming reentered as a result of FlushAsyncLoading or UpdateLevelStreaming and upgraded
		// the flush type, we'll need to do at least one additional loop to be certain all processing is complete
		bLevelsPendingVisibility = ((LastStreamingType != FlushLevelStreamingType) || IsVisibilityRequestPending());
	}
	
	check(!IsVisibilityRequestPending());

	// we need this, or the traces will be abysmally slow
	EnsureCollisionTreeIsBuilt();

	// We already blocked on async loading.
	if (FlushLevelStreamingType == EFlushLevelStreamingType::Full)
	{
		bRequestedBlockOnAsyncLoading = false;
	}
}

/**
 * Forces streaming data to be rebuilt for the current world.
 */
static void ForceBuildStreamingData()
{
	for (TObjectIterator<UWorld> ObjIt;  ObjIt; ++ObjIt)
	{
		UWorld* WorldComp = *ObjIt;
		if (WorldComp && WorldComp->PersistentLevel && WorldComp->PersistentLevel->OwningWorld == WorldComp)
		{
			ULevel::BuildStreamingData(WorldComp);
		}		
	}
}

static FAutoConsoleCommand ForceBuildStreamingDataCmd(
	TEXT("ForceBuildStreamingData"),
	TEXT("Forces streaming data to be rebuilt for the current world."),
	FConsoleCommandDelegate::CreateStatic(ForceBuildStreamingData)
	);


void UWorld::TriggerStreamingDataRebuild()
{
	bStreamingDataDirty = true;
	BuildStreamingDataTimer = FPlatformTime::Seconds() + 5.0;
}


void UWorld::ConditionallyBuildStreamingData()
{
	if ( bStreamingDataDirty && FPlatformTime::Seconds() > BuildStreamingDataTimer )
	{
		bStreamingDataDirty = false;
		ULevel::BuildStreamingData( this );
	}
}

bool UWorld::IsVisibilityRequestPending() const
{
	return (CurrentLevelPendingVisibility != nullptr || CurrentLevelPendingInvisibility != nullptr);
}

bool UWorld::AreAlwaysLoadedLevelsLoaded() const
{
	for (ULevelStreaming* LevelStreaming : StreamingLevels)
	{
		// See whether there's a level with a pending request.
		if (LevelStreaming && LevelStreaming->ShouldBeAlwaysLoaded() && LevelStreaming->GetLevelStreamingState() != ELevelStreamingState::FailedToLoad)
		{	
			const ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();

			if (LevelStreaming->HasLoadRequestPending()
				|| !LoadedLevel
				|| LoadedLevel->bIsVisible != LevelStreaming->ShouldBeVisible())
			{
				return false;
			}
		}
	}

	return true;
}

void UWorld::AsyncLoadAlwaysLoadedLevelsForSeamlessTravel()
{
	// Need to do this now so that data can be set correctly on the loaded world's collections.
	// This normally happens in InitWorld but that happens too late for seamless travel.
	ConditionallyCreateDefaultLevelCollections();

	for (ULevelStreaming* LevelStreaming : StreamingLevels)
	{
		// See whether there's a level with a pending request.
		if (LevelStreaming && LevelStreaming->ShouldBeAlwaysLoaded())
		{	
			const ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();

			if (LevelStreaming->HasLoadRequestPending() || !LoadedLevel)
			{
				FStreamingLevelPrivateAccessor::RequestLevel(LevelStreaming, this, true, ULevelStreaming::NeverBlock);
			}
		}
	}
}

bool UWorld::AllowLevelLoadRequests() const
{
	// Always allow level load request in the editor or when we do full streaming flush
	if (IsGameWorld() && FlushLevelStreamingType != EFlushLevelStreamingType::Full)
	{
		const bool bAreLevelsPendingPurge = 
			GLevelStreamingForceGCAfterLevelStreamedOut != 0 &&
			FLevelStreamingGCHelper::GetNumLevelsPendingPurge() > 0;
		
		// Let code choose. Hold off queueing in case: 
		// We are only flushing levels visibility
		// There pending unload requests
		// There pending load requests and gameplay has already started.
		const bool bWorldIsRendering = GetGameViewport() != nullptr && !GetGameViewport()->bDisableWorldRendering;
		const bool bIsPlaying = bWorldIsRendering && GetTimeSeconds() > 1.f;
		const bool bIsPlayingWhileLoading = IsAsyncLoading() && bIsPlaying;
		if (bAreLevelsPendingPurge || FlushLevelStreamingType == EFlushLevelStreamingType::Visibility || (bIsPlayingWhileLoading && !GLevelStreamingAllowLevelRequestsWhileAsyncLoadingInMatch))
		{
			return false;
		}

		// Don't allow requesting new levels if we're playing in game and already busy loading a maximum number of them.
		if (bIsPlaying && GLevelStreamingMaxLevelRequestsAtOnceWhileInMatch > 0 && NumStreamingLevelsBeingLoaded >= GLevelStreamingMaxLevelRequestsAtOnceWhileInMatch)
		{
			return false;
		}
	}

	return true;
}

void UWorld::HandleTimelineScrubbed()
{
	// Deactivate all FX components that belong to world settings
	// These components are all one shot unmanaged FX that are generally going to destroy on complete or be returned back to the component pool they belong to
	if (AWorldSettings* WorldSettings = GetWorldSettings())
	{
		TArray<UFXSystemComponent*> FXSystemComponents;
		WorldSettings->GetComponents(FXSystemComponents);
		for (UFXSystemComponent* FXSystemComponent : FXSystemComponents)
		{
			FXSystemComponent->DeactivateImmediate();
		}
	}

	// Clearing game state references
	if (GameState && !IsValid(GameState))
	{
		GameState = nullptr;
	}

	// Clear collections separately as the game state could differ (instant replay)
	for (FLevelCollection& Collection : LevelCollections)
	{
		const AGameStateBase* CollectionGameState = Collection.GetGameState();
		if (CollectionGameState && !IsValid(CollectionGameState))
		{
			Collection.SetGameState(nullptr);
		}
	}
}

bool UWorld::HandleDemoScrubCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	FString TimeString;
	if (!FParse::Token(Cmd, TimeString, 0))
	{
		Ar.Log(TEXT("You must specify a time"));
	}
	else if (DemoNetDriver != nullptr && DemoNetDriver->GetReplayStreamer().IsValid() && DemoNetDriver->ServerConnection != nullptr && DemoNetDriver->ServerConnection->OwningActor != nullptr)
	{
		APlayerController* PlayerController = Cast<APlayerController>(DemoNetDriver->ServerConnection->OwningActor);
		if (PlayerController != nullptr)
		{
			GetWorldSettings()->SetPauserPlayerState(PlayerController->PlayerState);
			const float Time = FCString::Atof(*TimeString);
			DemoNetDriver->GotoTimeInSeconds(Time);
		}
	}
	return true;
}

bool UWorld::HandleDemoPauseCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	FString TimeString;

	AWorldSettings* WorldSettings = GetWorldSettings();
	check(WorldSettings != nullptr);

	if (WorldSettings->GetPauserPlayerState() == nullptr)
	{
		if (DemoNetDriver != nullptr && DemoNetDriver->ServerConnection != nullptr && DemoNetDriver->ServerConnection->OwningActor != nullptr)
		{
			APlayerController* PlayerController = Cast<APlayerController>(DemoNetDriver->ServerConnection->OwningActor);
			if (PlayerController != nullptr)
			{
				WorldSettings->SetPauserPlayerState(PlayerController->PlayerState);
			}
		}
	}
	else
	{
		WorldSettings->SetPauserPlayerState(nullptr);
	}
	return true;
}

bool UWorld::HandleDemoSpeedCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	FString TimeString;

	AWorldSettings* WorldSettings = GetWorldSettings();
	check(WorldSettings != nullptr);

	FString SpeedString;
	if (!FParse::Token(Cmd, SpeedString, 0))
	{
		Ar.Log(TEXT("You must specify a speed in the form of a float"));
	}
	else
	{
		const float Speed = FCString::Atof(*SpeedString);
		WorldSettings->DemoPlayTimeDilation = Speed;
	}
	return true;
}

bool UWorld::HandleDemoCheckpointCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	const UGameInstance* GameInst = GetGameInstance();
	UReplaySubsystem* ReplaySubsystem = GameInst ? GameInst->GetSubsystem<UReplaySubsystem>() : nullptr;

	if (ReplaySubsystem)
	{
		ReplaySubsystem->RequestCheckpoint();
	}
	else
	{
		Ar.Log(TEXT("Unable to get the replay subsystem."));
	}

	return true;
}

bool UWorld::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( FParse::Command( &Cmd, TEXT("TRACETAG") ) )
	{
		return HandleTraceTagCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("TRACETAGALL")))
	{
		bDebugDrawAllTraceTags = !bDebugDrawAllTraceTags;
		return true;
	}
	else
#endif
		if( FParse::Command( &Cmd, TEXT("FLUSHPERSISTENTDEBUGLINES") ) )
	{		
		return HandleFlushPersistentDebugLinesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("LOGACTORCOUNTS")))
	{		
		return HandleLogActorCountsCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOREC")))
	{		
		return HandleDemoRecordCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("DEMOPLAY") ) )
	{		
		return HandleDemoPlayCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("DEMOSTOP") ) )
	{		
		return HandleDemoStopCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOSCRUB")))
	{
		return HandleDemoScrubCommand(Cmd, Ar, InWorld);
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOPAUSE")))
	{
		return HandleDemoPauseCommand(Cmd, Ar, InWorld);
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOSPEED")))
	{
		return HandleDemoSpeedCommand(Cmd, Ar, InWorld);
	}
	else if (FParse::Command(&Cmd, TEXT("DEMOCHECKPOINT")))
	{
		return HandleDemoCheckpointCommand(Cmd, Ar, InWorld);
	}
	else if(FPhysicsInterface::ExecPhysCommands( Cmd, &Ar, InWorld ) )
	{
		return HandleLogActorCountsCommand( Cmd, Ar, InWorld );
	}
	else 
	{
		return 0;
	}
}

bool UWorld::HandleTraceTagCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString TagStr;
	FParse::Token(Cmd, TagStr, 0);
	DebugDrawTraceTag = FName(*TagStr);
#endif
	return true;
}

bool UWorld::HandleFlushPersistentDebugLinesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	PersistentLineBatcher->Flush();
	return true;
}

bool UWorld::HandleLogActorCountsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT("Num Actors: %i"), InWorld->GetActorCount());
	return true;
}

bool UWorld::HandleDemoRecordCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if (InWorld != nullptr && InWorld->GetGameInstance() != nullptr)
	{
		FString DemoName;

		FParse::Token( Cmd, DemoName, 0 );

		// Allow additional url arguments after the demo name
		TArray<FString> Options;
		if (DemoName.ParseIntoArray(Options, TEXT("?")) > 1)
		{
			DemoName = Options[0];
			Options.RemoveAtSwap(0);
		}

		// The friendly name will be the map name if no name is supplied
		const FString FriendlyName = DemoName.IsEmpty() ? InWorld->GetMapName() : DemoName;

		InWorld->GetGameInstance()->StartRecordingReplay( DemoName, FriendlyName, Options );
	}

	return true;
}

bool UWorld::HandleDemoPlayCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FString Temp;
	const TCHAR* ErrorString = nullptr;

	if ( !FParse::Token( Cmd, Temp, 0 ) )
	{
		ErrorString = TEXT( "You must specify a filename" );
	}
	else if ( InWorld == nullptr )
	{
		ErrorString = TEXT( "InWorld is null" );
	}
	else if (InWorld->WorldType == EWorldType::Editor)
	{
		ErrorString = TEXT("Cannot play a demo without a PIE instance running");
	}
	else if ( InWorld->GetGameInstance() == nullptr )
	{
		ErrorString = TEXT( "InWorld->GetGameInstance() is null" );
	}
	else if (InWorld->WorldType == EWorldType::PIE)
	{
		// Prevent multiple playback in the editor.
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World()->IsPlayingReplay())
			{
				ErrorString = TEXT("A demo is already in progress, cannot play more than one demo at a time in PIE.");
				break;
			}
		}
	}

	if (ErrorString != nullptr)
	{
		UE_SUPPRESS(LogDemo, Error, Ar.Log(ErrorString));

		if (GetGameInstance() != nullptr)
		{
			GetGameInstance()->HandleDemoPlaybackFailure(EReplayResult::Unknown);
		}
	}
	else
	{
		// defer playback to the next frame
		GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(InWorld, [InWorld, Temp]()
		{
			if (InWorld->GetGameInstance())
			{
				FString ReplayName = Temp;
				// Allow additional url arguments after the demo name
				TArray<FString> Options;
				if (Temp.ParseIntoArray(Options, TEXT("?")) > 1)
				{
					ReplayName = Options[0];
					Options.RemoveAtSwap(0);

					InWorld->GetGameInstance()->PlayReplay(ReplayName, nullptr, Options);
				}
				else
				{
					InWorld->GetGameInstance()->PlayReplay(ReplayName);
				}
			}
		}));
	}

	return true;
}

bool UWorld::HandleDemoStopCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if ( InWorld != nullptr && InWorld->GetGameInstance() != nullptr )
	{
		InWorld->GetGameInstance()->StopRecordingReplay();
	}

	return true;
}

void UWorld::DestroyDemoNetDriver()
{
	if ( DemoNetDriver != nullptr )
	{
		const FName DemoNetDriverName = DemoNetDriver->NetDriverName;

		check( GEngine->FindNamedNetDriver( this, DemoNetDriverName ) == DemoNetDriver );

		DemoNetDriver->StopDemo();
		DemoNetDriver->SetWorld(nullptr);

		GEngine->DestroyNamedNetDriver( this, DemoNetDriverName );

		check( GEngine->FindNamedNetDriver( this, DemoNetDriverName ) == nullptr);

		DemoNetDriver = nullptr;
	}
}

void UWorld::ClearDemoNetDriver()
{
	FLevelCollection* const SourceCollection = FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	if (SourceCollection)
	{
		SourceCollection->SetDemoNetDriver(nullptr);
	}
	FLevelCollection* const StaticCollection = FindCollectionByType(ELevelCollectionType::StaticLevels);
	if (StaticCollection)
	{
		StaticCollection->SetDemoNetDriver(nullptr);
	}

	DemoNetDriver = nullptr;
}

void UWorld::ClearNetDriver(UNetDriver* Driver)
{
	if (GetNetDriver() == Driver)
	{
		SetNetDriver(nullptr);
	}

	if (GetDemoNetDriver() == Driver)
	{
		SetDemoNetDriver(nullptr);
	}

	for (FLevelCollection& Collection : LevelCollections)
	{
		if (Collection.GetNetDriver() == Driver)
		{
			Collection.SetNetDriver(nullptr);
		}

		if (Collection.GetDemoNetDriver() == Driver)
		{
			Collection.SetDemoNetDriver(nullptr);
		}
	}
}

bool UWorld::SetGameMode(const FURL& InURL)
{
	if (!IsNetMode(NM_Client) && !AuthorityGameMode)
	{
		AuthorityGameMode = GetGameInstance()->CreateGameModeForURL(InURL, this);
		if( AuthorityGameMode != NULL )
		{
			return true;
		}
		else
		{
			UE_LOG(LogWorld, Error, TEXT("Failed to spawn GameMode actor."));
			return false;
		}
	}

	return false;
}

void UWorld::InitializeActorsForPlay(const FURL& InURL, bool bResetTime, FRegisterComponentContext* Context)
{
	TRACE_WORLD(this);
	TRACE_OBJECT_EVENT(this, InitializeActorsForPlay);

	check(bIsWorldInitialized);
	SCOPED_BOOT_TIMING("UWorld::InitializeActorsForPlay");
	double StartTime = FPlatformTime::Seconds();

	// Don't reset time for seamless world transitions.
	if (bResetTime)
	{
		TimeSeconds = 0.0;
		UnpausedTimeSeconds = 0.0;
		RealTimeSeconds = 0.0;
		AudioTimeSeconds = 0.0;
	}

	// Get URL Options
	FString Options(TEXT(""));
	FString	Error(TEXT(""));
	for( int32 i=0; i<InURL.Op.Num(); i++ )
	{
		Options += TEXT("?");
		Options += InURL.Op[i];
	}

	// Set level info.
	if( !InURL.GetOption(TEXT("load"),NULL) )
	{
		URL = InURL;
	}

	// Update world and the components of all levels.	
	// We don't need to rerun construction scripts if we have cooked data or we are playing in editor unless the PIE world was loaded
	// from disk rather than duplicated
	const bool bRerunConstructionScript = !(FPlatformProperties::RequiresCookedData() || (IsGameWorld() && (PersistentLevel->bHasRerunConstructionScripts || PersistentLevel->bWasDuplicatedForPIE)));
	UpdateWorldComponents( bRerunConstructionScript, true, Context);

	// Init level gameplay info.
	if( !AreActorsInitialized() )
	{
		// Check that paths are valid
		if ( !IsNavigationRebuilt() )
		{
			UE_LOG(LogWorld, Warning, TEXT("*** WARNING - PATHS MAY NOT BE VALID ***"));
		}

		if (GEngine != NULL)
		{
			// Lock the level.
			if (IsPreviewWorld())
			{
				UE_LOG(LogWorld, Verbose, TEXT("Bringing preview %s up for play (max tick rate %i) at %s"), *GetFullName(), FMath::RoundToInt(GEngine->GetMaxTickRate(0, false)), *FDateTime::Now().ToString());
			}
			else
			{
				UE_LOG(LogWorld, Log, TEXT("Bringing %s up for play (max tick rate %i) at %s"), *GetFullName(), FMath::RoundToInt(GEngine->GetMaxTickRate(0, false)), *FDateTime::Now().ToString());
			}
		}

		// Initialize network actors and start execution.
		for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
		{
			ULevel*	const Level = Levels[LevelIndex];
			Level->InitializeNetworkActors();
		}

		// Enable actor script calls.
		bStartup = true;
		bActorsInitialized = true;

		// Spawn server actors
		ENetMode CurNetMode = GEngine != NULL ? GEngine->GetNetMode(this) : NM_Standalone;

		if (CurNetMode == NM_ListenServer || CurNetMode == NM_DedicatedServer)
		{
			GEngine->SpawnServerActors(this);
		}

		// Init the game mode.
		if (AuthorityGameMode && !AuthorityGameMode->IsActorInitialized())
		{
			AuthorityGameMode->InitGame( FPaths::GetBaseFilename(InURL.Map), Options, Error );
		}

		// Route various initialization functions and set volumes.
		const int32 ProcessAllRouteActorInitializationGranularity = 0;
		for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
		{
			ULevel* const Level = Levels[LevelIndex];
			Level->RouteActorInitialize(ProcessAllRouteActorInitializationGranularity);
		}

		// Let server know client sub-levels visibility state
		{
			for (FLocalPlayerIterator It(GEngine, this); It; ++It)
			{
				if (APlayerController* LocalPlayerController = It->GetPlayerController(this))
				{
					TArray<FUpdateLevelVisibilityLevelInfo> LevelVisibilities;
					for (int32 LevelIndex = 1; LevelIndex < Levels.Num(); LevelIndex++)
					{
						ULevel*	SubLevel = Levels[LevelIndex];

						FUpdateLevelVisibilityLevelInfo& LevelVisibility = *new (LevelVisibilities) FUpdateLevelVisibilityLevelInfo(SubLevel, SubLevel->bIsVisible);
						LevelVisibility.PackageName = LocalPlayerController->NetworkRemapPath(LevelVisibility.PackageName, false);
					}

					if(LevelVisibilities.Num() > 0)
					{
						LocalPlayerController->ServerUpdateMultipleLevelsVisibility(LevelVisibilities);
					}
				}
			}
		}

		bStartup = 0;
	}

	// Rearrange actors: static not net relevant actors first, then static net relevant actors and then others.
	check( Levels.Num() );
	check( PersistentLevel );
	check( Levels[0] == PersistentLevel );
	for( int32 LevelIndex=0; LevelIndex<Levels.Num(); LevelIndex++ )
	{
		ULevel*	Level = Levels[LevelIndex];
		Level->SortActorList();
	}

	// Make sure to reset level flags necessary for a future AddToWorld to work properly.
	// 
	// One use case where this is important is for AlwaysLoaded Streaming Levels : 
	//  - Streaming Level Volumes can unload the level
	//  - If we don't reset these flags, the next time the level gets added, it will skip steps like RouteActorInitialize.
	//
	// Skip PersistentLevel (as it's not required for it)
	for (int32 LevelIndex = 1; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ResetLevelFlagsOnLevelAddedToWorld(Levels[LevelIndex]);
	} 

	// update the auto-complete list for the console
	UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
	if (ViewportConsole != nullptr)
	{
		ViewportConsole->BuildRuntimeAutoCompleteList();
	}

	// Let others know the actors are initialized. There are two versions here for convenience.
	FActorsInitializedParams OnActorInitParams(this, bResetTime);

	OnActorsInitialized.Broadcast(OnActorInitParams);
	FWorldDelegates::OnWorldInitializedActors.Broadcast(OnActorInitParams); // Global notification

	// FIXME: Nav and AI system should now use above delegate

	// let all subsystems/managers know:
	// @note if UWorld starts to host more of these it might a be a good idea to create a generic IManagerInterface of some sort
	if (NavigationSystem != nullptr)
	{
		NavigationSystem->OnInitializeActors();
	}

	if (AISystem != nullptr)
	{
		AISystem->InitializeActorsForPlay(bResetTime);
	}

	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel*	Level = Levels[LevelIndex];
		IStreamingManager::Get().AddLevel(Level);
	}

	CheckTextureStreamingBuildValidity(this);

	if(IsPreviewWorld())
	{
		UE_LOG(LogWorld, Verbose, TEXT("Bringing up preview level for play took: %f"), FPlatformTime::Seconds() - StartTime );
	}
	else
	{
		UE_LOG(LogWorld, Log, TEXT("Bringing up level for play took: %f"), FPlatformTime::Seconds() - StartTime );
	}
}

void UWorld::BeginPlay()
{
	const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());

	if (SupportsMakingVisibleTransactionRequests() && (IsNetMode(NM_DedicatedServer) || IsNetMode(NM_ListenServer)))
	{
		ServerStreamingLevelsVisibility = AServerStreamingLevelsVisibility::SpawnServerActor(this);
	}

#if WITH_EDITOR
	// Gives a chance to any assets being used for PIE/game to complete
	FAssetCompilingManager::Get().ProcessAsyncTasks();
#endif

	for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
	{
		WorldSubsystem->OnWorldBeginPlay(*this);
	}

	AGameModeBase* const GameMode = GetAuthGameMode();
	if (GameMode)
	{
		GameMode->StartPlay();
		if (GetAISystem())
		{
			GetAISystem()->StartPlay();
		}
	}

	OnWorldBeginPlay.Broadcast();

	if(PhysicsScene)
	{
		PhysicsScene->OnWorldBeginPlay();
	}
}

bool UWorld::IsNavigationRebuilt() const
{
	return GetNavigationSystem() == NULL || GetNavigationSystem()->IsNavigationBuilt(GetWorldSettings());
}

void UWorld::CleanupWorld(bool bSessionEnded, bool bCleanupResources, UWorld* NewWorld)
{
    CleanupWorldGlobalTag++;
	if (!bIsWorldInitialized)
	{
		// Only issue the warning for the TopLevelWorld. It is currently valid to call CleanupWorld on the UWorld of
		// streaming sublevels, and they never call InitWorld. (this is done by PrivateDestroyLevel when removing a
		// streaming level from the Level List in the editor.)
		bool bIsStreamingSubWorld = PersistentLevel && PersistentLevel->OwningWorld != this;
		UE_CLOG(!bIsStreamingSubWorld, LogWorld, Warning, TEXT("UWorld::CleanupWorld called twice or called without InitWorld called first (%s)"), *GetName());
	}
	const bool bWorldChanged = NewWorld != this;
	CleanupWorldInternal(bSessionEnded, bCleanupResources, bWorldChanged);
	bIsWorldInitialized = false;
}

void UWorld::CleanupWorldInternal(bool bSessionEnded, bool bCleanupResources, bool bWorldChanged)
{
	TGuardValue<bool> IsBeingCleanedUp(bIsBeingCleanedUp, true);
	
	if(CleanupWorldTag == CleanupWorldGlobalTag)
	{
		return;
	}
	CleanupWorldTag = CleanupWorldGlobalTag;

	// Downgrade verbosity when running commandlet to reduce spam; this message isn't as important in commandlets
#if !NO_LOGGING
	FString Message = FString::Printf(TEXT("UWorld::CleanupWorld for %s, bSessionEnded=%s, bCleanupResources=%s"),
		*GetName(), bSessionEnded ? TEXT("true") : TEXT("false"), bCleanupResources ? TEXT("true") : TEXT("false"));
	if (IsRunningCommandlet())
	{
		UE_LOG(LogWorld, Verbose, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogWorld, Log, TEXT("%s"), *Message);
	}
#endif

	check(IsVisibilityRequestPending() == false);
	
	// Wait on current physics scenes if they are processing
	if(FPhysScene* CurrPhysicsScene = GetPhysicsScene())
	{
		CurrPhysicsScene->WaitPhysScenes();
		CurrPhysicsScene->OnWorldEndPlay();
	}

	FWorldDelegates::OnWorldCleanup.Broadcast(this, bSessionEnded, bCleanupResources);

	GetRendererModule().OnWorldCleanup(this, bSessionEnded, bCleanupResources, bWorldChanged);

	if (AISystem != nullptr)
	{
		AISystem->CleanupWorld(bSessionEnded, bCleanupResources);
	}

	if (bCleanupResources == true)
	{
		// cleanup & remove navigation system
		SetNavigationSystem(nullptr);
	}

	ForEachNetDriver(GEngine, this, [](UNetDriver* const Driver)
	{
		if (Driver != nullptr)
		{
			Driver->GetNetworkObjectList().Reset();
		}
	});

#if WITH_EDITOR
	// If we're server traveling, we need to break the reference dependency here (caused by levelscript)
	// to avoid a GC crash for not cleaning up the gameinfo referenced by levelscript
	if (IsGameWorld() && !GIsEditor && !IsRunningCommandlet() && bSessionEnded && bCleanupResources && PersistentLevel)
	{
		PersistentLevel->CleanupLevelScriptBlueprint();
	}
#endif //WITH_EDITOR

#if ENABLE_VISUAL_LOG
	FVisualLogger::Get().Cleanup(this);
#endif // ENABLE_VISUAL_LOG	

	// Tell actors to remove their components from the scene.
	ClearWorldComponents();

	if (bCleanupResources && PersistentLevel)
	{
		PersistentLevel->ReleaseRenderingResources();

		// Flush any render commands and released accessed UTextures and materials to give them a chance to be collected.
		if ( FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().FlushRenderState();
		}
	}

#if WITH_EDITOR
	const bool bUnloadFromEditor = GIsEditor && !IsTemplate() && bWorldChanged && bCleanupResources;

	// Clear standalone flag when switching maps in the Editor. This causes resources placed in the map
	// package to be garbage collected together with the world.
	if (bUnloadFromEditor)
	{
		// Iterate over all objects to find ones that reside in the same package as the world.
		ForEachObjectWithPackage(GetOutermost(), [this](UObject* CurrentObject)
		{
			if (CurrentObject != this)
			{
				CurrentObject->ClearFlags(RF_Standalone);
			}
			return true;
		});

		if (WorldType != EWorldType::PIE)
		{
			if (PersistentLevel && PersistentLevel->MapBuildData)
			{
				PersistentLevel->MapBuildData->ClearFlags(RF_Standalone);

				// Iterate over all objects to find ones that reside in the same package as the MapBuildData.
				// Specifically the PackageMetaData
				ForEachObjectWithPackage(PersistentLevel->MapBuildData->GetOutermost(), [this](UObject* CurrentObject)
				{
					if (CurrentObject != this)
					{
						CurrentObject->ClearFlags(RF_Standalone);
					}
					return true;
				});
			}
		}
	}

	// Cleanup Persistent level outside of following loop because uninitialized worlds don't have a valid Levels array
	// StreamingLevels are not initialized.
	if (PersistentLevel)
	{
		PersistentLevel->CleanupLevel(bCleanupResources, bUnloadFromEditor);
		PersistentLevel->CleanupReferences();
	}

	if (GetNumLevels() > 1)
	{
		check(GetLevel(0) == PersistentLevel);
		for (int32 LevelIndex = 1; LevelIndex < GetNumLevels(); ++LevelIndex)
		{
			ULevel* Level = GetLevel(LevelIndex);
			Level->CleanupLevel(bCleanupResources, bUnloadFromEditor);
			Level->CleanupReferences();
		}
	}

#else
	if (PersistentLevel)
	{
		check(bCleanupResources);
		PersistentLevel->CleanupLevel();
	}
#endif //WITH_EDITOR

	for (int32 LevelIndex = 0; LevelIndex < GetNumLevels(); ++LevelIndex)
	{
		UWorld* World = CastChecked<UWorld>(GetLevel(LevelIndex)->GetOuter());
		World->CleanupWorldInternal(bSessionEnded, bCleanupResources, bWorldChanged);
	}

	for (ULevelStreaming* StreamingLevel : GetStreamingLevels())
	{
		if (ULevel* Level = StreamingLevel->GetLoadedLevel())
		{
			UWorld* World = CastChecked<UWorld>(Level->GetOuter());
			World->CleanupWorldInternal(bSessionEnded, bCleanupResources, bWorldChanged);
		}
	}

	// Clean up any duplicated levels.
	const FLevelCollection* const DuplicateCollection = FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	if (DuplicateCollection)
	{
		for (const ULevel* Level : DuplicateCollection->GetLevels())
		{
			if (Level)
			{
				UWorld* const LevelWorld = CastChecked<UWorld>(Level->GetOuter());
				LevelWorld->CleanupWorldInternal(bSessionEnded, bCleanupResources, bWorldChanged);
			}
		}
	}

	PSCPool.Cleanup(this);

	FWorldDelegates::OnPostWorldCleanup.Broadcast(this, bSessionEnded, bCleanupResources);

	if (bCleanupResources)
	{
		SubsystemCollection.Deinitialize();
	}

	if(FXSystem && bWorldChanged)
	{
		FFXSystemInterface::Destroy( FXSystem );
		Scene->SetFXSystem(NULL);
		FXSystem = NULL;
	}
}

UGameViewportClient* UWorld::GetGameViewport() const
{
	FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(this);
	return (WorldContext ? WorldContext->GameViewport : NULL);
}

FConstControllerIterator UWorld::GetControllerIterator() const
{
	return ControllerList.CreateConstIterator();
}

int32 UWorld::GetNumControllers() const
{
	return ControllerList.Num();
}

FConstPlayerControllerIterator UWorld::GetPlayerControllerIterator() const
{
	return PlayerControllerList.CreateConstIterator();
}

int32 UWorld::GetNumPlayerControllers() const
{
	return PlayerControllerList.Num();
}

APlayerController* UWorld::GetFirstPlayerController() const
{
	APlayerController* PlayerController = NULL;
	if( GetPlayerControllerIterator() )
	{
		PlayerController = GetPlayerControllerIterator()->Get();
	}
	return PlayerController;
}

ULocalPlayer* UWorld::GetFirstLocalPlayerFromController() const
{
	for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if( PlayerController )
		{
			ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
			if( LocalPlayer )
			{
				return LocalPlayer;
			}
		}
	}
	return NULL;
}

void UWorld::AddController( AController* Controller )
{	
	check( Controller );
	ControllerList.AddUnique( Controller );	
	if( APlayerController* PlayerController = Cast<APlayerController>(Controller) )
	{
		if(!PlayerControllerList.Contains( PlayerController ))
		{
			PlayerControllerList.Add(PlayerController);
			PlayerController->OnAddedToPlayerControllerList();
		}
	}
}


void UWorld::RemoveController( AController* Controller )
{
	check( Controller );
	if( ControllerList.Remove( Controller ) > 0 )
	{
		if( APlayerController* PlayerController = Cast<APlayerController>(Controller) )
		{
			if (PlayerControllerList.Remove(PlayerController) > 0)
			{
				PlayerController->OnRemovedFromPlayerControllerList();
			}
		}
	}
}

FConstPawnIterator::FConstPawnIterator(UWorld* World)
	: Iterator(new TActorIterator<APawn>(World))
{
}

// Operators defined in the cpp to ensure the definition of TActorIterator is known
FConstPawnIterator::FConstPawnIterator(FConstPawnIterator&&) = default;
FConstPawnIterator& FConstPawnIterator::operator=(FConstPawnIterator&&) = default;
FConstPawnIterator::~FConstPawnIterator() = default;

 FConstPawnIterator::operator bool() const
{
	return Iterator.IsValid() && (bool)*Iterator;
}

FConstPawnIterator& FConstPawnIterator::operator++()
{
	checkf(Iterator.IsValid(), TEXT("FConstPawnIterator::operator++() - this iterator has been moved from and is now invalid."));

	++(*Iterator);
	return *this;
}

FConstPawnIterator& FConstPawnIterator::operator++(int)
{
	checkf(Iterator.IsValid(), TEXT("FConstPawnIterator::operator++(int) - this iterator has been moved from and is now invalid."));

	++(*Iterator);
	return *this;
}

FPawnIteratorObject FConstPawnIterator::operator*() const
{
	checkf(Iterator.IsValid(), TEXT("FConstPawnIterator::operator*() - this iterator has been moved from and is now invalid."));

	return FPawnIteratorObject(**Iterator);
}

TUniquePtr<FPawnIteratorObject> FConstPawnIterator::operator->() const
{
	checkf(Iterator.IsValid(), TEXT("FConstPawnIterator::operator->() - this iterator has been moved from and is now invalid."));

	return TUniquePtr<FPawnIteratorObject>(new FPawnIteratorObject(**Iterator));
}

void UWorld::RegisterAutoActivateCamera(ACameraActor* CameraActor, int32 PlayerIndex)
{
	check(CameraActor);
	check(PlayerIndex >= 0);
	AutoCameraActorList.AddUnique(CameraActor);
}

FConstCameraActorIterator UWorld::GetAutoActivateCameraIterator() const
{
	auto Result = AutoCameraActorList.CreateConstIterator();
	return (const FConstCameraActorIterator&)Result;
}


void UWorld::AddNetworkActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	if (Actor->IsPendingKillPending())
	{
		return;
	}

	if (!ContainsLevel(Actor->GetLevel()))
	{
		return;
	}

	ForEachNetDriver(GEngine, this, [Actor](UNetDriver* const Driver)
	{
		if (Driver != nullptr)
		{
			Driver->AddNetworkActor(Actor);
		}
	});
}

void UWorld::RemoveNetworkActor( AActor* Actor ) const
{
	if (Actor)
	{
		ForEachNetDriver(GEngine, this, [Actor](UNetDriver* const Driver)
		{
			if (Driver != nullptr)
			{
				Driver->RemoveNetworkActor(Actor);
			}
		});
	}
}

FDelegateHandle UWorld::AddOnActorSpawnedHandler(const FOnActorSpawned::FDelegate& InHandler) const
{
	return OnActorSpawned.Add(InHandler);
}

void UWorld::RemoveOnActorSpawnedHandler(FDelegateHandle InHandle) const
{
	OnActorSpawned.Remove(InHandle);
}

FDelegateHandle UWorld::AddOnActorPreSpawnInitialization(const FOnActorSpawned::FDelegate& InHandler) const
{
	return OnActorPreSpawnInitialization.Add(InHandler);
}

void UWorld::RemoveOnActorPreSpawnInitialization(FDelegateHandle InHandle) const
{
	OnActorPreSpawnInitialization.Remove(InHandle);
}

FDelegateHandle UWorld::AddOnActorDestroyedHandler(const FOnActorDestroyed::FDelegate& InHandler) const
{
	return OnActorDestroyed.Add(InHandler);
}

void UWorld::RemoveOnActorDestroyededHandler(FDelegateHandle InHandle) const
{
	OnActorDestroyed.Remove(InHandle);
}

FDelegateHandle UWorld::AddOnPostRegisterAllActorComponentsHandler(const FOnPostRegisterAllActorComponents::FDelegate& InHandler) const
{
	return OnPostRegisterAllActorComponents.Add(InHandler);
}

void UWorld::RemoveOnPostRegisterAllActorComponentsHandler(FDelegateHandle InHandle) const
{
	OnPostRegisterAllActorComponents.Remove(InHandle);
}

void UWorld::NotifyPostRegisterAllActorComponents(AActor* Actor)
{
	// This may be called on inactive worlds (for example, while cooking), if so ignore it.
	if (WorldType != EWorldType::Inactive)
	{
		OnPostRegisterAllActorComponents.Broadcast(Actor);
	}
}

FDelegateHandle UWorld::AddOnPreUnregisterAllActorComponentsHandler(const FOnPreUnregisterAllActorComponents::FDelegate& InHandler) const
{
	return OnPreUnregisterAllActorComponents.Add(InHandler);
}

void UWorld::RemoveOnPreUnregisterAllActorComponentsHandler(FDelegateHandle InHandle) const
{
	OnPreUnregisterAllActorComponents.Remove(InHandle);
}

void UWorld::NotifyPreUnregisterAllActorComponents(AActor* Actor)
{
	// This may be called on inactive/GCing worlds, if so ignore it.
	if (WorldType != EWorldType::Inactive && IsValid(this))
	{
		OnPreUnregisterAllActorComponents.Broadcast(Actor);
	}
}

FDelegateHandle UWorld::AddOnActorRemovedFromWorldHandler(const FOnActorRemovedFromWorld::FDelegate& InHandler) const
{
	return OnActorRemovedFromWorld.Add(InHandler);
}

void UWorld::RemoveOnActorRemovedFromWorldHandler(FDelegateHandle InHandle) const
{
	OnActorRemovedFromWorld.Remove(InHandle);
}

FDelegateHandle UWorld::AddMovieSceneSequenceTickHandler(const FOnMovieSceneSequenceTick::FDelegate& InHandler)
{
	return MovieSceneSequenceTick.Add(InHandler);
}

void UWorld::RemoveMovieSceneSequenceTickHandler(FDelegateHandle InHandle)
{
	MovieSceneSequenceTick.Remove(InHandle);
}

bool UWorld::IsMovieSceneSequenceTickHandlerBound() const
{
	return MovieSceneSequenceTick.IsBound();
}

ABrush* UWorld::GetDefaultBrush() const
{
	check(PersistentLevel);
	return PersistentLevel->GetDefaultBrush();
}

bool UWorld::HasBegunPlay() const
{
	return GetBegunPlay() && PersistentLevel && PersistentLevel->Actors.Num();
}

bool UWorld::AreActorsInitialized() const
{
	return bActorsInitialized && PersistentLevel && PersistentLevel->Actors.Num();
}

void UWorld::CreatePhysicsScene(const AWorldSettings* Settings)
{
#if CHAOS_DEBUG_NAME
	const FName PhysicsName = IsNetMode(NM_DedicatedServer) ? TEXT("ServerPhysics") : TEXT("ClientPhysics");
	FPhysScene* NewScene = new FPhysScene(nullptr, PhysicsName);
#else
	FPhysScene* NewScene = new FPhysScene(nullptr);
#endif

	SetPhysicsScene(NewScene);
}

void UWorld::SetPhysicsScene(FPhysScene* InScene)
{ 
	// Clear world pointer in old FPhysScene (if there is one)
	if(PhysicsScene != NULL)
	{
		PhysicsScene->SetOwningWorld(nullptr);
		delete PhysicsScene;
	}

	// Assign pointer
	PhysicsScene = InScene;

	if(PhysicsScene != NULL)
	{
		// Set pointer in scene to know which world its coming from
		PhysicsScene->SetOwningWorld(this);
	}
}

APhysicsVolume* UWorld::InternalGetDefaultPhysicsVolume() const
{
	// Create on demand.
	if (DefaultPhysicsVolume == nullptr)
	{
		// Try WorldSettings first
		AWorldSettings* WorldSettings = GetWorldSettings(/*bCheckStreamingPesistent=*/ false, /*bChecked=*/ false);
		UClass* DefaultPhysicsVolumeClass = (WorldSettings ? WorldSettings->DefaultPhysicsVolumeClass : nullptr);

		// Fallback on DefaultPhysicsVolume static
		if (DefaultPhysicsVolumeClass == nullptr)
		{
			DefaultPhysicsVolumeClass = ADefaultPhysicsVolume::StaticClass();
		}

		// Spawn volume
		FActorSpawnParameters SpawnParams;
		SpawnParams.bAllowDuringConstructionScript = true;

#if WITH_EDITOR
		SpawnParams.bCreateActorPackage = false;
#endif

		UWorld* MutableThis = const_cast<UWorld*>(this);
		MutableThis->DefaultPhysicsVolume = MutableThis->SpawnActor<APhysicsVolume>(DefaultPhysicsVolumeClass, SpawnParams);
		MutableThis->DefaultPhysicsVolume->Priority = -1000000;
	}
	return DefaultPhysicsVolume;
}

void UWorld::AddPhysicsVolume(APhysicsVolume* Volume)
{
	if (!Cast<ADefaultPhysicsVolume>(Volume))
	{
		NonDefaultPhysicsVolumeList.Add(Volume);
	}
}

void UWorld::RemovePhysicsVolume(APhysicsVolume* Volume)
{
	NonDefaultPhysicsVolumeList.RemoveSwap(Volume);
	// Also remove null entries that may accumulate as items become invalidated
	NonDefaultPhysicsVolumeList.RemoveSwap(nullptr);
}

void UWorld::SetAllowDeferredPhysicsStateCreation(bool bAllow)
{
	bAllowDeferredPhysicsStateCreation = bAllow;
}

bool UWorld::GetAllowDeferredPhysicsStateCreation() const
{
	return bAllowDeferredPhysicsStateCreation;
}

ALevelScriptActor* UWorld::GetLevelScriptActor( ULevel* OwnerLevel ) const
{
	if( OwnerLevel == NULL )
	{
#if WITH_EDITORONLY_DATA
		OwnerLevel = CurrentLevel;
#else
		OwnerLevel = PersistentLevel;
#endif
	}
	check(OwnerLevel);
	return OwnerLevel->GetLevelScriptActor();
}


AWorldSettings* UWorld::K2_GetWorldSettings()
{
	return GetWorldSettings(/*bCheckStreamingPersistent*/false, /*bChecked*/false);
}


AWorldSettings* UWorld::GetWorldSettings( const bool bCheckStreamingPersistent, const bool bChecked ) const
{
	checkSlow(!IsInActualRenderingThread());
	AWorldSettings* WorldSettings = nullptr;
	if (PersistentLevel)
	{
		WorldSettings = PersistentLevel->GetWorldSettings(bChecked);

		if( bCheckStreamingPersistent )
		{
			if( StreamingLevels.Num() > 0 &&
				StreamingLevels[0] &&
				StreamingLevels[0]->IsA<ULevelStreamingPersistent>()) 
			{
				ULevel* Level = StreamingLevels[0]->GetLoadedLevel();
				if (Level != nullptr)
				{
					WorldSettings = Level->GetWorldSettings(bChecked);
				}
			}
		}
	}
	return WorldSettings;
}

AWorldDataLayers* UWorld::GetWorldDataLayers() const
{
	AWorldDataLayers* WorldDataLayers = nullptr;
	if (PersistentLevel)
	{
		WorldDataLayers = PersistentLevel->GetWorldDataLayers();
	}
	return WorldDataLayers;
}

void UWorld::SetWorldDataLayers(AWorldDataLayers* NewWorldDataLayers)
{
	if (PersistentLevel)
	{
		PersistentLevel->SetWorldDataLayers(NewWorldDataLayers);
	}
}

FString UWorld::GetDebugDisplayName() const
{
	return FString::Printf(TEXT("%s (%s)"), *GetDebugStringForWorld(this), *GetPathNameSafe(this));
}

UWorldPartition* UWorld::GetWorldPartition() const
{
	AWorldSettings* WorldSettings = GetWorldSettings(/*bCheckStreamingPersistent*/false, /*bChecked*/false);
	return WorldSettings ? WorldSettings->GetWorldPartition() : nullptr;
}

UDataLayerManager* UWorld::GetDataLayerManager() const
{
	UWorldPartition* WorldPartition = GetWorldPartition();
	return WorldPartition ? WorldPartition->GetDataLayerManager() : nullptr;
}

UModel* UWorld::GetModel() const
{
#if !WITH_EDITORONLY_DATA
	ULevel* CurrentLevel = PersistentLevel;
#endif
	check(CurrentLevel);
	return CurrentLevel->Model;
}


float UWorld::GetGravityZ() const
{
	AWorldSettings* WorldSettings = GetWorldSettings();
	return (WorldSettings != NULL) ? WorldSettings->GetGravityZ() : 0.f;
}


float UWorld::GetDefaultGravityZ() const
{
	UPhysicsSettings * PhysSetting = UPhysicsSettings::Get();
	return (PhysSetting != NULL) ? PhysSetting->DefaultGravityZ : 0.f;
}

/** This is our global function for retrieving the current MapName **/
ENGINE_API const FString GetMapNameStatic()
{
	FString Retval;

	FWorldContext const* ContextToUse = NULL;
	if ( GEngine )
	{
		// We're going to look through the WorldContexts and pull any Game context we find
		// If there isn't a Game context, we'll take the first PIE we find
		// and if none of those we'll use an Editor
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::Game)
			{
				ContextToUse = &WorldContext;
				break;
			}
			else if (WorldContext.WorldType == EWorldType::PIE && (ContextToUse == NULL || ContextToUse->WorldType != EWorldType::PIE))
			{
				ContextToUse = &WorldContext;
			}
			else if (WorldContext.WorldType == EWorldType::Editor && ContextToUse == NULL)
			{	
				ContextToUse = &WorldContext;
			}
		}
	}

	if( ContextToUse != NULL && ContextToUse->World() != NULL )
	{
		Retval = ContextToUse->World()->GetMapName();
	}
	else if( UObjectInitialized() )
	{
		Retval = appGetStartupMap( FCommandLine::Get() );
	}

	return Retval;
}

const FString UWorld::GetMapName() const
{
	// Default to the world's package as the map name.
	FString MapName;
	
	// In the case of a seamless world check to see whether there are any persistent levels in the levels
	// array and use its name if there is one.
	for (ULevelStreaming* StreamingLevel : StreamingLevels)
	{
		// Use the name of the first found persistent level.
		if (ULevelStreamingPersistent* PersistentStreamingLevel = Cast<ULevelStreamingPersistent>(StreamingLevel))
		{
			MapName = PersistentStreamingLevel->GetWorldAssetPackageName();
			break;
		}
	}

	if (MapName.IsEmpty())
	{
		MapName = GetOutermost()->GetName();
	}

	// Just return the name of the map, not the rest of the path
	MapName = FPackageName::GetLongPackageAssetName(MapName);

	return MapName;
}

EAcceptConnection::Type UWorld::NotifyAcceptingConnection()
{
	check(NetDriver);
	if( NetDriver->ServerConnection )
	{
		// We are a client and we don't welcome incoming connections.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Client refused") );
		return EAcceptConnection::Reject;
	}
	else if( NextURL!=TEXT("") )
	{
		// Server is switching levels.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s refused"), *GetName() );
		return EAcceptConnection::Ignore;
	}
	else
	{
		// Server is up and running.
		UE_CLOG(!NetDriver->DDoS.CheckLogRestrictions(), LogNet, Verbose, TEXT("NotifyAcceptingConnection: Server %s accept"), *GetName());
		return EAcceptConnection::Accept;
	}
}

void UWorld::NotifyAcceptedConnection( UNetConnection* Connection )
{
	check(NetDriver!=NULL);
	check(NetDriver->ServerConnection==NULL);
	UE_LOG(LogNet, Log, TEXT("NotifyAcceptedConnection: Name: %s, TimeStamp: %s, %s"), *GetName(), FPlatformTime::StrTimestamp(), *Connection->Describe() );
	NETWORK_PROFILER( GNetworkProfiler.TrackEvent( TEXT( "OPEN" ), *( GetName() + TEXT( " " ) + Connection->LowLevelGetRemoteAddress() ), Connection ) );
}

bool UWorld::NotifyAcceptingChannel( UChannel* Channel )
{
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;

	if( Driver->ServerConnection )
	{
		// We are a client and the server has just opened up a new channel.
		if (Driver->ChannelDefinitionMap[Channel->ChName].bServerOpen)
		{
			//UE_LOG(LogWorld, Log, TEXT("NotifyAcceptingChannel %i/%s client %s"), Channel->ChIndex, *Channel->ChType.ToString(), *GetFullName() );
			return true;
		}
		else
		{
			// Unwanted channel type.
			UE_LOG(LogNet, Log, TEXT("Client refusing unwanted channel of type %s"), *Channel->ChName.ToString() );
			return false;
		}
	}
	else
	{
		// We are the server.
		if (Driver->ChannelDefinitionMap[Channel->ChName].bClientOpen)
		{
			// The client has opened initial channel.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %s %i server %s: Accepted"), *Channel->ChName.ToString(), Channel->ChIndex, *GetFullName() );
			return true;
		}
		else
		{
			// Client can't open any other kinds of channels.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %s %i server %s: Refused"), *Channel->ChName.ToString(), Channel->ChIndex, *GetFullName() );
			return false;
		}
	}
}

void UWorld::WelcomePlayer(UNetConnection* Connection)
{
#if !WITH_EDITORONLY_DATA
	ULevel* CurrentLevel = PersistentLevel;
#endif

	check(CurrentLevel);

	FString LevelName;

	const FSeamlessTravelHandler& SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld(this);
	if (SeamlessTravelHandler.IsInTransition())
	{
		// Tell the client to go to the destination map
		LevelName = SeamlessTravelHandler.GetDestinationMapName();
		Connection->SetClientWorldPackageName(NAME_None);
	}
	else
	{
		LevelName = CurrentLevel->GetOutermost()->GetName();
		Connection->SetClientWorldPackageName(CurrentLevel->GetOutermost()->GetFName());
	}
	if (UGameInstance* GameInst = GetGameInstance())
	{
		GameInst->ModifyClientTravelLevelURL(LevelName);
	}

	FString GameName;
	FString RedirectURL;
	if (AuthorityGameMode != NULL)
	{
		GameName = AuthorityGameMode->GetClass()->GetPathName();
		AuthorityGameMode->GameWelcomePlayer(Connection, RedirectURL);
	}

	FNetControlMessage<NMT_Welcome>::Send(Connection, LevelName, GameName, RedirectURL);

	FString ClientNetPingICMPAddress;
	FString ClientNetPingUDPAddress;

	if (FParse::Value(FCommandLine::Get(), TEXT("ClientNetPingICMPAddress="), ClientNetPingICMPAddress) && ClientNetPingICMPAddress.Len() > 0)
	{
		const uint32 PingType = static_cast<uint32>(EPingType::ICMP);
		const ENetPingControlMessage MessageType = ENetPingControlMessage::SetPingAddress;
		FString MessageStr = FString::Printf(TEXT("%i=%s"), PingType, ToCStr(ClientNetPingICMPAddress));

		FNetControlMessage<NMT_NetPing>::Send(Connection, MessageType, MessageStr);
	}

	if (FParse::Value(FCommandLine::Get(), TEXT("ClientNetPingUDPAddress="), ClientNetPingUDPAddress) && ClientNetPingUDPAddress.Len() > 0)
	{
		const uint32 PingType = static_cast<uint32>(EPingType::UDPQoS);
		const ENetPingControlMessage MessageType = ENetPingControlMessage::SetPingAddress;
		FString MessageStr = FString::Printf(TEXT("%i=%s"), PingType, ToCStr(ClientNetPingUDPAddress));

		FNetControlMessage<NMT_NetPing>::Send(Connection, MessageType, MessageStr);
	}


	Connection->FlushNet();
	// don't count initial join data for netspeed throttling
	// as it's unnecessary, since connection won't be fully open until it all gets received, and this prevents later gameplay data from being delayed to "catch up"
	Connection->QueuedBits = 0;
	Connection->SetClientLoginState( EClientLoginState::Welcomed );		// Client has been told to load the map, will respond via SendJoin
}

bool UWorld::DestroySwappedPC(UNetConnection* Connection)
{
	for( FConstPlayerControllerIterator Iterator = GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController && PlayerController->Player == NULL && PlayerController->PendingSwapConnection == Connection)
		{
			DestroyActor(PlayerController);
			return true;
		}
	}

	return false;
}

void UWorld::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if( NetDriver->ServerConnection )
	{
		check(Connection == NetDriver->ServerConnection);

		// We are the client, traveling to a new map with the same server
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogNet, Verbose, TEXT("Level client received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
			case NMT_Failure:
			{
				// our connection attempt failed for some reason, for example a synchronization mismatch (bad GUID, etc) or because the server rejected our join attempt (too many players, etc)
				// here we can further parse the string to determine the reason that the server closed our connection and present it to the user
				FString EntryURL = TEXT("?failed");
				FString ErrorMsg;

				if (FNetControlMessage<NMT_Failure>::Receive(Bunch, ErrorMsg))
				{
					if (ErrorMsg.IsEmpty())
					{
						ErrorMsg = NSLOCTEXT("NetworkErrors", "GenericConnectionFailed", "Connection Failed.").ToString();
					}

					if (Connection != nullptr)
					{
						Connection->SendCloseReason(ENetCloseResult::FailureReceived);
					}

					GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::FailureReceived, ErrorMsg);

					if (Connection != nullptr)
					{
						Connection->Close(ENetCloseResult::FailureReceived);
					}
				}

				break;
			}
			case NMT_DebugText:
			{
				// debug text message
				FString Text;

				if (FNetControlMessage<NMT_DebugText>::Receive(Bunch, Text))
				{
					UE_LOG(LogNet, Log, TEXT("%s received NMT_DebugText Text=[%s] Desc=%s DescRemote=%s"),
							*Connection->Driver->GetDescription(), *Text, *Connection->LowLevelDescribe(),
							ToCStr(Connection->LowLevelGetRemoteAddress(true)));
				}

				break;
			}
			case NMT_NetGUIDAssign:
			{
				FNetworkGUID NetGUID;
				FString Path;

				if (FNetControlMessage<NMT_NetGUIDAssign>::Receive(Bunch, NetGUID, Path))
				{
					UE_LOG(LogNet, Verbose, TEXT("NMT_NetGUIDAssign  NetGUID %s. Path: %s. "), *NetGUID.ToString(), *Path);
					Connection->PackageMap->ResolvePathAndAssignNetGUID(NetGUID, Path);
				}

				break;
			}
		}
	}
	else
	{
		// We are the server.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogNet, Verbose, TEXT("Level server received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		if ( !Connection->IsClientMsgTypeValid( MessageType ) )
		{
			// If we get here, either code is mismatched on the client side, or someone could be spoofing the client address
			UE_LOG(LogNet, Error, TEXT( "IsClientMsgTypeValid FAILED (%i): Remote Address = %s" ), (int)MessageType,
					ToCStr(Connection->LowLevelGetRemoteAddress(true)));
			Bunch.SetError();
			return;
		}
		
		switch (MessageType)
		{
			case NMT_Hello:
			{
				uint8 IsLittleEndian = 0;
				uint32 RemoteNetworkVersion = 0;
				uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
				FString EncryptionToken;

				EEngineNetworkRuntimeFeatures LocalNetworkFeatures = NetDriver->GetNetworkRuntimeFeatures();
				EEngineNetworkRuntimeFeatures RemoteNetworkFeatures = EEngineNetworkRuntimeFeatures::None;

				if (FNetControlMessage<NMT_Hello>::Receive(Bunch, IsLittleEndian, RemoteNetworkVersion, EncryptionToken, RemoteNetworkFeatures))
				{
					const bool bIsCompatible = FNetworkVersion::IsNetworkCompatible(LocalNetworkVersion, RemoteNetworkVersion) && FNetworkVersion::AreNetworkRuntimeFeaturesCompatible(LocalNetworkFeatures, RemoteNetworkFeatures);
					if (!bIsCompatible)
					{
						TStringBuilder<128> LocalNetFeaturesDescription;
						TStringBuilder<128> RemoteNetFeaturesDescription;

						FNetworkVersion::DescribeNetworkRuntimeFeaturesBitset(LocalNetworkFeatures, LocalNetFeaturesDescription);
						FNetworkVersion::DescribeNetworkRuntimeFeaturesBitset(RemoteNetworkFeatures, RemoteNetFeaturesDescription);

						UE_LOG(LogNet, Log, TEXT("NotifyControlMessage: Client %s connecting with invalid version. LocalNetworkVersion: %u, RemoteNetworkVersion: %u, LocalNetFeatures=%s, RemoteNetFeatures=%s"),
							*Connection->GetName(), 
							LocalNetworkVersion, RemoteNetworkVersion,
							LocalNetFeaturesDescription.ToString(), RemoteNetFeaturesDescription.ToString()
						);

						FNetControlMessage<NMT_Upgrade>::Send(Connection, LocalNetworkVersion, LocalNetworkFeatures);
						Connection->FlushNet(true);
						Connection->Close(ENetCloseResult::Upgrade);
#if USE_SERVER_PERF_COUNTERS
						PerfCountersIncrement(TEXT("ClosedConnectionsDueToIncompatibleVersion"));
#endif
					}
					else
					{
						if (EncryptionToken.IsEmpty())
						{
							EEncryptionFailureAction FailureResult = EEncryptionFailureAction::Default;
							
							if (FNetDelegates::OnReceivedNetworkEncryptionFailure.IsBound())
							{
								FailureResult = FNetDelegates::OnReceivedNetworkEncryptionFailure.Execute(Connection);
							}

							const bool bGameplayDisableEncryptionCheck = FailureResult == EEncryptionFailureAction::AllowConnection;
							const bool bEncryptionRequired = NetDriver->IsEncryptionRequired() && !bGameplayDisableEncryptionCheck;

							if (!bEncryptionRequired)
							{
								Connection->SendChallengeControlMessage();
							}
							else
							{
								FString FailureMsg(TEXT("Encryption token missing"));

								UE_LOG(LogNet, Warning, TEXT("%s: No EncryptionToken specified, disconnecting."), ToCStr(Connection->GetName()));

								Connection->SendCloseReason(ENetCloseResult::EncryptionTokenMissing);
								FNetControlMessage<NMT_Failure>::Send(Connection, FailureMsg);
								Connection->FlushNet(true);
								Connection->Close(ENetCloseResult::EncryptionTokenMissing);
							}
						}
						else
						{
							if (FNetDelegates::OnReceivedNetworkEncryptionToken.IsBound())
							{
								FNetDelegates::OnReceivedNetworkEncryptionToken.Execute(EncryptionToken,
									FOnEncryptionKeyResponse::CreateUObject(Connection, &UNetConnection::SendChallengeControlMessage));
							}
							else
							{
								FString FailureMsg(TEXT("Encryption failure"));

								UE_LOG(LogNet, Warning, TEXT("%s: No delegate available to handle encryption token, disconnecting."),
										ToCStr(Connection->GetName()));

								Connection->SendCloseReason(ENetCloseResult::EncryptionFailure);
								FNetControlMessage<NMT_Failure>::Send(Connection, FailureMsg);
								Connection->FlushNet(true);
								Connection->Close(ENetCloseResult::EncryptionFailure);
							}
						}
					}
				}

				break;
			}

			case NMT_Netspeed:
			{
				int32 Rate;

				if (FNetControlMessage<NMT_Netspeed>::Receive(Bunch, Rate))
				{
					Connection->CurrentNetSpeed = FMath::Clamp(Rate, 1800, NetDriver->MaxClientRate);
					UE_LOG(LogNet, Log, TEXT("Client netspeed is %i"), Connection->CurrentNetSpeed);
				}

				break;
			}
			case NMT_Abort:
			{
				break;
			}
			case NMT_Skip:
			{
				break;
			}
			case NMT_Login:
			{
				// Admit or deny the player here.
				FUniqueNetIdRepl UniqueIdRepl;
				FString OnlinePlatformName;
				FString& RequestURL = Connection->RequestURL;

				// Expand the maximum string serialization size, to accommodate extremely large Fortnite join URL's.
				Bunch.ArMaxSerializeSize += (16 * 1024 * 1024);

				bool bReceived = FNetControlMessage<NMT_Login>::Receive(Bunch, Connection->ClientResponse, RequestURL, UniqueIdRepl,
																		OnlinePlatformName);

				Bunch.ArMaxSerializeSize -= (16 * 1024 * 1024);

				if (bReceived)
				{
					// Only the options/portal for the URL should be used during join
					const TCHAR* NewRequestURL = *RequestURL;

					for (; *NewRequestURL != '\0' && *NewRequestURL != '?' && *NewRequestURL != '#'; NewRequestURL++){}


					UE_LOG(LogNet, Log, TEXT("Login request: %s userId: %s platform: %s"), NewRequestURL, UniqueIdRepl.IsValid() ? *UniqueIdRepl.ToDebugString() : TEXT("UNKNOWN"), *OnlinePlatformName);

					// Compromise for passing splitscreen playercount through to gameplay login code,
					// without adding a lot of extra unnecessary complexity throughout the login code.
					// NOTE: This code differs from NMT_JoinSplit, by counting + 1 for SplitscreenCount
					//			(since this is the primary connection, not counted in Children)
					FURL InURL( NULL, NewRequestURL, TRAVEL_Absolute );

					if ( !InURL.Valid )
					{
						RequestURL = NewRequestURL;

						UE_LOG( LogNet, Error, TEXT( "NMT_Login: Invalid URL %s" ), *RequestURL );
						Bunch.SetError();
						break;
					}

					int32 SplitscreenCount = FMath::Min(Connection->Children.Num() + 1, 255);

					// Don't allow clients to specify this value
					InURL.RemoveOption(TEXT("SplitscreenCount"));
					InURL.AddOption(*FString::Printf(TEXT("SplitscreenCount=%i"), SplitscreenCount));

					RequestURL = InURL.ToString();

					// skip to the first option in the URL
					const TCHAR* Tmp = *RequestURL;
					for (; *Tmp && *Tmp != '?'; Tmp++);

					// keep track of net id for player associated with remote connection
					Connection->PlayerId = UniqueIdRepl;

					// keep track of the online platform the player associated with this connection is using.
					Connection->SetPlayerOnlinePlatformName(FName(*OnlinePlatformName));

					// ask the game code if this player can join
					AGameModeBase* GameMode = GetAuthGameMode();
					AGameModeBase::FOnPreLoginCompleteDelegate OnComplete = AGameModeBase::FOnPreLoginCompleteDelegate::CreateUObject(
						this, &UWorld::PreLoginComplete, TWeakObjectPtr<UNetConnection>(Connection));
					if (GameMode)
					{
						GameMode->PreLoginAsync(Tmp, Connection->LowLevelGetRemoteAddress(), Connection->PlayerId, OnComplete);
					}
					else
					{
						OnComplete.ExecuteIfBound(FString());
					}
				}
				else
				{
					Connection->ClientResponse.Empty();
					RequestURL.Empty();
				}

				break;
			}
			case NMT_Join:
			{
				if (Connection->PlayerController == NULL)
				{
					// Spawn the player-actor for this network player.
					FString ErrorMsg;
					UE_LOG(LogNet, Log, TEXT("Join request: %s"), *Connection->RequestURL);

					FURL InURL( NULL, *Connection->RequestURL, TRAVEL_Absolute );

					if ( !InURL.Valid )
					{
						UE_LOG( LogNet, Error, TEXT( "NMT_Login: Invalid URL %s" ), *Connection->RequestURL );
						Bunch.SetError();
						break;
					}

					Connection->PlayerController = SpawnPlayActor( Connection, ROLE_AutonomousProxy, InURL, Connection->PlayerId, ErrorMsg );
					if (Connection->PlayerController == NULL)
					{
						// Failed to connect.
						UE_LOG(LogNet, Log, TEXT("Join failure: %s"), *ErrorMsg);
						NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("JOIN FAILURE"), *ErrorMsg, Connection));

						Connection->SendCloseReason(ENetCloseResult::JoinFailure);
						FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
						Connection->FlushNet(true);
						Connection->Close(ENetCloseResult::JoinFailure);
					}
					else
					{
						// Successfully in game.
						UE_LOG(LogNet, Log, TEXT("Join succeeded: %s"), *Connection->PlayerController->PlayerState->GetPlayerName());
						NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("JOIN"), *Connection->PlayerController->PlayerState->GetPlayerName(), Connection));

						Connection->SetClientLoginState(EClientLoginState::ReceivedJoin);

						// if we're in the middle of a transition or the client is in the wrong world, tell it to travel
						FString LevelName;
						FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );

						if (SeamlessTravelHandler.IsInTransition())
						{
							// tell the client to go to the destination map
							LevelName = SeamlessTravelHandler.GetDestinationMapName();
						}
						else if (!Connection->PlayerController->HasClientLoadedCurrentWorld())
						{
							// tell the client to go to our current map
							FString NewLevelName = GetOutermost()->GetName();
							UE_LOG(LogNet, Log, TEXT("Client joined but was sent to another level. Asking client to travel to: '%s'"), *NewLevelName);
							LevelName = NewLevelName;
						}
						if (LevelName != TEXT(""))
						{
							Connection->PlayerController->ClientTravel(LevelName, TRAVEL_Relative, true);
						}

						// @TODO FIXME - TEMP HACK? - clear queue on join
						Connection->QueuedBits = 0;
					}
				}
				break;
			}
			case NMT_JoinSplit:
			{
				// Handle server-side request for spawning a new controller using a child connection.
				FString SplitRequestURL;
				FUniqueNetIdRepl SplitRequestUniqueIdRepl;

				if (FNetControlMessage<NMT_JoinSplit>::Receive(Bunch, SplitRequestURL, SplitRequestUniqueIdRepl))
				{
					// Only the options/portal for the URL should be used during join
					const TCHAR* NewRequestURL = *SplitRequestURL;

					for (; *NewRequestURL != '\0' && *NewRequestURL != '?' && *NewRequestURL != '#'; NewRequestURL++){}

					UE_LOG(LogNet, Log, TEXT("Join splitscreen request: %s userId: %s parentUserId: %s"),
						NewRequestURL,
						SplitRequestUniqueIdRepl.IsValid() ? *SplitRequestUniqueIdRepl->ToString() : TEXT("Invalid"),
						Connection->PlayerId.IsValid() ? *Connection->PlayerId->ToString() : TEXT("Invalid"));

					// Compromise for passing splitscreen playercount through to gameplay login code,
					// without adding a lot of extra unnecessary complexity throughout the login code.
					// NOTE: This code differs from NMT_Login, by counting + 2 for SplitscreenCount
					//			(once for pending child connection, once for primary non-child connection)
					FURL InURL(NULL, NewRequestURL, TRAVEL_Absolute);

					if (!InURL.Valid)
					{
						SplitRequestURL = NewRequestURL;

						UE_LOG(LogNet, Error, TEXT("NMT_JoinSplit: Invalid URL %s"), *SplitRequestURL);
						Bunch.SetError();
						break;
					}

					int32 SplitscreenCount = FMath::Min(Connection->Children.Num() + 2, 255);

					// Don't allow clients to specify this value
					InURL.RemoveOption(TEXT("SplitscreenCount"));
					InURL.AddOption(*FString::Printf(TEXT("SplitscreenCount=%i"), SplitscreenCount));

					SplitRequestURL = InURL.ToString();

					// skip to the first option in the URL
					const TCHAR* Tmp = *SplitRequestURL;
					for (; *Tmp && *Tmp != '?'; Tmp++);

					// go through the same full login process for the split player
					AGameModeBase* GameMode = GetAuthGameMode();
					AGameModeBase::FOnPreLoginCompleteDelegate OnComplete = AGameModeBase::FOnPreLoginCompleteDelegate::CreateUObject(
						this, &UWorld::PreLoginCompleteSplit, TWeakObjectPtr<UNetConnection>(Connection), SplitRequestUniqueIdRepl, SplitRequestURL);
					if (GameMode)
					{
						GameMode->PreLoginAsync(Tmp, Connection->LowLevelGetRemoteAddress(), SplitRequestUniqueIdRepl, OnComplete);
					}
					else
					{
						OnComplete.ExecuteIfBound(FString());
					}
				}

				break;
			}
			case NMT_PCSwap:
			{
				UNetConnection* SwapConnection = Connection;
				int32 ChildIndex;

				if (FNetControlMessage<NMT_PCSwap>::Receive(Bunch, ChildIndex))
				{
					if (ChildIndex >= 0)
					{
						SwapConnection = Connection->Children.IsValidIndex(ChildIndex) ? ToRawPtr(Connection->Children[ChildIndex]) : nullptr;
					}

					bool bSuccess = false;

					if (SwapConnection != nullptr)
					{
						bSuccess = DestroySwappedPC(SwapConnection);
					}

					if (!bSuccess)
					{
						UE_LOG(LogNet, Log, TEXT("Received invalid swap message with child index %i"), ChildIndex);
					}
				}

				break;
			}
			case NMT_DebugText:
			{
				// debug text message
				FString Text;

				if (FNetControlMessage<NMT_DebugText>::Receive(Bunch, Text))
				{
					UE_LOG(LogNet, Log, TEXT("%s received NMT_DebugText Text=[%s] Desc=%s DescRemote=%s"),
							*Connection->Driver->GetDescription(), *Text, *Connection->LowLevelDescribe(),
							ToCStr(Connection->LowLevelGetRemoteAddress(true)));
				}

				break;
			}
		}
	}
}

bool UWorld::PreLoginCheckError(UNetConnection* Connection, const FString& ErrorMsg)
{
	if (Connection)
	{
		if (ErrorMsg.IsEmpty())
		{
			return true;
		}

		UE_LOG(LogNet, Log, TEXT("PreLogin failure: %s"), *ErrorMsg);
		NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("PRELOGIN FAILURE"), *ErrorMsg, Connection));
		Connection->SendCloseReason(ENetCloseResult::PreLoginFailure);
		FString ErrorMsgCopy(ErrorMsg); // Needed because Send() can't handle const FString.
		FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsgCopy);
		Connection->FlushNet(true);
		Connection->Close(ENetCloseResult::PreLoginFailure);
	}
	else
	{
		UE_LOG(LogNet, Log, TEXT("PreLogin failure: connection was null"));
	}

	return false;
}

void UWorld::PreLoginComplete(const FString& ErrorMsg, TWeakObjectPtr<UNetConnection> WeakConnection)
{
	UNetConnection* Connection = WeakConnection.Get();
	if (!PreLoginCheckError(Connection, ErrorMsg))
	{
		return;
	}

	WelcomePlayer(Connection);
}

void UWorld::PreLoginCompleteSplit(const FString& ErrorMsg, TWeakObjectPtr<UNetConnection> WeakConnection, FUniqueNetIdRepl SplitRequestUniqueIdRepl, FString SplitRequestURL)
{
	UNetConnection* Connection = WeakConnection.Get();
	if (!PreLoginCheckError(Connection, ErrorMsg))
	{
		return;
	}

	// create a child network connection using the existing connection for its parent
	check(Connection->GetUChildConnection() == NULL);
#if WITH_EDITORONLY_DATA
	check(CurrentLevel);
#else
	ULevel* CurrentLevel = PersistentLevel;
#endif

	UChildConnection* ChildConn = NetDriver->CreateChild(Connection);
	ChildConn->PlayerId = SplitRequestUniqueIdRepl;
	ChildConn->SetPlayerOnlinePlatformName(Connection->GetPlayerOnlinePlatformName());
	ChildConn->RequestURL = SplitRequestURL;
	ChildConn->SetClientWorldPackageName(CurrentLevel->GetOutermost()->GetFName());

	// create URL from string
	FURL JoinSplitURL(NULL, *SplitRequestURL, TRAVEL_Absolute);

	UE_LOG(LogNet, Log, TEXT("JOINSPLIT: Join request: URL=%s"), *JoinSplitURL.ToString());
	FString SpawnErrorMsg;
	APlayerController* PC = SpawnPlayActor(ChildConn, ROLE_AutonomousProxy, JoinSplitURL, ChildConn->PlayerId, SpawnErrorMsg, uint8(Connection->Children.Num()));
	if (PC == NULL)
	{
		// Failed to connect.
		UE_LOG(LogNet, Log, TEXT("JOINSPLIT: Join failure: %s"), *SpawnErrorMsg);
		NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("JOINSPLIT FAILURE"), *SpawnErrorMsg, Connection));
		// remove the child connection
		Connection->Children.Remove(ChildConn);

		// if any splitscreen viewport fails to join, all viewports on that client also fail
		Connection->SendCloseReason(ENetCloseResult::JoinSplitFailure);
		FNetControlMessage<NMT_Failure>::Send(Connection, SpawnErrorMsg);
		Connection->FlushNet(true);
		Connection->Close(ENetCloseResult::JoinSplitFailure);
	}
	else
	{
		// Successfully spawned in game.
		UE_LOG(LogNet, Log, TEXT("JOINSPLIT: Succeeded: %s PlayerId: %s"),
			*ChildConn->PlayerController->PlayerState->GetPlayerName(),
			*ChildConn->PlayerController->PlayerState->GetUniqueId().ToDebugString());
	}
}

bool UWorld::Listen( FURL& InURL )
{
#if WITH_SERVER_CODE
	LLM_SCOPE(ELLMTag::Networking);

	if( NetDriver )
	{
		GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::NetDriverAlreadyExists);
		return false;
	}

	// Create net driver.
	if (GEngine->CreateNamedNetDriver(this, NAME_GameNetDriver, NAME_GameNetDriver))
	{
		NetDriver = GEngine->FindNamedNetDriver(this, NAME_GameNetDriver);
		NetDriver->SetWorld(this);
		FLevelCollection* const SourceCollection = FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
		if (SourceCollection)
		{
			SourceCollection->SetNetDriver(NetDriver);
		}
		FLevelCollection* const StaticCollection = FindCollectionByType(ELevelCollectionType::StaticLevels);
		if (StaticCollection)
		{
			StaticCollection->SetNetDriver(NetDriver);
		}
	}

	if (NetDriver == nullptr)
	{
		GEngine->BroadcastNetworkFailure(this, NULL, ENetworkFailure::NetDriverCreateFailure);
		return false;
	}

	AWorldSettings* WorldSettings = GetWorldSettings();
	const bool bReuseAddressAndPort = WorldSettings ? WorldSettings->bReuseAddressAndPort : false;

	FString Error;
	if( !NetDriver->InitListen( this, InURL, bReuseAddressAndPort, Error ) )
	{
		GEngine->BroadcastNetworkFailure(this, NetDriver, ENetworkFailure::NetDriverListenFailure, Error);
		UE_LOG(LogWorld, Log,  TEXT("Failed to listen: %s"), *Error );
		GEngine->DestroyNamedNetDriver(this, NetDriver->NetDriverName);
		NetDriver = nullptr;
		FLevelCollection* SourceCollection = FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
		if (SourceCollection)
		{
			SourceCollection->SetNetDriver(nullptr);
		}
		FLevelCollection* StaticCollection = FindCollectionByType(ELevelCollectionType::StaticLevels);
		if (StaticCollection)
		{
			StaticCollection->SetNetDriver(nullptr);
		}
		return false;
	}
	static const bool bLanPlay = FParse::Param(FCommandLine::Get(),TEXT("lanplay"));
	const bool bLanSpeed = bLanPlay || InURL.HasOption(TEXT("LAN"));
	if ( !bLanSpeed && (NetDriver->MaxInternetClientRate < NetDriver->MaxClientRate) && (NetDriver->MaxInternetClientRate > 2500) )
	{
		NetDriver->MaxClientRate = NetDriver->MaxInternetClientRate;
	}

	NextSwitchCountdown = NetDriver->ServerTravelPause;
	return true;
#else
	return false;
#endif // WITH_SERVER_CODE
}

bool UWorld::IsPlayingReplay() const
{
	return (DemoNetDriver && DemoNetDriver->IsPlaying());
}

bool UWorld::IsRecordingReplay() const
{
	const UGameInstance* GameInst = GetGameInstance();
	const UReplaySubsystem* ReplaySubsystem = GameInst ? GameInst->GetSubsystem<UReplaySubsystem>() : nullptr;

	if (ReplaySubsystem)
	{
		return ReplaySubsystem->IsRecording();
	}
	else
	{
		// Using IsServer() because it also calls IsRecording() internally
		return (DemoNetDriver && DemoNetDriver->IsServer());
	}
}

void UWorld::PrepareMapChange(const TArray<FName>& LevelNames)
{
	// Kick off async loading request for those maps.
	if( !GEngine->PrepareMapChange(this, LevelNames) )
	{
		UE_LOG(LogWorld, Warning,TEXT("Preparing map change via %s was not successful: %s"), *GetFullName(), *GEngine->GetMapChangeFailureDescription(this) );
	}
}

bool UWorld::IsPreparingMapChange() const
{
	return GEngine->IsPreparingMapChange(const_cast<UWorld*>(this));
}

bool UWorld::IsMapChangeReady() const
{
	return GEngine->IsReadyForMapChange(const_cast<UWorld*>(this));
}

void UWorld::CancelPendingMapChange()
{
	GEngine->CancelPendingMapChange(this);
}

void UWorld::CommitMapChange()
{
	if( IsPreparingMapChange() )
	{
		GEngine->SetShouldCommitPendingMapChange(this, true);
	}
	else
	{
		UE_LOG(LogWorld, Log, TEXT("AWorldSettings::CommitMapChange being called without a pending map change!"));
	}
}

FTimerManager& UWorld::GetTimerManager() const
{
	return (OwningGameInstance ? OwningGameInstance->GetTimerManager() : *TimerManager);
}

FLatentActionManager& UWorld::GetLatentActionManager()
{
	return (OwningGameInstance ? OwningGameInstance->GetLatentActionManager() : LatentActionManager);
}

void UWorld::RequestNewWorldOrigin(FIntVector InNewOriginLocation)
{
	RequestedOriginLocation = InNewOriginLocation;
}

bool UWorld::SetNewWorldOrigin(FIntVector InNewOriginLocation)
{
	if (OriginLocation == InNewOriginLocation) 
	{
		return true;
	}
	
	// We cannot shift world origin while Level is in the process to be added to a world
	// it will cause wrong positioning for this level 
	if (IsVisibilityRequestPending())
	{
		return false;
	}
	
	UE_LOG(LogLevel, Log, TEXT("WORLD TRANSLATION BEGIN {%d, %d, %d} -> {%d, %d, %d}"), 
		OriginLocation.X, OriginLocation.Y, OriginLocation.Z, InNewOriginLocation.X, InNewOriginLocation.Y, InNewOriginLocation.Z);

	const double MoveStartTime = FPlatformTime::Seconds();

	// Broadcast that we going to shift world to the new origin
	FCoreDelegates::PreWorldOriginOffset.Broadcast(this, OriginLocation, InNewOriginLocation);

	FVector Offset(OriginLocation - InNewOriginLocation);
	OriginOffsetThisFrame = Offset;

	// Send offset command to rendering thread
	Scene->ApplyWorldOffset(Offset);

	// Shift physics scene
	if (PhysicsScene && FPhysScene::SupportsOriginShifting())
	{
		PhysicsScene->ApplyWorldOffset(Offset);
	}
		
	// Apply offset to all visible levels
	for(int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel* LevelToShift = Levels[LevelIndex];
		
		// Only visible sub-levels need to be shifted
		// Hidden sub-levels will be shifted once they become visible in UWorld::AddToWorld
		if (LevelToShift->bIsVisible || LevelToShift->IsPersistentLevel())
		{
			LevelToShift->ApplyWorldOffset(Offset, true);
		}
	}

	// Shift navigation meshes
	if (NavigationSystem)
	{
		NavigationSystem->ApplyWorldOffset(Offset, true);
	}

	// Apply offset to components with no actor (like UGameplayStatics::SpawnEmitterAtLocation) 
	{
		TArray <UObject*> WorldChildren; 
		GetObjectsWithOuter(this, WorldChildren, false);

		for (UObject* ChildObject : WorldChildren)
		{
		   UActorComponent* Component = Cast<UActorComponent>(ChildObject);
		   if (Component && Component->GetOwner() == nullptr)
		   {
				Component->ApplyWorldOffset(Offset, true);
		   }
		}
	}
			
	if (LineBatcher)
	{
		LineBatcher->ApplyWorldOffset(Offset, true);
	}
	
	if (PersistentLineBatcher)
	{
		PersistentLineBatcher->ApplyWorldOffset(Offset, true);
	}
	
	if (ForegroundLineBatcher)
	{
		ForegroundLineBatcher->ApplyWorldOffset(Offset, true);
	}

	if (PhysicsField)
	{
		PhysicsField->ApplyWorldOffset(Offset, true);
	}

	FIntVector PreviosWorldOriginLocation = OriginLocation;
	// Set new world origin
	OriginLocation = InNewOriginLocation;
	RequestedOriginLocation = InNewOriginLocation;
	
	// Propagate event to a level blueprints
	for(int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel* Level = Levels[LevelIndex];
		if (Level->bIsVisible && 
			Level->LevelScriptActor)
		{
			Level->LevelScriptActor->WorldOriginLocationChanged(PreviosWorldOriginLocation, OriginLocation);
		}
	}

	if (AISystem != NULL)
	{
		AISystem->WorldOriginLocationChanged(PreviosWorldOriginLocation, OriginLocation);
	}

	// Broadcast that have finished world shifting
	FCoreDelegates::PostWorldOriginOffset.Broadcast(this, PreviosWorldOriginLocation, OriginLocation);

	const double CurrentTime = FPlatformTime::Seconds();
	const double TimeTaken = CurrentTime - MoveStartTime;
	UE_LOG(LogLevel, Log, TEXT("WORLD TRANSLATION END {%d, %d, %d} took %.4f ms"),
		OriginLocation.X, OriginLocation.Y, OriginLocation.Z, TimeTaken * 1000);
	
	return true;
}

void UWorld::NavigateTo(FIntVector InLocation)
{
	check(WorldComposition != NULL);

	SetNewWorldOrigin(InLocation);
	WorldComposition->UpdateStreamingState(FVector::ZeroVector);
	FlushLevelStreaming();
}

/*-----------------------------------------------------------------------------
	Seamless world traveling
-----------------------------------------------------------------------------*/

void FSeamlessTravelHandler::SetHandlerLoadedData(UObject* InLevelPackage, UWorld* InLoadedWorld)
{
	LoadedPackage = InLevelPackage;
	LoadedWorld = InLoadedWorld;
	if (LoadedWorld != NULL)
	{
		LoadedWorld->AddToRoot();
	}

}

/** callback sent to async loading code to inform us when the level package is complete */
void FSeamlessTravelHandler::SeamlessTravelLoadCallback(const FName& PackageName, UPackage* LevelPackage, EAsyncLoadingResult::Type Result)
{
	// make sure we remove the name, even if travel was canceled.
	const FName URLMapFName = FName(*PendingTravelURL.Map);
	UWorld::WorldTypePreLoadMap.Remove(URLMapFName);

#if WITH_EDITOR
	if (GIsEditor)
	{
		FWorldContext &WorldContext = GEngine->GetWorldContextFromHandleChecked(WorldContextHandle);
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			FString URLMapPackageName = UWorld::ConvertToPIEPackageName(PendingTravelURL.Map, WorldContext.PIEInstance);
			UWorld::WorldTypePreLoadMap.Remove(FName(*URLMapPackageName));
		}
	}
#endif

	// defer until tick when it's safe to perform the transition
	if (IsInTransition())
	{
		UWorld* World = UWorld::FindWorldInPackage(LevelPackage);

		// If the world could not be found, follow a redirector if there is one.
		if (!World)
		{
			World = UWorld::FollowWorldRedirectorInPackage(LevelPackage);
			if (World)
			{
				LevelPackage = World->GetOutermost();
			}
		}

		SetHandlerLoadedData(LevelPackage, World);

		// Now that the p map is loaded, start async loading any always loaded levels
		if (World)
		{
			if (World->WorldType == EWorldType::PIE)
			{
				if (LevelPackage->GetPIEInstanceID() != -1)
				{
					World->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(LevelPackage->GetPIEInstanceID());
				}
				else
				{
					// If this is a PIE world but the PIEInstanceID is -1, that implies this world is a temporary save
					// for multi-process PIE which should have been saved with the correct StreamingLevelsPrefix.
					ensure(!World->StreamingLevelsPrefix.IsEmpty());
				}
			}

			if (World->PersistentLevel)
			{
				World->PersistentLevel->HandleLegacyMapBuildData();
			}

			World->AsyncLoadAlwaysLoadedLevelsForSeamlessTravel();
		}
	}

	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "StartTravelComplete - " ) + PackageName.ToString() )) );
	TRACE_BOOKMARK(TEXT("StartTravelComplete - %s"), *PackageName.ToString());
}

bool FSeamlessTravelHandler::StartTravel(UWorld* InCurrentWorld, const FURL& InURL)
{
	FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(InCurrentWorld);
	WorldContextHandle = Context.ContextHandle;

	SeamlessTravelStartTime = FPlatformTime::Seconds();

	if (!InURL.Valid)
	{
		UE_LOG(LogWorld, Error, TEXT("Invalid travel URL specified"));
		return false;
	}
	else
	{
		FLoadTimeTracker::Get().ResetRawLoadTimes();
		UE_LOG(LogWorld, Log, TEXT("SeamlessTravel to: %s"), *InURL.Map);
		FString MapName = UWorld::RemovePIEPrefix(InURL.Map);
		if (!FPackageName::DoesPackageExist(MapName))
		{
			UE_LOG(LogWorld, Error, TEXT("Unable to travel to '%s' - file not found"), *MapName);
			return false;
			// @todo: might have to handle this more gracefully to handle downloading (might also need to send GUID and check it here!)
		}
		else
		{
			bool bCancelledExisting = false;
			if (IsInTransition())
			{
				if (PendingTravelURL.Map == InURL.Map)
				{
					// we are going to the same place so just replace the options
					PendingTravelURL = InURL;
					return true;
				}
				UE_LOG(LogWorld, Warning, TEXT("Cancelling travel to '%s' to go to '%s' instead"), *PendingTravelURL.Map, *InURL.Map);
				CancelTravel();
				bCancelledExisting = true;
			}

			// CancelTravel will null out CurrentWorld, so we need to assign it after that.
			CurrentWorld = InCurrentWorld;

			FWorldDelegates::OnSeamlessTravelStart.Broadcast(CurrentWorld, InURL.Map);

			checkSlow(LoadedPackage == NULL);
			checkSlow(LoadedWorld == NULL);

			PendingTravelURL = InURL;
			bSwitchedToDefaultMap = false;
			bTransitionInProgress = true;
			bPauseAtMidpoint = false;
			bNeedCancelCleanUp = false;

			FName CurrentMapName = CurrentWorld->GetOutermost()->GetFName();
			FName DestinationMapName = FName(*PendingTravelURL.Map);

			FString TransitionMap = GetDefault<UGameMapsSettings>()->TransitionMap.GetLongPackageName();
			FName DefaultMapFinalName(*TransitionMap);

			// if we're already in the default map, skip loading it and just go to the destination
			if (DefaultMapFinalName == CurrentMapName ||
				DefaultMapFinalName == DestinationMapName)
			{
				UE_LOG(LogWorld, Log, TEXT("Already in default map or the default map is the destination, continuing to destination"));
				bSwitchedToDefaultMap = true;
				if (bCancelledExisting)
				{
					// we need to fully finishing loading the old package and GC it before attempting to load the new one
					bPauseAtMidpoint = true;
					bNeedCancelCleanUp = true;
				}
				else
				{
					StartLoadingDestination();
				}
			}
			else
			{
				UNetDriver* const NetDriver = CurrentWorld->GetNetDriver();
				if (NetDriver)
				{
					for (int32 ClientIdx = 0; ClientIdx < NetDriver->ClientConnections.Num(); ClientIdx++)
					{
						UNetConnection* Connection = NetDriver->ClientConnections[ClientIdx];
						if (Connection)
						{
							// Empty the current map name on all transitions because the server could try to spawn actors 
							// before the client starts the transfer causing the server to think its loaded
							Connection->SetClientWorldPackageName(NAME_None);
						}
					}
				}
				
				if (TransitionMap.IsEmpty())
				{
					// If a default transition map doesn't exist, create a dummy World to use as the transition
					EWorldType::Type TransitionWorldType = CurrentWorld->WorldType == EWorldType::PIE ? EWorldType::PIE : EWorldType::None;
					SetHandlerLoadedData(nullptr, UWorld::CreateWorld(TransitionWorldType, false));
				}
				else
				{
					// Load the transition map, possibly with PIE prefix
					STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "StartTravel - " ) + TransitionMap )) );
					TRACE_BOOKMARK(TEXT("StartTravel - %s"), *TransitionMap);

					if (!StartLoadingMap(TransitionMap))
					{
						UE_LOG(LogWorld, Error, TEXT("StartTravel: Invalid TransitionMap \"%s\""), *TransitionMap);
					}
				}
			}

			return true;
		}
	}
}

/** cancels transition in progress */
void FSeamlessTravelHandler::CancelTravel()
{
	LoadedPackage = NULL;
	if (LoadedWorld != NULL)
	{
		LoadedWorld->RemoveFromRoot();
		LoadedWorld->ClearFlags(RF_Standalone);
		LoadedWorld = NULL;
	}

	if (bTransitionInProgress)
	{
		UPackage* Package = CurrentWorld ? CurrentWorld->GetOutermost() : nullptr;
		if (Package)
		{
			FName CurrentPackageName = Package->GetFName();
			UNetDriver* const NetDriver = CurrentWorld->GetNetDriver();
			if (NetDriver)
			{
				for (int32 ClientIdx = 0; ClientIdx < NetDriver->ClientConnections.Num(); ClientIdx++)
				{
					UNetConnection* Connection = NetDriver->ClientConnections[ClientIdx];
					if (Connection)
					{
						UChildConnection* ChildConnection = Connection->GetUChildConnection();
						if (ChildConnection)
						{
							Connection = ChildConnection->Parent;
						}

						// Mark all clients as being where they are since this was set to None in StartTravel
						Connection->SetClientWorldPackageName(CurrentPackageName);
					}
				}
			}
		}
	
		CurrentWorld = NULL;
		bTransitionInProgress = false;
		UE_LOG(LogWorld, Log, TEXT("----SeamlessTravel is cancelled!------"));
	}
}

void FSeamlessTravelHandler::SetPauseAtMidpoint(bool bNowPaused)
{
	if (!bTransitionInProgress)
	{
		UE_LOG(LogWorld, Warning, TEXT("Attempt to pause seamless travel when no transition is in progress"));
	}
	else if (bSwitchedToDefaultMap && bNowPaused)
	{
		UE_LOG(LogWorld, Warning, TEXT("Attempt to pause seamless travel after started loading final destination"));
	}
	else
	{
		bPauseAtMidpoint = bNowPaused;
		if (!bNowPaused && bSwitchedToDefaultMap)
		{
			// load the final destination now that we're done waiting
			StartLoadingDestination();
		}
	}
}

bool FSeamlessTravelHandler::StartLoadingMap(FString MapPackageToLoadFrom)
{
	if (MapPackageToLoadFrom.IsEmpty())
	{
		return false;
	}

	// In PIE we might want to mangle MapPackageName when traveling to a map loaded in the editor
	FString MapPackageName = MapPackageToLoadFrom;
	EPackageFlags PackageFlags = PKG_None;
	int32 PIEInstanceID = INDEX_NONE;

#if WITH_EDITOR
	if (GIsEditor)
	{
		FWorldContext& WorldContext = GEngine->GetWorldContextFromHandleChecked(WorldContextHandle);

		PIEInstanceID = WorldContext.PIEInstance;
		MapPackageName = UWorld::ConvertToPIEPackageName(MapPackageName, PIEInstanceID);

		if (WorldContext.WorldType == EWorldType::PIE)
		{
			PackageFlags |= PKG_PlayInEditor;

			// Prepare soft object paths for fixup
			FSoftObjectPath::AddPIEPackageName(FName(*MapPackageName));
		}
	}
#endif

	// Set the world type in the static map, so that UWorld::PostLoad can set the world type
	FName PackageFName(*MapPackageName);
	UWorld::WorldTypePreLoadMap.FindOrAdd(PackageFName) = CurrentWorld->WorldType;

	FPackagePath PackagePath;
	if (FPackagePath::TryFromMountedName(MapPackageToLoadFrom, PackagePath))
	{
		LoadPackageAsync(
			PackagePath,
			PackageFName,
			FLoadPackageAsyncDelegate::CreateRaw(this, &FSeamlessTravelHandler::SeamlessTravelLoadCallback),
			PackageFlags,
			PIEInstanceID
		);

		return true;
	}

	return false;
}

void FSeamlessTravelHandler::StartLoadingDestination()
{
	if (bTransitionInProgress && bSwitchedToDefaultMap)
	{
		UE_LOG(LogWorld, Log, TEXT("StartLoadingDestination to: %s"), *PendingTravelURL.Map);

		CurrentWorld->GetGameInstance()->PreloadContentForURL(PendingTravelURL);

		if (!StartLoadingMap(PendingTravelURL.Map))
		{
			UE_LOG(LogWorld, Error, TEXT("StartLoadingDestination: Invalid destination map \"%s\""), *PendingTravelURL.Map);
		}
	}
	else
	{
		UE_LOG(LogWorld, Error, TEXT("Called StartLoadingDestination() when not ready! bTransitionInProgress: %u bSwitchedToDefaultMap: %u"), bTransitionInProgress, bSwitchedToDefaultMap);
		checkSlow(0);
	}
}

void FSeamlessTravelHandler::CopyWorldData()
{
	FWorldDelegates::OnCopyWorldData.Broadcast(CurrentWorld, LoadedWorld);

	FLevelCollection* const CurrentCollection = CurrentWorld->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	FLevelCollection* const CurrentStaticCollection = CurrentWorld->FindCollectionByType(ELevelCollectionType::StaticLevels);
	FLevelCollection* const LoadedCollection = LoadedWorld->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	FLevelCollection* const LoadedStaticCollection = LoadedWorld->FindCollectionByType(ELevelCollectionType::StaticLevels);

	UNetDriver* const NetDriver = CurrentWorld->GetNetDriver();
	LoadedWorld->SetNetDriver(NetDriver);

	if (CurrentCollection && LoadedCollection)
	{
		LoadedCollection->SetNetDriver(NetDriver);
		CurrentCollection->SetNetDriver(nullptr);
	}
	if (CurrentStaticCollection && LoadedStaticCollection)
	{
		LoadedStaticCollection->SetNetDriver(NetDriver);
		CurrentStaticCollection->SetNetDriver(nullptr);
	}

	if (NetDriver != nullptr)
	{
		CurrentWorld->SetNetDriver(nullptr);
		NetDriver->SetWorld(LoadedWorld);
	}
	LoadedWorld->WorldType = CurrentWorld->WorldType;
	LoadedWorld->SetGameInstance(CurrentWorld->GetGameInstance());

	LoadedWorld->TimeSeconds = CurrentWorld->TimeSeconds;
	LoadedWorld->UnpausedTimeSeconds = CurrentWorld->UnpausedTimeSeconds;
	LoadedWorld->RealTimeSeconds = CurrentWorld->RealTimeSeconds;
	LoadedWorld->AudioTimeSeconds = CurrentWorld->AudioTimeSeconds;

	if (NetDriver != nullptr)
	{
		LoadedWorld->NextSwitchCountdown = NetDriver->ServerTravelPause;
	}
}

/** 
 * Version of FArchiveReplaceObjectRef that will also clear references to garbage objects not in the replacement map 
 * This does not try to recursively serialize subobjects because it was unreliable and missed ones without hard parent references
 */
template< class T >
class FArchiveReplaceOrClearGarbageReferences : public FArchiveReplaceObjectRef<T>
{
	typedef FArchiveReplaceObjectRef<T> TSuper;
public:
	FArchiveReplaceOrClearGarbageReferences
		( UObject* InSearchObject
		, const TMap<T*, T*>& InReplacementMap
		, EArchiveReplaceObjectFlags Flags = EArchiveReplaceObjectFlags::None)
		: TSuper(InSearchObject, InReplacementMap, EArchiveReplaceObjectFlags::DelayStart | Flags)
	{
		if (!(Flags & EArchiveReplaceObjectFlags::DelayStart))
		{
			this->SerializeSingleSearchObject();
		}
	}

	void SerializeSingleSearchObject()
	{
		TSuper::ReplacedReferences.Reset();

		// Difference from parent behavior is to always run even if map is empty, and to ignore subobjects
		TSuper::SerializedObjects.Add(TSuper::SearchObject);
		TSuper::SerializingObject = TSuper::SearchObject;
		TSuper::SerializeObject(TSuper::SearchObject);
	}

	FArchive& operator<<(UObject*& Obj) override
	{
		UObject* Resolved = Obj;
		TSuper::operator<<(Resolved);

		// if Resolved is garbage, just clear the reference:
		if (Resolved && !IsValid(Resolved))
		{
			Resolved = nullptr;
		}
		Obj = Resolved;
		return *this;
	}
};

UWorld* FSeamlessTravelHandler::Tick()
{
	bool bWorldChanged = false;
	if (bNeedCancelCleanUp)
	{
		if (!IsAsyncLoading())
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
			bNeedCancelCleanUp = false;
			SetPauseAtMidpoint(false);
		}
	}
	//@fixme: wait for client to verify packages before finishing transition. Is this the right fix?
	// Once the default map is loaded, go ahead and start loading the destination map
	// Once the destination map is loaded, wait until all packages are verified before finishing transition

	check(CurrentWorld);

	UNetDriver* NetDriver = CurrentWorld->GetNetDriver();

	if ( ( LoadedPackage != nullptr || LoadedWorld != nullptr ) && CurrentWorld->NextURL == TEXT( "" ) )
	{
		// Wait for async loads to finish before finishing seamless. (E.g., we've loaded the persistent map but are still loading 'always loaded' sub levels)
		if (LoadedWorld)
		{
			if (IsAsyncLoading() )
			{
				return nullptr;
			}
		}

		// First some validity checks		
		if( CurrentWorld == LoadedWorld )
		{
			// We are not going anywhere - this is the same world. 
			FString Error = FString::Printf(TEXT("Travel aborted - new world is the same as current world" ) );
			UE_LOG(LogWorld, Error, TEXT("%s"), *Error);
			// abort
			CancelTravel();			
		}
		else if ( LoadedWorld == nullptr || LoadedWorld->PersistentLevel == nullptr)
		{
			// Package isn't a level
			FString Error = FString::Printf(TEXT("Unable to travel to '%s' - package is not a level"), LoadedPackage ? *LoadedPackage->GetName() : *LoadedWorld->GetName());
			UE_LOG(LogWorld, Error, TEXT("%s"), *Error);
			// abort
			CancelTravel();
			GEngine->BroadcastTravelFailure(CurrentWorld, ETravelFailure::NoLevel, Error);
		}
		else
		{
			// Make sure there are no pending visibility requests.
			CurrentWorld->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
				
			if (!GIsEditor && !IsRunningDedicatedServer() && bSwitchedToDefaultMap)
			{
				// If requested, duplicate dynamic levels here after the source levels are created.
				LoadedWorld->DuplicateRequestedLevels(LoadedWorld->GetOuter()->GetFName());
			}

			if (CurrentWorld->GetGameState())
			{
				CurrentWorld->GetGameState()->SeamlessTravelTransitionCheckpoint(!bSwitchedToDefaultMap);
			}
			
			CurrentWorld->BeginTearingDown();

			FWorldDelegates::OnSeamlessTravelTransition.Broadcast(CurrentWorld);

			// mark actors we want to keep
			FUObjectAnnotationSparseBool KeepAnnotation;
			TArray<AActor*> KeepActors;

			if (AGameModeBase* AuthGameMode = CurrentWorld->GetAuthGameMode())
			{
				AuthGameMode->GetSeamlessTravelActorList(!bSwitchedToDefaultMap, KeepActors);
			}

			const bool bIsClient = (CurrentWorld->GetNetMode() == NM_Client);

			// always keep Controllers that belong to players
			if (bIsClient)
			{
				for (FLocalPlayerIterator It(GEngine, CurrentWorld); It; ++It)
				{
					if (It->PlayerController != nullptr)
					{
						KeepAnnotation.Set(It->PlayerController);
					}
				}
			}
			else
			{
				for( FConstControllerIterator Iterator = CurrentWorld->GetControllerIterator(); Iterator; ++Iterator )
				{
					if (AController* Player = Iterator->Get())
					{
						if (Player->ShouldParticipateInSeamlessTravel())
						{
							KeepAnnotation.Set(Player);
						}
					}
				}
			}

			// ask players what else we should keep
			for (FLocalPlayerIterator It(GEngine, CurrentWorld); It; ++It)
			{
				if (It->PlayerController != nullptr)
				{
					It->PlayerController->GetSeamlessTravelActorList(!bSwitchedToDefaultMap, KeepActors);
				}
			}
			// mark all valid actors specified
			for (AActor* KeepActor : KeepActors)
			{
				if (KeepActor != nullptr)
				{
					KeepAnnotation.Set(KeepActor);
				}
			} 

			TArray<AActor*> ActuallyKeptActors;
			ActuallyKeptActors.Reserve(KeepAnnotation.Num());

			// Rename dynamic actors in the old world's PersistentLevel that we want to keep into the new world
			auto ProcessActor = [this, &KeepAnnotation, &ActuallyKeptActors, NetDriver](AActor* TheActor) -> bool
			{
				const bool bIsInCurrentLevel	= TheActor->GetLevel() == CurrentWorld->PersistentLevel;
				const bool bManuallyMarkedKeep	= KeepAnnotation.Get(TheActor);
				const bool bDormant				= TheActor->GetIsReplicated() && (TheActor->NetDormancy > DORM_Awake);
				const bool bKeepNonOwnedActor	= TheActor->GetLocalRole() < ROLE_Authority && !bDormant && !TheActor->IsNetStartupActor();
				const bool bForceExcludeActor	= TheActor->IsA(ALevelScriptActor::StaticClass());

				// Keep if it's in the current level AND it isn't specifically excluded AND it was either marked as should keep OR we don't own this actor
				if (bIsInCurrentLevel && !bForceExcludeActor && (bManuallyMarkedKeep || bKeepNonOwnedActor))
				{
					if (NetDriver != nullptr)
					{
						NetDriver->NotifyActorIsTraveling(TheActor);
					}

					ActuallyKeptActors.Add(TheActor);
					return true;
				}
				else
				{
					if (bManuallyMarkedKeep)
					{
						UE_LOG(LogWorld, Warning, TEXT("Actor '%s' was indicated to be kept but exists in level '%s', not the persistent level.  Actor will not travel."), *TheActor->GetName(), *TheActor->GetLevel()->GetOutermost()->GetName());
					}

					TheActor->RouteEndPlay(EEndPlayReason::LevelTransition);

					// otherwise, set to be deleted
					KeepAnnotation.Clear(TheActor);
					// close any channels for this actor
					if (NetDriver != nullptr)
					{
						NetDriver->NotifyActorLevelUnloaded(TheActor);
					}
					return false;
				}
			};

			// We move everything but the player controllers first, and then the controllers, keeping their relative order, to avoid breaking any call to GetPlayerController with a fixed index.
			for (FActorIterator It(CurrentWorld); It; ++It)
			{
				AActor* TheActor = *It;
				if (!TheActor->IsA(APlayerController::StaticClass()))
				{
					ProcessActor(TheActor);
				}
			}

			for (FConstPlayerControllerIterator Iterator = CurrentWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				if (APlayerController* Player = Iterator->Get())
				{
					ProcessActor(Player);
				}
			}

			if (NetDriver)
			{
				NetDriver->CleanupWorldForSeamlessTravel();
			}

			bool bCreateNewGameMode = !bIsClient;
			TArray<AController*> KeptControllers;
			{
				// scope because after GC the kept pointers will be bad
				AGameModeBase* KeptGameMode = nullptr;
				AGameStateBase* KeptGameState = nullptr;

				// Second pass to rename and move actors that need to transition into the new world
				// This is done after cleaning up actors that aren't transitioning in case those actors depend on these
				// actors being in the same world.
				for (AActor* const TheActor : ActuallyKeptActors)
				{
					KeepAnnotation.Clear(TheActor);
					
					// if it's a Controller, remove it from the appropriate list in the current world's WorldSettings
					if (AController* Controller = Cast<AController>(TheActor))
					{
						CurrentWorld->RemoveController(Controller);
						KeptControllers.Add(Controller);
					}
					else if (TheActor->IsA<AGameModeBase>())
					{
						KeptGameMode = static_cast<AGameModeBase*>(TheActor);
					}
					else if (TheActor->IsA<AGameStateBase>())
					{
						KeptGameState = static_cast<AGameStateBase*>(TheActor);
					}

					TheActor->Rename(nullptr, LoadedWorld->PersistentLevel);

					TheActor->bActorSeamlessTraveled = true;
				}

				if (KeptGameMode)
				{
					LoadedWorld->CopyGameState(KeptGameMode, KeptGameState);
					bCreateNewGameMode = false;
				}

				CopyWorldData(); // This copies the net driver too (LoadedWorld now has whatever NetDriver was previously held by CurrentWorld)
			}

			// only consider session ended if we're making the final switch so that HUD, etc. UI elements stay around until the end
			CurrentWorld->CleanupWorld(bSwitchedToDefaultMap);
			CurrentWorld->RemoveFromRoot();
			CurrentWorld->ClearFlags(RF_Standalone);

			// Stop all audio to remove references to old world
			if (FAudioDevice* AudioDevice = CurrentWorld->GetAudioDeviceRaw())
			{
				AudioDevice->Flush(CurrentWorld);
			}

			// Copy the standby cheat status
			bool bHasStandbyCheatTriggered = (CurrentWorld->NetworkManager) ? CurrentWorld->NetworkManager->bHasStandbyCheatTriggered : false;

			// the new world should not be garbage collected
			LoadedWorld->AddToRoot();
			
			// Update the FWorldContext to point to the newly loaded world
			FWorldContext &CurrentContext = GEngine->GetWorldContextFromWorldChecked(CurrentWorld);
			CurrentContext.SetCurrentWorld(LoadedWorld);

			LoadedWorld->WorldType = CurrentContext.WorldType;
			if (CurrentContext.WorldType == EWorldType::PIE)
			{
				UPackage * WorldPackage = LoadedWorld->GetOutermost();
				check(WorldPackage);
				WorldPackage->SetPackageFlags(PKG_PlayInEditor);
			}

			// Clear any world specific state from NetDriver before switching World
			if (NetDriver)
			{
				NetDriver->PreSeamlessTravelGarbageCollect();

				// Warn if we loaded a game mode that wanted a different replication system from the previous mode.
				if (AGameModeBase* GameMode = LoadedWorld->GetAuthGameMode())
				{
					EReplicationSystem LoadedGameModeRepSystem = GameMode->GetGameNetDriverReplicationSystem();
					const bool bIsNetDriverCompatible = LoadedGameModeRepSystem == EReplicationSystem::Default || 
														UE::Net::GetUseIrisReplicationCmdlineValue() != EReplicationSystem::Default ||
														(LoadedGameModeRepSystem == EReplicationSystem::Iris && NetDriver->IsUsingIrisReplication());
					ensureMsgf(bIsNetDriverCompatible, TEXT("Seamless travel loaded game mode %s that wants a different replication system than the current NetDriver uses."), *GetNameSafe(GameMode));
				}
			}

			GWorld = nullptr;

			// mark everything else contained in the world to be deleted
			for (auto LevelIt(CurrentWorld->GetLevelIterator()); LevelIt; ++LevelIt)
			{
				if (const ULevel* Level = *LevelIt)
				{
					UWorld* World = CastChecked<UWorld>(Level->GetOuter());
					if (!World->HasMarkedObjectsPendingKill())
					{
						World->MarkObjectsPendingKill();
					}
				}
			}

			for (ULevelStreaming* LevelStreaming : CurrentWorld->GetStreamingLevels())
			{
				// If an unloaded levelstreaming still has a loaded level we need to mark its objects to be deleted as well
				if (LevelStreaming->GetLoadedLevel())
				{
					UWorld* World = CastChecked<UWorld>(LevelStreaming->GetLoadedLevel()->GetOuter());
					if (!World->HasMarkedObjectsPendingKill())
					{
						World->MarkObjectsPendingKill();
					}
				}
			}

			CurrentWorld = nullptr;

			if (!UObjectBaseUtility::IsGarbageEliminationEnabled())
			{
				// If pending kill is disabled, run an explicit serializer to clear references to garbage objects on the transferred actors
				TMap<UObject*, UObject*> ReplacementMap;

				for (AActor* const TheActor : ActuallyKeptActors)
				{
					auto ClearReferences = [ReplacementMap](UObject* Object)
					{
						FArchiveReplaceOrClearGarbageReferences<UObject> ReplaceAr(Object, ReplacementMap, EArchiveReplaceObjectFlags::IgnoreOuterRef);
					};

					// Process all subobjects, even unreferenced ones
					ClearReferences(TheActor);
					ForEachObjectWithOuter(TheActor, ClearReferences, true, RF_NoFlags, EInternalObjectFlags::Garbage);
				}
			}

			// collect garbage to delete the old world
			// because we marked everything in it pending kill, references will be NULL'ed so we shouldn't end up with any dangling pointers
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );

			if (GIsEditor)
			{
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );
			}

			appDefragmentTexturePool();
			appDumpTextureMemoryStats(TEXT(""));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// verify that we successfully cleaned up the old world
			GEngine->CheckAndHandleStaleWorldObjectReferences(&CurrentContext);
#endif
			// Clean out NetDriver's Packagemaps, since they may have a lot of NULL object ptrs rotting in the lookup maps.
			if (NetDriver)
			{
				NetDriver->PostSeamlessTravelGarbageCollect();
			}

			// set GWorld to the new world and initialize it
			GWorld = LoadedWorld;
			if (!LoadedWorld->bIsWorldInitialized)
			{
				LoadedWorld->InitWorld();
			}

			// add controllers to initialized world 
			for (AController* Controller : KeptControllers)
			{
				LoadedWorld->AddController(Controller);
			}

			bWorldChanged = true;
			// Track session change on seamless travel.
			NETWORK_PROFILER(GNetworkProfiler.TrackSessionChange(true, LoadedWorld->URL));

#if WITH_EDITOR
			// PIE worlds should use the same feature level as the editor
			if (CurrentContext.PIEWorldFeatureLevel != ERHIFeatureLevel::Num && LoadedWorld->GetFeatureLevel() != CurrentContext.PIEWorldFeatureLevel)
			{
				LoadedWorld->ChangeFeatureLevel(CurrentContext.PIEWorldFeatureLevel);
			}
#endif

			checkSlow((LoadedWorld->GetNetMode() == NM_Client) == bIsClient);

			if (bCreateNewGameMode)
			{
				LoadedWorld->SetGameMode(PendingTravelURL);
			}

			// if we've already switched to entry before and this is the transition to the new map, re-create the gameinfo
			if (bSwitchedToDefaultMap && !bIsClient)
			{
				if (FAudioDevice* AudioDevice = LoadedWorld->GetAudioDeviceRaw())
				{
					AudioDevice->SetDefaultBaseSoundMix(LoadedWorld->GetWorldSettings()->DefaultBaseSoundMix);
				}

				// Copy cheat flags if the game info is present
				// @todo FIXMELH - see if this exists, it should not since it's created in GameMode or it's garbage info
				if (LoadedWorld->NetworkManager != nullptr)
				{
					LoadedWorld->NetworkManager->bHasStandbyCheatTriggered = bHasStandbyCheatTriggered;
				}
			}

			// Make sure "always loaded" sub-levels are fully loaded
			{
				SCOPE_LOG_TIME_IN_SECONDS(TEXT("    SeamlessTravel FlushLevelStreaming "), nullptr)
				LoadedWorld->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);	
			}
			
			// Note that AI system will be created only if ai-system-creation conditions are met
			LoadedWorld->CreateAISystem();

			// call initialize functions on everything that wasn't carried over from the old world
			LoadedWorld->InitializeActorsForPlay(PendingTravelURL, false);

			// If using an empty temporary transition world, make sure the world settings aren't replicated
			FString TransitionMap = GetDefault<UGameMapsSettings>()->TransitionMap.GetLongPackageName();
			if (TransitionMap.IsEmpty() && !bSwitchedToDefaultMap)
			{
				AWorldSettings* WorldSettings = LoadedWorld->GetWorldSettings();
				if (WorldSettings)
				{
					WorldSettings->SetReplicates(false);
					LoadedWorld->RemoveNetworkActor(WorldSettings);
				}
			}

			// calling it after InitializeActorsForPlay has been called to have all potential bounding boxed initialized
			FNavigationSystem::AddNavigationSystemToWorld(*LoadedWorld, FNavigationSystemRunMode::GameMode);

			FName LoadedWorldName = FName(*UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName()));

			// send loading complete notifications for all local players
			for (FLocalPlayerIterator It(GEngine, LoadedWorld); It; ++It)
			{
				UE_LOG(LogWorld, Log, TEXT("Sending NotifyLoadedWorld for LP: %s PC: %s"), *It->GetName(), It->PlayerController ? *It->PlayerController->GetName() : TEXT("NoPC"));
				if (It->PlayerController != nullptr)
				{
#if !UE_BUILD_SHIPPING
					LOG_SCOPE_VERBOSITY_OVERRIDE(LogNet, ELogVerbosity::VeryVerbose);
					LOG_SCOPE_VERBOSITY_OVERRIDE(LogNetTraffic, ELogVerbosity::VeryVerbose);
					UE_LOG(LogNet, Verbose, TEXT("NotifyLoadedWorld Begin"));
#endif
					It->PlayerController->NotifyLoadedWorld(LoadedWorldName, bSwitchedToDefaultMap);

					const FName SendLoadedWorldName = It->PlayerController->NetworkRemapPath(LoadedWorldName, false);
					It->PlayerController->ServerNotifyLoadedWorld(SendLoadedWorldName);
#if !UE_BUILD_SHIPPING
					UE_LOG(LogNet, Verbose, TEXT("NotifyLoadedWorld End"));
#endif
				}
				else
				{
					UE_LOG(LogNet, Error, TEXT("No Player Controller during seamless travel for LP: %s."), *It->GetName());
					// @todo add some kind of travel back to main menu
				}
			}

			// we've finished the transition
			LoadedWorld->bWorldWasLoadedThisTick = true;
			
			if (bSwitchedToDefaultMap)
			{
				// we've now switched to the final destination, so we're done
				
				// remember the last used URL
				CurrentContext.LastURL = PendingTravelURL;

				// Flag our transition as completed before we call PostSeamlessTravel.  This 
				// allows for chaining of maps.

				bTransitionInProgress = false;
				
				double TotalSeamlessTravelTime = FPlatformTime::Seconds() - SeamlessTravelStartTime;
				UE_LOG(LogWorld, Log, TEXT("----SeamlessTravel finished in %.2f seconds ------"), TotalSeamlessTravelTime );
				FLoadTimeTracker::Get().DumpRawLoadTimes();

				AGameModeBase* const GameMode = LoadedWorld->GetAuthGameMode();
				if (GameMode)
				{
					// inform the new GameMode so it can handle players that persisted
					GameMode->PostSeamlessTravel();					
				}

				// Called after post seamless travel to make sure players are setup correctly first
				LoadedWorld->BeginPlay();

				FCoreUObjectDelegates::PostLoadMapWithWorld.Broadcast(LoadedWorld);
			}
			else
			{
				bSwitchedToDefaultMap = true;
				CurrentWorld = LoadedWorld;
				if (!bPauseAtMidpoint)
				{
					StartLoadingDestination();
				}
			}			
		}		
	}
	UWorld* OutWorld = nullptr;
	if( bWorldChanged )
	{
		OutWorld = LoadedWorld;
		// Cleanup the old pointers
		LoadedPackage = nullptr;
		LoadedWorld = nullptr;
	}
	
	return OutWorld;
}

/** seamlessly travels to the given URL by first loading the entry level in the background,
 * switching to it, and then loading the specified level. Does not disrupt network communication or disconnect clients.
 * You may need to implement GameMode::GetSeamlessTravelActorList(), PlayerController::GetSeamlessTravelActorList(),
 * GameMode::PostSeamlessTravel(), and/or GameMode::HandleSeamlessTravelPlayer() to handle preserving any information
 * that should be maintained (player teams, etc)
 * This codepath is designed for worlds that use little or no level streaming and GameModes where the game state
 * is reset/reloaded when transitioning. (like UT)
 * @param URL the URL to travel to; must be relative to the current URL (same server)
 * @param bAbsolute (opt) - if true, URL is absolute, otherwise relative
 */
void UWorld::SeamlessTravel(const FString& SeamlessTravelURL, bool bAbsolute)
{
	// construct the URL
	FURL NewURL(&GEngine->LastURLFromWorld(this), *SeamlessTravelURL, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative);
	if (!NewURL.Valid)
	{
		const FString Error = FText::Format( NSLOCTEXT("Engine", "InvalidUrl", "Invalid URL: {0}"), FText::FromString( SeamlessTravelURL ) ).ToString();
		GEngine->BroadcastTravelFailure(this, ETravelFailure::InvalidURL, Error);
	}
	else
	{
		if (NewURL.HasOption(TEXT("Restart")))
		{
			//@todo url cleanup - we should merge the two URLs, not completely replace it
			NewURL = GEngine->LastURLFromWorld(this);
		}
		// tell the handler to start the transition
		FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );
		if (!SeamlessTravelHandler.StartTravel(this, NewURL) && !SeamlessTravelHandler.IsInTransition())
		{
			const FString Error = FText::Format( NSLOCTEXT("Engine", "InvalidUrl", "Invalid URL: {0}"), FText::FromString( SeamlessTravelURL ) ).ToString();
			GEngine->BroadcastTravelFailure(this, ETravelFailure::InvalidURL, Error);
		}
	}
}

/** @return whether we're currently in a seamless transition */
bool UWorld::IsInSeamlessTravel() const
{
	FSeamlessTravelHandler& SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld(const_cast<UWorld*>(this));
	return SeamlessTravelHandler.IsInTransition();
}

/** this function allows pausing the seamless travel in the middle,
 * right before it starts loading the destination (i.e. while in the transition level)
 * this gives the opportunity to perform any other loading tasks before the final transition
 * this function has no effect if we have already started loading the destination (you will get a log warning if this is the case)
 * @param bNowPaused - whether the transition should now be paused
 */
void UWorld::SetSeamlessTravelMidpointPause(bool bNowPaused)
{
	FSeamlessTravelHandler &SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld( this );
	SeamlessTravelHandler.SetPauseAtMidpoint(bNowPaused);
}

int32 UWorld::GetDetailMode() const
{
	return GetCachedScalabilityCVars().DetailMode;
}

/**
 * Updates all physics constraint actor joint locations.
 */
void UWorld::UpdateConstraintActors()
{
	if( bAreConstraintsDirty )
	{
		for( TActorIterator<APhysicsConstraintActor> It(this); It; ++It )
		{
			APhysicsConstraintActor* ConstraintActor = *It;
			if( ConstraintActor->GetConstraintComp())
			{
				ConstraintActor->GetConstraintComp()->UpdateConstraintFrames();
			}
		}
		bAreConstraintsDirty = false;
	}
}

int32 UWorld::GetProgressDenominator() const
{
	return GetActorCount();
}

int32 UWorld::GetActorCount() const
{
	int32 TotalActorCount = 0;
	for( int32 LevelIndex=0; LevelIndex<GetNumLevels(); LevelIndex++ )
	{
		ULevel* Level = GetLevel(LevelIndex);
		TotalActorCount += Level->Actors.Num();
	}
	return TotalActorCount;
}

FConstLevelIterator	UWorld::GetLevelIterator() const
{
	return ToRawPtrTArrayUnsafe(Levels).CreateConstIterator();
}

ULevel* UWorld::GetLevel( int32 InLevelIndex ) const
{
	check(InLevelIndex < Levels.Num());
	check(Levels[InLevelIndex]);
	return Levels[ InLevelIndex ];
}

bool UWorld::ContainsLevel( ULevel* InLevel ) const
{
	return Levels.Find( InLevel ) != INDEX_NONE;
}

int32 UWorld::GetNumLevels() const
{
	return Levels.Num();
}

const TArray<class ULevel*>& UWorld::GetLevels() const
{
	return Levels;
}

bool UWorld::AddLevel( ULevel* InLevel )
{
	bool bAddedLevel = false;
	if(ensure(InLevel))
	{
		bAddedLevel = true;
		Levels.AddUnique( InLevel );
		BroadcastLevelsChanged();
	}
	return bAddedLevel;
}

bool UWorld::RemoveLevel( ULevel* InLevel )
{
	bool bRemovedLevel = false;
	if(ContainsLevel( InLevel ) == true )
	{
		bRemovedLevel = true;
		
#if WITH_EDITOR
		if( IsLevelSelected( InLevel ))
		{
			DeSelectLevel( InLevel );
		}
#endif //WITH_EDITOR
		Levels.Remove( InLevel );
		BroadcastLevelsChanged();
	}
	return bRemovedLevel;
}


FString UWorld::GetLocalURL() const
{
	return URL.ToString();
}

/** Returns whether script is executing within the editor. */
bool UWorld::IsPlayInEditor() const
{
	return WorldType == EWorldType::PIE;
}

bool UWorld::IsPlayInPreview() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("PIEVIACONSOLE"));
}


bool UWorld::IsPlayInMobilePreview() const
{
#if WITH_EDITOR
	if (FPIEPreviewDeviceModule::IsRequestingPreviewDevice()
		|| FParse::Param(FCommandLine::Get(), TEXT("featureleveles31")))
	{
		return true;
	}
#endif // WITH_EDITOR
	return FParse::Param(FCommandLine::Get(), TEXT("simmobile")) && !IsPlayInVulkanPreview();
}

bool UWorld::IsPlayInVulkanPreview() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
}

bool UWorld::IsGameWorld() const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview || WorldType == EWorldType::GameRPC;
}

bool UWorld::IsEditorWorld() const
{
	return WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview || WorldType == EWorldType::PIE;
}

bool UWorld::IsPreviewWorld() const
{
	return WorldType == EWorldType::EditorPreview || WorldType == EWorldType::GamePreview;
}

bool UWorld::UsesGameHiddenFlags() const
{
	return IsGameWorld();
}

FString UWorld::GetAddressURL() const
{
	return FString::Printf( TEXT("%s"), *URL.GetHostPortString() );
}

FString UWorld::RemovePIEPrefix(const FString &Source, int32* OutPIEInstanceID)
{
	// PIE prefix is: UEDPIE_X_MapName (where X is some decimal number)
	const FString LookFor = PLAYWORLD_PACKAGE_PREFIX;
	if (OutPIEInstanceID)
	{
		*OutPIEInstanceID = INDEX_NONE;
	}
	int32 idx = Source.Find(LookFor);
	if (idx >= 0)
	{
		int32 end = idx + LookFor.Len();
		if ((end >= Source.Len()) || (Source[end] != '_'))
		{
			UE_LOG(LogWorld, Warning, TEXT("Looks like World path invalid PIE prefix (expected '_' characeter after PIE prefix): %s"), *Source);
			return Source;
		}
		int32 PIEInstanceIDStartIndex = end + 1;
		for (++end; (end < Source.Len()) && (Source[end] != '_'); ++end)
		{
			if ((Source[end] < '0') || (Source[end] > '9'))
			{
				UE_LOG(LogWorld, Warning, TEXT("Looks like World have invalid PIE prefix (PIE instance not number): %s"), *Source);
				return Source;
			}
		}
		if (end >= Source.Len())
		{
			UE_LOG(LogWorld, Warning, TEXT("Looks like World path invalid PIE prefix (can't find end of PIE prefix): %s"), *Source);
			return Source;
		}
		if (OutPIEInstanceID && (end > PIEInstanceIDStartIndex))
		{
			const int32 PIEInstanceIDCount = end - PIEInstanceIDStartIndex;
			const FString PIEInstanceIDStr = Source.Mid(PIEInstanceIDStartIndex, PIEInstanceIDCount);
			TTypeFromString<int32>::FromString(*OutPIEInstanceID, *PIEInstanceIDStr);
		}
		const FString Prefix = Source.Left(idx);
		const FString Suffix = Source.Right(Source.Len() - end - 1);
		return Prefix + Suffix;
	}

	return Source;
}

UWorld* UWorld::FindWorldInPackage(UPackage* Package)
{
	UWorld* Result = nullptr;
	ForEachObjectWithPackage(Package, [&Result](UObject* Object)
	{
		Result = Cast<UWorld>(Object);
		return !Result;
	}, false);
	return Result;
}

bool UWorld::IsWorldOrWorldExternalPackage(UPackage* Package)
{
	bool bResult = false;
	ForEachObjectWithPackage(Package, [&bResult](UObject* Object)
	{
		bResult = !!Cast<UWorld>(Object) || !!Object->GetTypedOuter<UWorld>();
		return !bResult;
	}, false);
	return bResult;
}

UWorld* UWorld::FollowWorldRedirectorInPackage(UPackage* Package, UObjectRedirector** OptionalOutRedirector)
{
	UWorld* RetVal = nullptr;
	TArray<UObject*> PotentialRedirectors;
	GetObjectsWithPackage(Package, PotentialRedirectors, false);
	for (auto ObjIt = PotentialRedirectors.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UObjectRedirector* Redirector = Cast<UObjectRedirector>(*ObjIt);
		if (Redirector)
		{
			RetVal = Cast<UWorld>(Redirector->DestinationObject);
			if ( RetVal )
			{
				// Patch up the WorldType if found in the PreLoad map
				EWorldType::Type* PreLoadWorldType = UWorld::WorldTypePreLoadMap.Find(Redirector->GetOuter()->GetFName());
				if (PreLoadWorldType)
				{
					RetVal->WorldType = *PreLoadWorldType;
				}

				if ( OptionalOutRedirector )
				{
					(*OptionalOutRedirector) = Redirector;
				}
				break;
			}
		}
	}

	return RetVal;
}

#if WITH_EDITOR


void UWorld::BroadcastSelectedLevelsChanged() 
{ 
	if( bBroadcastSelectionChange )
	{
		SelectedLevelsChangedEvent.Broadcast(); 
	}
}


void UWorld::SelectLevel( ULevel* InLevel )
{
	check( InLevel );
	if( IsLevelSelected( InLevel ) == false )
	{
		SelectedLevels.AddUnique( InLevel );
		BroadcastSelectedLevelsChanged();
	}
}

void UWorld::DeSelectLevel( ULevel* InLevel )
{
	check( InLevel );
	if( IsLevelSelected( InLevel ) == true )
	{
		SelectedLevels.Remove( InLevel );
		BroadcastSelectedLevelsChanged();
	}
}

bool UWorld::IsLevelSelected( ULevel* InLevel ) const
{
	return SelectedLevels.Find( InLevel ) != INDEX_NONE;
}

int32 UWorld::GetNumSelectedLevels() const 
{
	return SelectedLevels.Num();
}

TArray<TObjectPtr<class ULevel>>& UWorld::GetSelectedLevels()
{
	return SelectedLevels;
}

ULevel* UWorld::GetSelectedLevel( int32 InLevelIndex ) const
{
	check(SelectedLevels[ InLevelIndex ]);
	return SelectedLevels[ InLevelIndex ];
}

void UWorld::SetSelectedLevels( const TArray<class ULevel*>& InLevels )
{
	// Disable the broadcasting of selection changes - we will send a single broadcast when we have finished
	bBroadcastSelectionChange = false;
	SelectedLevels.Empty();
	for (int32 iSelected = 0; iSelected <  InLevels.Num(); iSelected++)
	{
		SelectLevel( InLevels[ iSelected ]);
	}
	// Enable the broadcasting of selection changes
	bBroadcastSelectionChange = true;
	// Broadcast we have change the selections
	BroadcastSelectedLevelsChanged();
}

FDelegateHandle UWorld::AddOnFeatureLevelChangedHandler(const FOnFeatureLevelChanged::FDelegate& InHandler)
{
	return OnFeatureLevelChanged.Add(InHandler);
}

void UWorld::RemoveOnFeatureLevelChangedHandler(FDelegateHandle InHandle)
{
	OnFeatureLevelChanged.Remove(InHandle);
}

#endif // WITH_EDITOR

/**
 * Jumps the server to new level.  If bAbsolute is true and we are using seamless traveling, we
 * will do an absolute travel (URL will be flushed).
 *
 * @param URL the URL that we are traveling to
 * @param bAbsolute whether we are using relative or absolute travel
 * @param bShouldSkipGameNotify whether to notify the clients/game or not
 */
bool UWorld::ServerTravel(const FString& FURL, bool bAbsolute, bool bShouldSkipGameNotify)
{
	AGameModeBase* GameMode = GetAuthGameMode();
	
	if (GameMode != nullptr && !GameMode->CanServerTravel(FURL, bAbsolute))
	{
		return false;
	}

	// Set the next travel type to use
	NextTravelType = bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative;

	// if we're not already in a level change, start one now
	// If the bShouldSkipGameNotify is there, then don't worry about seamless travel recursion
	// and accept that we really want to travel
	if (NextURL.IsEmpty() && (!IsInSeamlessTravel() || bShouldSkipGameNotify))
	{
		NextURL = FURL;
		if (GameMode != NULL)
		{
			// Skip notifying clients if requested
			if (!bShouldSkipGameNotify)
			{
				GameMode->ProcessServerTravel(FURL, bAbsolute);
			}
		}
		else
		{
			NextSwitchCountdown = 0;
		}
	}

	return true;
}

void UWorld::SetNavigationSystem(UNavigationSystemBase* InNavigationSystem)
{
	if (NavigationSystem != NULL && NavigationSystem != InNavigationSystem)
	{
		NavigationSystem->CleanUp(FNavigationSystem::ECleanupMode::CleanupWithWorld);
	}

	NavigationSystem = InNavigationSystem;
}

#if WITH_EDITORONLY_DATA
/** Set the CurrentLevel for this world. **/
bool UWorld::SetCurrentLevel( class ULevel* InLevel )
{
	bool bChanged = false;
	if( CurrentLevel != InLevel )
	{
		ULevel* OldLevel = CurrentLevel;
		CurrentLevel = InLevel;
		bChanged = true;

		FWorldDelegates::OnCurrentLevelChanged.Broadcast(CurrentLevel, OldLevel, this);
	}
	return bChanged;
}
#endif

/** Get the CurrentLevel for this world. **/
ULevel* UWorld::GetCurrentLevel() const
{
#if WITH_EDITORONLY_DATA
	return CurrentLevel;
#else
	return PersistentLevel;
#endif
}

ENetMode UWorld::InternalGetNetMode() const
{
	if (NetDriver != nullptr)
	{
		const bool bIsClientOnly = IsRunningClientOnly();
		return bIsClientOnly ? NM_Client : NetDriver->GetNetMode();
	}

	// Use replay driver's net mode if we're in playback or ticking recording
	if (DemoNetDriver && (DemoNetDriver->IsPlaying() || (DemoNetDriver->IsRecording() && DemoNetDriver->IsInTick())))
	{
		return DemoNetDriver->GetNetMode();
	}

	ENetMode URLNetMode = AttemptDeriveFromURL();
#if WITH_EDITOR
	if (WorldType == EWorldType::PIE && (URLNetMode == NM_Standalone || PlayInEditorNetMode == NM_DedicatedServer))
	{
		// If we're early in startup before the net driver exists and there is no URL override
		// or this is a dedicated server, use the mode we were first created with
		// This is required for dedicated server/listen worlds so it is correct for InitWorld
		return PlayInEditorNetMode;
	}
#endif
	return URLNetMode;
}

bool UWorld::IsRecordingClientReplay() const
{
	if (GetNetDriver() != nullptr && !GetNetDriver()->IsServer())
	{
		if (DemoNetDriver != nullptr && DemoNetDriver->IsServer())
		{
			return true;
		}
	}

	return false;
}

bool UWorld::IsPlayingClientReplay() const
{
	return (DemoNetDriver != nullptr && DemoNetDriver->IsPlayingClientReplay());
}

ENetMode UWorld::AttemptDeriveFromURL() const
{
	if (GEngine != nullptr)
	{
		FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(this);

		if (WorldContext != nullptr)
		{
			// NetMode can be derived from the NextURL if it exists
			if (NextURL.Len() > 0)
			{
				FURL NextLevelURL(&WorldContext->LastURL, *NextURL, NextTravelType);

				if (NextLevelURL.Valid)
				{
					if (NextLevelURL.HasOption(TEXT("listen")))
					{
						return NM_ListenServer;
					}
					else if (NextLevelURL.Host.Len() > 0)
					{
						return NM_Client;
					}
				}
			}
			// NetMode can be derived from the PendingNetURL if it exists
			else if (WorldContext->PendingNetGame != nullptr && WorldContext->PendingNetGame->URL.Valid)
			{
				if (WorldContext->PendingNetGame->URL.HasOption(TEXT("listen")))
				{
					return NM_ListenServer;
				}
				else if (WorldContext->PendingNetGame->URL.Host.Len() > 0)
				{
					return NM_Client;
				}
			}
		}
	}

	return NM_Standalone;
}

void UWorld::SetGameState(AGameStateBase* NewGameState)
{
	if (NewGameState == GameState)
	{
		return;
	}

	GameState = NewGameState;

	// Set the GameState on the LevelCollection it's associated with.
	if (NewGameState != nullptr)
	{
	    const ULevel* const CachedLevel = NewGameState->GetLevel();
		if(CachedLevel != nullptr)
		{
	        FLevelCollection* const FoundCollection = CachedLevel->GetCachedLevelCollection();
	        if (FoundCollection)
	        {
		        FoundCollection->SetGameState(NewGameState);
        
		        // For now the static levels use the same GameState as the source dynamic levels.
		        if (FoundCollection->GetType() == ELevelCollectionType::DynamicSourceLevels)
		        {
			        FLevelCollection& StaticLevels = FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
			        StaticLevels.SetGameState(NewGameState);
		        }
	        }
		}
	}

	GameStateSetEvent.Broadcast(GameState);
}

void UWorld::CopyGameState(AGameModeBase* FromGameMode, AGameStateBase* FromGameState)
{
	AuthorityGameMode = FromGameMode;
	SetGameState(FromGameState);
}

void UWorld::GetLightMapsAndShadowMaps(ULevel* Level, TArray<UTexture2D*>& OutLightMapsAndShadowMaps, bool bForceLazyLoad /*= true*/)
{
	class FFindLightmapsArchive : public FArchiveUObject
	{
		/** The array of textures discovered */
		TArray<UTexture2D*>& TextureList;
		bool bForceLazyLoad;

	public:
		FFindLightmapsArchive(UObject* InSearch, TArray<UTexture2D*>& OutTextureList, bool bInForceLazyLoad)
			: TextureList(OutTextureList)
			, bForceLazyLoad(bInForceLazyLoad)
		{
			ArIsObjectReferenceCollector = true;
			ArIsModifyingWeakAndStrongReferences = true; // While we are not modifying them, we want to follow weak references as well

			// Don't bother searching through the object's references if there's no objects of the types we're looking for
			TArray<UObject*> Objects;
			GetObjectsOfClass(ULightMapTexture2D::StaticClass(), Objects);
			GetObjectsOfClass(UShadowMapTexture2D::StaticClass(), Objects);
			GetObjectsOfClass(ULightMapVirtualTexture2D::StaticClass(), Objects);

			if (Objects.Num())
			{
				for (FThreadSafeObjectIterator It; It; ++It)
				{
					It->Mark(OBJECTMARK_TagExp);
				}

				*this << InSearch;
			}
		}

		FArchive& operator<<(class UObject*& Obj)
		{
			// Don't check null references or objects already visited. Also, skip UWorlds as they will pull in more levels than desired
			// Also skip StaticMesh as it will cause stalls during async compilation and they do not contain any lightmaps anyway.
			if (Obj != NULL && Obj->HasAnyMarks(OBJECTMARK_TagExp) && !Obj->IsA<UWorld>() && !Obj->IsA<UStaticMesh>())
			{
				if (Obj->IsA<ULightMapTexture2D>() ||
					Obj->IsA<UShadowMapTexture2D>() ||
					Obj->IsA<ULightMapVirtualTexture2D>())
				{
					UTexture2D* Tex = Cast<UTexture2D>(Obj);
					if ( ensure(Tex) )
					{
						TextureList.Add(Tex);
					}
				}

				Obj->UnMark(OBJECTMARK_TagExp);
				Obj->Serialize(*this);
			}

			return *this;
		}

		FArchive& operator<<(FObjectPtr& Obj)
		{
			// @TODO: OBJPTR: Could some or all of this behavior be generalized for use in other reference collectors?
			//			Could add another Ar* flag to control whether lazy loads get resolved.  Could add a derivative
			//			of FArchiveUObject that filters references by type.

			// Don't check null references or objects already visited. Also, skip UWorlds as they will pull in more levels than desired
			// Also skip StaticMesh as it will cause stalls during async compilation and they do not contain any lightmaps anyway.
			if (Obj && !Obj.IsA<UWorld>() && !Obj.IsA<UStaticMesh>())
			{
				if (Obj.IsA<ULightMapTexture2D>() ||
					Obj.IsA<UShadowMapTexture2D>() ||
					Obj.IsA<ULightMapVirtualTexture2D>())
				{
					UTexture2D* Tex = Cast<UTexture2D>(Obj.Get());
					if (ensure(Tex))
					{
						TextureList.Add(Tex);
					}
				}
				else if (IsObjectHandleResolved(Obj.GetHandle()) || bForceLazyLoad)
				{
					return FArchiveUObject::operator<<(Obj);
				}
			}
			return *this;
		}
	};

	UObject* SearchObject = Level;
	if ( !SearchObject )
	{
		SearchObject = PersistentLevel;
	}

	FFindLightmapsArchive FindArchive(SearchObject, OutLightMapsAndShadowMaps, bForceLazyLoad);
}

void UWorld::CreateFXSystem()
{
	if ( !IsRunningDedicatedServer() && !IsRunningCommandlet() )
	{
		FXSystem = FFXSystemInterface::Create(GetFeatureLevel(), Scene);
	}
	else
	{
		FXSystem = NULL;
		Scene->SetFXSystem(NULL);
	}
}

FLevelCollection& UWorld::FindOrAddCollectionByType(const ELevelCollectionType InType)
{
	for (FLevelCollection& LC : LevelCollections)
	{
		if (LC.GetType() == InType)
		{
			return LC;
		}
	}

	// Not found, add a new one.
	FLevelCollection NewLC;
	NewLC.SetType(InType);
	LevelCollections.Add(MoveTemp(NewLC));
	return LevelCollections.Last();
}

int32 UWorld::FindOrAddCollectionByType_Index(const ELevelCollectionType InType)
{
	const int32 FoundIndex = FindCollectionIndexByType(InType);

	if (FoundIndex != INDEX_NONE)
	{
		return FoundIndex;
	}

	// Not found, add a new one.
	FLevelCollection NewLC;
	NewLC.SetType(InType);
	return LevelCollections.Add(MoveTemp(NewLC));
}

FLevelCollection* UWorld::FindCollectionByType(const ELevelCollectionType InType)
{
	for (FLevelCollection& LC : LevelCollections)
	{
		if (LC.GetType() == InType)
		{
			return &LC;
		}
	}

	return nullptr;
}

const FLevelCollection* UWorld::FindCollectionByType(const ELevelCollectionType InType) const
{
	for (const FLevelCollection& LC : LevelCollections)
	{
		if (LC.GetType() == InType)
		{
			return &LC;
		}
	}

	return nullptr;
}

int32 UWorld::FindCollectionIndexByType(const ELevelCollectionType InType) const
{
	return LevelCollections.IndexOfByPredicate([InType](const FLevelCollection& Collection)
	{
		return Collection.GetType() == InType;
	});
}

const FLevelCollection* UWorld::GetActiveLevelCollection() const
{
	if (LevelCollections.IsValidIndex(ActiveLevelCollectionIndex))
	{
		return &LevelCollections[ActiveLevelCollectionIndex];
	}

	return nullptr;
}

void UWorld::SetActiveLevelCollection(int32 LevelCollectionIndex)
{
	ActiveLevelCollectionIndex = LevelCollectionIndex;
	const FLevelCollection* const ActiveLevelCollection = GetActiveLevelCollection();

	if (ActiveLevelCollection == nullptr)
	{
		return;
	}

	PersistentLevel = ActiveLevelCollection->GetPersistentLevel();
#if WITH_EDITORONLY_DATA
	if (IsGameWorld())
	{
		SetCurrentLevel(ActiveLevelCollection->GetPersistentLevel());
	}
#endif
	GameState = ActiveLevelCollection->GetGameState();
	NetDriver = ActiveLevelCollection->GetNetDriver();
	DemoNetDriver = ActiveLevelCollection->GetDemoNetDriver();

	// TODO: START TEMP FIX FOR UE-42508
	if (NetDriver && NetDriver->NetDriverName != NAME_None)
	{
		UNetDriver* TempNetDriver = GEngine->FindNamedNetDriver(this, NetDriver->NetDriverName);
		if (TempNetDriver != NetDriver)
		{
			UE_LOG(LogWorld, Warning, TEXT("SetActiveLevelCollection attempted to use an out of date NetDriver: %s"), *(NetDriver->NetDriverName.ToString()));
			NetDriver = TempNetDriver;
		}
	}

	if (DemoNetDriver && DemoNetDriver->NetDriverName != NAME_None)
	{
		UDemoNetDriver* TempDemoNetDriver = Cast<UDemoNetDriver>(GEngine->FindNamedNetDriver(this, DemoNetDriver->NetDriverName));
		if (TempDemoNetDriver != DemoNetDriver)
		{
			UE_LOG(LogWorld, Warning, TEXT("SetActiveLevelCollection attempted to use an out of date DemoNetDriver: %s"), *(DemoNetDriver->NetDriverName.ToString()));
			DemoNetDriver = TempDemoNetDriver;
		}
	}
	// TODO: END TEMP FIX FOR UE-42508
}

static ULevel* DuplicateLevelWithPrefix(ULevel* InLevel, int32 InstanceID )
{
	if (!InLevel || !InLevel->GetOutermost())
	{
		return nullptr;
	}

	const double DuplicateStart = FPlatformTime::Seconds();

	UWorld* OriginalOwningWorld = CastChecked<UWorld>(InLevel->GetOuter());
	UPackage* OriginalPackage = InLevel->GetOutermost();

	const FString OriginalPackageName = OriginalPackage->GetName();

	// Use a PIE prefix for the new package
	const FString PrefixedPackageName = UWorld::ConvertToPIEPackageName( OriginalPackageName, InstanceID );

	// Create a package for duplicated level
	UPackage* NewPackage = CreatePackage( *PrefixedPackageName );
	NewPackage->SetPackageFlags( PKG_PlayInEditor );
	NewPackage->SetPIEInstanceID(InstanceID);
	NewPackage->SetLoadedPath(OriginalPackage->GetLoadedPath());
#if WITH_EDITORONLY_DATA
	NewPackage->SetSavedHash( OriginalPackage->GetSavedHash() );
#endif
	NewPackage->MarkAsFullyLoaded();

	FSoftObjectPath::AddPIEPackageName(NewPackage->GetFName());

	FTemporaryPlayInEditorIDOverride IDHelper(InstanceID);

	// Create "vestigial" world for the persistent level - it's OwningWorld will still be the main world,
	// but we're treating it like a streaming level (even though it's a duplicate of the persistent level).
	UWorld* NewWorld = NewObject<UWorld>(NewPackage, OriginalOwningWorld->GetFName());
	NewWorld->SetFlags(RF_Transactional);
	NewWorld->WorldType = EWorldType::Game;
	NewWorld->SetFeatureLevel(ERHIFeatureLevel::Num);

	ULevel::StreamedLevelsOwningWorld.Add(NewPackage->GetFName(), OriginalOwningWorld);

	FObjectDuplicationParameters Parameters( InLevel, NewWorld );
		
	Parameters.DestName			= InLevel->GetFName();
	Parameters.DestClass		= InLevel->GetClass();
	Parameters.PortFlags		= PPF_DuplicateForPIE;
	Parameters.DuplicateMode	= EDuplicateMode::PIE;

	ULevel* NewLevel = CastChecked<ULevel>( StaticDuplicateObjectEx( Parameters ) );

	ULevel::StreamedLevelsOwningWorld.Remove(NewPackage->GetFName());

	// Fixup model components. The index buffers have been created for the components in the source world and the order
	// in which components were post-loaded matters. So don't try to guarantee a particular order here, just copy the
	// elements over.
	if ( NewLevel->Model != NULL
			&& NewLevel->Model == InLevel->Model
			&& NewLevel->ModelComponents.Num() == InLevel->ModelComponents.Num() )
	{
		NewLevel->Model->ClearLocalMaterialIndexBuffersData();
		for ( int32 ComponentIndex = 0; ComponentIndex < NewLevel->ModelComponents.Num(); ++ComponentIndex )
		{
			UModelComponent* SrcComponent = InLevel->ModelComponents[ComponentIndex];
			UModelComponent* DestComponent = NewLevel->ModelComponents[ComponentIndex];
			DestComponent->CopyElementsFrom( SrcComponent );
		}
	}

	const double DuplicateEnd = FPlatformTime::Seconds();
	const double TotalSeconds = ( DuplicateEnd - DuplicateStart );

	UE_LOG( LogNet, Log, TEXT( "DuplicateLevelWithPrefix. TotalSeconds: %2.2f" ), TotalSeconds );

	return NewLevel;
}

void UWorld::DuplicateRequestedLevels(const FName MapName)
{
	if (GEngine->Experimental_ShouldPreDuplicateMap(MapName))
	{
		if (!IsPartitionedWorld())
		{
			// Duplicate the persistent level and only dynamic levels, but don't add them to the world.
			FLevelCollection DuplicateLevels;
			DuplicateLevels.SetType(ELevelCollectionType::DynamicDuplicatedLevels);
			DuplicateLevels.SetIsVisible(false);
			ULevel* const DuplicatePersistentLevel = DuplicateLevelWithPrefix(PersistentLevel, 1);
			if (!DuplicatePersistentLevel)
			{
				UE_LOG(LogWorld, Warning, TEXT("UWorld::DuplicateRequestedLevels: failed to duplicate persistent level %s. No duplicate level collection will be created."),
					*GetFullNameSafe(PersistentLevel));
				return;
			}
			// Don't tell the server about this level
			DuplicatePersistentLevel->bClientOnlyVisible = true;
			DuplicateLevels.SetPersistentLevel(DuplicatePersistentLevel);
			DuplicateLevels.AddLevel(DuplicatePersistentLevel);

			for (ULevelStreaming* StreamingLevel : StreamingLevels)
			{
				if (StreamingLevel && !StreamingLevel->bIsStatic)
				{
					ULevel* DuplicatedLevel = DuplicateLevelWithPrefix(StreamingLevel->GetLoadedLevel(), 1);
					if (!DuplicatedLevel)
					{
						UE_LOG(LogWorld, Warning, TEXT("UWorld::DuplicateRequestedLevels: failed to duplicate streaming level %s. No duplicate level collection will be created."),
							*GetFullNameSafe(StreamingLevel->GetLoadedLevel()));
						return;
					}
					// Don't tell the server about these levels
					DuplicatedLevel->bClientOnlyVisible = true;
					DuplicateLevels.AddLevel(DuplicatedLevel);
				}
			}

			LevelCollections.Add(MoveTemp(DuplicateLevels));
		}
		else
		{
			UE_LOG(LogWorld, Error, TEXT("UWorld::DuplicateRequestedLevels: Attempted to duplicate streaming levels for partitioned world. This is not a supported operation."));
		}	
	}
}

#if WITH_EDITOR
void UWorld::ChangeFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel, bool bShowSlowProgressDialog, bool bForceUpdate)
{
	if (InFeatureLevel != GetFeatureLevel() || bForceUpdate)
	{
		UE_LOG(LogWorld, Log, TEXT("Changing Feature Level (Enum) from %i to %i%s"), (int)GetFeatureLevel(), (int)InFeatureLevel, bForceUpdate ? TEXT(" (forced)") : TEXT(""));
		FScopedSlowTask SlowTask(100.f, NSLOCTEXT("Engine", "ChangingPreviewRenderingLevelMessage", "Changing Preview Rendering Level"), bShowSlowProgressDialog);
		SlowTask.MakeDialog();
		{
			SlowTask.EnterProgressFrame(10.0f);
			// Give all scene components the opportunity to prepare for pending feature level change.
			for (TObjectIterator<USceneComponent> It; It; ++It)
			{
				USceneComponent* SceneComponent = *It;
				if (SceneComponent->GetWorld() == this)
				{
					SceneComponent->PreFeatureLevelChange(InFeatureLevel);
				}
			}

			SlowTask.EnterProgressFrame(10.0f);
			FGlobalComponentReregisterContext RecreateComponents;
			// Finish any deferred / async render cleanup work.
			GetRendererModule().PerFrameCleanupIfSkipRenderer();
			FlushRenderingCommands();

			SetFeatureLevel(InFeatureLevel);

			SlowTask.EnterProgressFrame(10.0f);
			RecreateScene(InFeatureLevel);

			InvalidateAllSkyCaptures();

			OnFeatureLevelChanged.Broadcast(GetFeatureLevel());

			SlowTask.EnterProgressFrame(10.0f);
			TriggerStreamingDataRebuild();
		}
	}
}

void UWorld::ShaderPlatformChanged()
{
	if (Scene)
	{
		Scene->UpdateEarlyZPassMode();
	}
}

void UWorld::RecreateScene(ERHIFeatureLevel::Type InFeatureLevel, bool bBroadcastChange)
{
	if (Scene)
	{
		ensure(InFeatureLevel == GetFeatureLevel());
		for (ULevel* Level : Levels)
		{
			Level->ReleaseRenderingResources();
		}

		Scene->Release();
		IRendererModule& RendererModule = GetRendererModule();
		RendererModule.RemoveScene(Scene);

		if (bBroadcastChange)
		{
			FRenderResource::ChangeFeatureLevel(InFeatureLevel);
		}

		RendererModule.AllocateScene(this, bRequiresHitProxies, FXSystem != nullptr, InFeatureLevel);

		for (ULevel* Level : Levels)
		{
			Level->InitializeRenderingResources();
			Level->PrecomputedVisibilityHandler.UpdateScene(Scene);
			Level->PrecomputedVolumeDistanceField.UpdateScene(Scene);
		}
	}
}

// Recreate the editor world's FScene with a null scene interface to drop extra GPU memory during PIE
void UWorld::PurgeScene()
{
	if (CVarPurgeEditorSceneDuringPIE.GetValueOnGameThread() == 0)
	{
		return;
	}

	if (!bPurgedScene && this->IsEditorWorld())
	{
		// Clear out Slate's active scenes list since these ptrs no longer reference valid scenes.
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().FlushRenderState();
		}

		FGlobalComponentReregisterContext RecreateComponents;

		// Finish any deferred / async render cleanup work.
		GetRendererModule().PerFrameCleanupIfSkipRenderer();
		FlushRenderingCommands();

		const bool bOldVal = GUsingNullRHI;
		GUsingNullRHI = true;
		{
			RecreateScene(GetFeatureLevel(), false /* bBroadcastChange */);
		}
		GUsingNullRHI = bOldVal;

		InvalidateAllSkyCaptures();
		TriggerStreamingDataRebuild();

		bPurgedScene = true;
	}
}

// Restore the purged editor world FScene back to the proper GPU representation
void UWorld::RestoreScene()
{
	if (bPurgedScene)
	{
		FGlobalComponentReregisterContext RecreateComponents;

		// Finish any deferred / async render cleanup work.
		GetRendererModule().PerFrameCleanupIfSkipRenderer();
		FlushRenderingCommands();

		RecreateScene(GetFeatureLevel(), false /* bBroadcastChange */);

		InvalidateAllSkyCaptures();
		TriggerStreamingDataRebuild();

		bPurgedScene = false;
	}
}

void UWorld::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UWorld::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	if (PersistentLevel && PersistentLevel->OwningWorld)
	{
		if (ULevelScriptBlueprint* Blueprint = PersistentLevel->GetLevelScriptBlueprint(true))
		{
			Blueprint->GetAssetRegistryTags(Context);
		}
		// If there are no blueprints FiBData will be empty, the search manager will treat this as indexed
								
		if (UWorldPartition* WorldPartition = GetWorldPartition())
		{
			WorldPartition->AppendAssetRegistryTags(Context);
		}
		else
		{
			FBox LevelBounds;
			if (PersistentLevel->LevelBoundsActor.IsValid())
			{
				LevelBounds = PersistentLevel->LevelBoundsActor.Get()->GetComponentsBoundingBox();
			}
			else
			{
				LevelBounds = ALevelBounds::CalculateLevelBounds(PersistentLevel);
			}

			FVector LevelBoundsLocation;
			FVector LevelBoundsExtent;
			LevelBounds.GetCenterAndExtents(LevelBoundsLocation, LevelBoundsExtent);

			static const FName NAME_LevelBoundsLocation(TEXT("LevelBoundsLocation"));
			Context.AddTag(FAssetRegistryTag(NAME_LevelBoundsLocation, LevelBoundsLocation.ToCompactString(), FAssetRegistryTag::TT_Hidden));

			static const FName NAME_LevelBoundsExtent(TEXT("LevelBoundsExtent"));
			Context.AddTag(FAssetRegistryTag(NAME_LevelBoundsExtent, LevelBoundsExtent.ToCompactString(), FAssetRegistryTag::TT_Hidden));
		}
	
		if (PersistentLevel->IsUsingExternalActors())
		{
			static const FName NAME_LevelIsUsingExternalActors(TEXT("LevelIsUsingExternalActors"));
			Context.AddTag(FAssetRegistryTag(NAME_LevelIsUsingExternalActors, TEXT("1"), FAssetRegistryTag::TT_Hidden));
		}

		if (PersistentLevel->IsUsingActorFolders())
		{
			static const FName NAME_LevelIsUsingActorFolders(TEXT("LevelIsUsingActorFolders"));
			Context.AddTag(FAssetRegistryTag(NAME_LevelIsUsingActorFolders, TEXT("1"), FAssetRegistryTag::TT_Hidden));
		}

		if (AWorldSettings* WorldSettings = GetWorldSettings(/*bCheckStreamingPersistent*/false, /*bChecked*/false))
		{
			FVector LevelInstancePivotOffset = WorldSettings ? WorldSettings->LevelInstancePivotOffset : FVector::ZeroVector;
			if (!LevelInstancePivotOffset.IsNearlyZero())
			{
				static const FName NAME_LevelInstancePivotOffset(TEXT("LevelInstancePivotOffset"));
				Context.AddTag(FAssetRegistryTag(NAME_LevelInstancePivotOffset, LevelInstancePivotOffset.ToCompactString(), FAssetRegistryTag::TT_Hidden));
			}
		}
	}

	// Get the full file path with extension
	FString FullFilePath;
	if (FPackageName::TryConvertLongPackageNameToFilename(GetOutermost()->GetName(), FullFilePath, FPackageName::GetMapPackageExtension()))
	{
		// Save/Display the modify date. File size is handled generically for all packages
		FDateTime AssetDateModified = IFileManager::Get().GetTimeStamp(*FullFilePath);
		Context.AddTag(FAssetRegistryTag("DateModified", AssetDateModified.ToString(), FAssetRegistryTag::TT_Chronological, FAssetRegistryTag::TD_Date));
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	TArray<UObject::FAssetRegistryTag> DeprecatedTags;
	FWorldDelegates::GetAssetTags.Broadcast(this, DeprecatedTags);
	for (UObject::FAssetRegistryTag& Tag : DeprecatedTags)
	{
		Context.AddTag(MoveTemp(Tag));
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
	FWorldDelegates::GetAssetTagsWithContext.Broadcast(this, Context);
}

void UWorld::PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const
{
	Super::PostLoadAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);

	// GetAssetRegistryTags appends the LevelBlueprint tags to the World's tags, so we also have to run the Blueprint PostLoadAssetRegistryTags
	UBlueprint::PostLoadBlueprintAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);
}

bool UWorld::IsNameStableForNetworking() const
{
	return bIsNameStableForNetworking || Super::IsNameStableForNetworking();
}
#endif

bool UWorld::ResolveSubobject(const TCHAR* SubObjectPath, UObject*& OutObject, bool bLoadIfExists)
{
	FString SubObjectName;
	FString SubObjectContext;	
	if (FString(SubObjectPath).Split(TEXT("."), &SubObjectContext, &SubObjectName))
	{
		if (UObject* SubObject = StaticFindObject(nullptr, this, *SubObjectContext))
		{
			return SubObject->ResolveSubobject(*SubObjectName, OutObject, bLoadIfExists);
		}
	}

	OutObject = nullptr;
	return false;
}

FPrimaryAssetId UWorld::GetPrimaryAssetId() const
{
	UPackage* Package = GetOutermost();
	const IWorldPartitionCell* WorldPartitionCell = PersistentLevel ? PersistentLevel->GetWorldPartitionRuntimeCell() : nullptr;

	// PIE and world partition runtime levels are temporary and do not represent a primary asset
	if (!Package->HasAnyPackageFlags(PKG_PlayInEditor) && !WorldPartitionCell)
	{
		// Return Map:/path/to/map
		return FPrimaryAssetId(UAssetManager::MapType, Package->GetFName());
	}

	return FPrimaryAssetId();
}

void UWorld::InsertPostProcessVolume(IInterface_PostProcessVolume* InVolume)
{
	const int32 NumVolumes = PostProcessVolumes.Num();
	float TargetPriority = InVolume->GetProperties().Priority;
	int32 InsertIndex = 0;
	// TODO: replace with binary search.
	for (; InsertIndex < NumVolumes; InsertIndex++)
	{
		IInterface_PostProcessVolume* CurrentVolume = PostProcessVolumes[InsertIndex];
		float CurrentPriority = CurrentVolume->GetProperties().Priority;

		if (TargetPriority < CurrentPriority)
		{
			break;
		}
		if (CurrentVolume == InVolume)
		{
			return;
		}
	}
	PostProcessVolumes.Insert(InVolume, InsertIndex);
}

void UWorld::RemovePostProcessVolume(IInterface_PostProcessVolume* InVolume)
{
	PostProcessVolumes.RemoveSingle(InVolume);
}

void UWorld::InitializeSubsystems()
{
	check(!SubsystemCollection.IsInitialized());
	SubsystemCollection.Initialize(this);
}

void UWorld::PostInitializeSubsystems()
{
	check(bIsWorldInitialized);

	const TArray<UWorldSubsystem*>& WorldSubsystems = SubsystemCollection.GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());

	for (UWorldSubsystem* WorldSubsystem : WorldSubsystems)
	{
		WorldSubsystem->PostInitialize();
	}
}

static void DoPostProcessVolume(IInterface_PostProcessVolume* Volume, FVector ViewLocation, FSceneView* SceneView)
{
	const FPostProcessVolumeProperties VolumeProperties = Volume->GetProperties();
	if (!VolumeProperties.bIsEnabled)
	{
		return;
	}

	float DistanceToPoint = 0.0f;
	float LocalWeight = FMath::Clamp(VolumeProperties.BlendWeight, 0.0f, 1.0f);

	ensureMsgf((LocalWeight >= 0 && LocalWeight <= 1.0f), TEXT("Invalid post process blend weight retrieved from volume (%f)"), LocalWeight);

	if (!VolumeProperties.bIsUnbound)
	{
		float SquaredBlendRadius = VolumeProperties.BlendRadius * VolumeProperties.BlendRadius;
		Volume->EncompassesPoint(ViewLocation, 0.0f, &DistanceToPoint);

		if (DistanceToPoint >= 0)
		{
			if (DistanceToPoint > VolumeProperties.BlendRadius)
			{
				// outside
				LocalWeight = 0.0f;
			}
			else
			{
				// to avoid div by 0
				if (VolumeProperties.BlendRadius >= 1.0f)
				{
					LocalWeight *= 1.0f - DistanceToPoint / VolumeProperties.BlendRadius;

					if(!(LocalWeight >= 0 && LocalWeight <= 1.0f))
					{
						// Mitigate crash here by disabling this volume and generating info regarding the calculation that went wrong.
						ensureMsgf(false, TEXT("Invalid LocalWeight after post process volume weight calculation (Local: %f, DtP: %f, Radius: %f, SettingsWeight: %f)"), LocalWeight, DistanceToPoint, VolumeProperties.BlendRadius, VolumeProperties.BlendWeight);
						LocalWeight = 0.0f;
					}
				}
			}
		}
		else
		{
			LocalWeight = 0;
		}
	}

#if DEBUG_POST_PROCESS_VOLUME_ENABLE
	if (SceneView->Family && SceneView->Family->EngineShowFlags.VisualizePostProcessStack)
	{
		FPostProcessSettingsDebugInfo& PPDebug = SceneView->FinalPostProcessDebugInfo.AddDefaulted_GetRef();

		PPDebug.Name = Volume->GetDebugName();

		FPostProcessVolumeProperties VProperties = Volume->GetProperties();
		PPDebug.bIsEnabled = VProperties.bIsEnabled;
		PPDebug.bIsUnbound = VProperties.bIsUnbound;
		PPDebug.Priority = VProperties.Priority;
		PPDebug.CurrentBlendWeight = LocalWeight;
	}
#endif

	if (LocalWeight > 0)
	{
		SceneView->OverridePostProcessSettings(*VolumeProperties.Settings, LocalWeight);
	}
}

void UWorld::AddPostProcessingSettings(FVector ViewLocation, FSceneView* SceneView)
{
	OnBeginPostProcessSettings.Broadcast(ViewLocation, SceneView);

#if DEBUG_POST_PROCESS_VOLUME_ENABLE
	SceneView->FinalPostProcessDebugInfo.Reset();
#endif

	for (IInterface_PostProcessVolume* PPVolume : PostProcessVolumes)
	{
		DoPostProcessVolume(PPVolume, ViewLocation, SceneView);
	}
}

void UWorld::SetAudioDevice(const FAudioDeviceHandle& InHandle)
{
	check(IsInGameThread());

	if (InHandle.GetDeviceID() == AudioDeviceHandle.GetDeviceID())
	{
		return;
	}

	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		// Register new world with incoming device first to avoid premature reporting due to no handles being valid...
		if (InHandle.IsValid())
		{
			check(InHandle.GetWorld() == this);
			DeviceManager->RegisterWorld(this, InHandle.GetDeviceID());
		}

		const Audio::FDeviceId OldDeviceId = AudioDeviceHandle.GetDeviceID();
		const bool bUnregister = AudioDeviceHandle.IsValid();

		AudioDeviceHandle = InHandle;

		if (bUnregister)
		{
			DeviceManager->UnregisterWorld(this, OldDeviceId);
		}
	}
	else
	{
		AudioDeviceHandle.Reset();
	}

	if (AudioDeviceHandle.IsValid() && !AudioDeviceDestroyedHandle.IsValid())
	{
		AudioDeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddLambda([this](const Audio::FDeviceId InDeviceId)
		{
			if (InDeviceId == AudioDeviceHandle.GetDeviceID())
			{
				FAudioDeviceHandle EmptyHandle;
				SetAudioDevice(EmptyHandle);
			}
		});
	}
	else if (!AudioDeviceHandle.IsValid() && AudioDeviceDestroyedHandle.IsValid())
	{
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(AudioDeviceDestroyedHandle);
		AudioDeviceDestroyedHandle.Reset();
	}
}

FAudioDeviceHandle UWorld::GetAudioDevice() const
{
	if (AudioDeviceHandle)
	{
		return AudioDeviceHandle;
	}
	else if (GEngine)
	{
		return GEngine->GetMainAudioDevice();
	}
	else
	{
		return FAudioDeviceHandle();
	}
}

FAudioDevice* UWorld::GetAudioDeviceRaw() const
{
	if (AudioDeviceHandle)
	{
		return AudioDeviceHandle.GetAudioDevice();
	}
	else if (GEngine)
	{
		return GEngine->GetMainAudioDeviceRaw();
	}
	else
	{
		return nullptr;
	}
}

/**
* Dump visible actors in current world.
*/
static void DumpVisibleActors(UWorld* InWorld)
{
	UE_LOG(LogWorld, Log, TEXT("------ START DUMP VISIBLE ACTORS ------"));
	for (FActorIterator ActorIterator(InWorld); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (Actor && Actor->WasRecentlyRendered(0.05f))
		{
			UE_LOG(LogWorld, Log, TEXT("Visible Actor : %s"), *Actor->GetFullName());
		}
	}
	UE_LOG(LogWorld, Log, TEXT("------ END DUMP VISIBLE ACTORS ------"));
}

static FAutoConsoleCommandWithWorld DumpVisibleActorsCmd(
	TEXT("DumpVisibleActors"),
	TEXT("Dump visible actors in current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpVisibleActors)
	);

static void DumpLevelCollections(UWorld* InWorld)
{
	if (!InWorld)
	{
		return;
	}

	UE_LOG(LogWorld, Log, TEXT("--- Dumping LevelCollections ---"));

	for(const FLevelCollection& LC : InWorld->GetLevelCollections())
	{
		UE_LOG(LogWorld, Log, TEXT("%d: %d levels."), static_cast<int32>(LC.GetType()), LC.GetLevels().Num());
		UE_LOG(LogWorld, Log, TEXT("  PersistentLevel: %s"), *GetFullNameSafe(LC.GetPersistentLevel()));
		UE_LOG(LogWorld, Log, TEXT("  GameState: %s"), *GetFullNameSafe(LC.GetGameState()));
		UE_LOG(LogWorld, Log, TEXT("  Levels:"));
		for (const ULevel* Level : LC.GetLevels())
		{
			UE_LOG(LogWorld, Log, TEXT("    %s"), *GetFullNameSafe(Level));
		}
	}
}

static FAutoConsoleCommandWithWorld DumpLevelCollectionsCmd(
	TEXT("DumpLevelCollections"),
	TEXT("Dump level collections in the current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpLevelCollections)
	);

#if WITH_EDITOR
FAsyncPreRegisterDDCRequest::~FAsyncPreRegisterDDCRequest()
{
	// Discard any results
	if (Handle != 0)
	{
		WaitAsynchronousCompletion();
		TArray<uint8> Junk;
		GetAsynchronousResults(Junk);
	}
}

bool FAsyncPreRegisterDDCRequest::PollAsynchronousCompletion()
{
	if (Handle != 0)
	{
		return GetDerivedDataCacheRef().PollAsynchronousCompletion(Handle);
	}
	return true;
}

void FAsyncPreRegisterDDCRequest::WaitAsynchronousCompletion()
{
	if (Handle != 0)
	{
		GetDerivedDataCacheRef().WaitAsynchronousCompletion(Handle);
	}
}

bool FAsyncPreRegisterDDCRequest::GetAsynchronousResults(TArray<uint8>& OutData)
{
	check(Handle != 0);
	bool bResult = GetDerivedDataCacheRef().GetAsynchronousResults(Handle, OutData);
	// invalidate request after results received
	Handle = 0;
	DDCKey = TEXT("");
	return bResult;
}
#endif

FString ToString(EWorldType::Type Type)
{
	switch (Type)
	{
	case EWorldType::None: return TEXT("None");
	case EWorldType::Game: return TEXT("Game");
	case EWorldType::Editor: return TEXT("Editor");
	case EWorldType::PIE: return TEXT("PIE");
	case EWorldType::EditorPreview: return TEXT("EditorPreview");
	case EWorldType::GamePreview: return TEXT("GamePreview");
	case EWorldType::GameRPC: return TEXT("GameRPC");
	case EWorldType::Inactive: return TEXT("Inactive");
	default: return TEXT("Unknown");
	}
}

FString ENGINE_API ToString(ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone: return TEXT("Standalone");
	case NM_DedicatedServer:  return TEXT("Dedicated Server");
	case NM_ListenServer: return TEXT("Listen Server");
	case NM_Client: return TEXT("Client");
	default: return TEXT("Invalid");
	}
}

#undef LOCTEXT_NAMESPACE 
