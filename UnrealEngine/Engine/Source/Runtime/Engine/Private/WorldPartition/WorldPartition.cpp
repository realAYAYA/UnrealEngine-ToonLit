// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WorldPartition.cpp: UWorldPartition implementation
=============================================================================*/
#include "WorldPartition/WorldPartition.h"
#include "Misc/PackageName.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "Misc/StringFormatArg.h"
#include "WorldPartition/WorldPartitionLevelStreamingPolicy.h"
#include "WorldPartition/WorldPartitionReplay.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/HLOD/HLODRuntimeSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/ExternalDataLayerManager.h"
#include "WorldPartition/WorldPartitionSettings.h"
#include "GameFramework/WorldSettings.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "LandscapeProxy.h"
#include "Engine/LevelStreaming.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartition)

#if WITH_EDITOR
#include "LevelUtils.h"
#include "Selection.h"
#include "HAL/FileManager.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"
#include "LocationVolume.h"
#include "Engine/LevelScriptBlueprint.h"
#include "ActorReferencesUtils.h"
#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionMiniMap.h"
#include "WorldPartition/WorldPartitionMiniMapHelper.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterPinnedActors.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageContextInterface.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationLogErrorHandler.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationMapCheckErrorHandler.h"
#include "Modules/ModuleManager.h"
#include "GameDelegates.h"
#else
#include "Engine/Level.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "WorldPartition"

namespace WorldPartition
{
#if WITH_EDITOR
	static const EConsoleVariableFlags ECVF_Runtime_ReadOnly = ECVF_Default;
#else
	static const EConsoleVariableFlags ECVF_Runtime_ReadOnly = ECVF_ReadOnly;
#endif
}

#if WITH_EDITOR
int32 UWorldPartition::LoadingRangeBugItGo = 12800;
FAutoConsoleVariableRef UWorldPartition::CVarLoadingRangeBugItGo(
	TEXT("wp.Editor.LoadingRangeBugItGo"),
	UWorldPartition::LoadingRangeBugItGo,
	TEXT("Loading range for BugItGo command."),
	ECVF_Default);

int32 UWorldPartition::WorldExtentToEnableStreaming = 400000;
FAutoConsoleVariableRef UWorldPartition::CVarWorldExtentToEnableStreaming(
	TEXT("wp.Editor.WorldExtentToEnableStreaming"),
	UWorldPartition::WorldExtentToEnableStreaming,
	TEXT("World extend to justify enabling streaming."),
	ECVF_Default);

bool UWorldPartition::DebugDedicatedServerStreaming = false;
FAutoConsoleVariableRef UWorldPartition::CVarDebugDedicatedServerStreaming(
	TEXT("wp.Runtime.DebugDedicatedServerStreaming"),
	UWorldPartition::DebugDedicatedServerStreaming,
	TEXT("Turn on/off to debug of server streaming."),
	ECVF_Default);

int32 UWorldPartition::EnableSimulationStreamingSource = 1;
FAutoConsoleVariableRef UWorldPartition::CVarEnableSimulationStreamingSource(
	TEXT("wp.Runtime.EnableSimulationStreamingSource"),
	UWorldPartition::EnableSimulationStreamingSource,
	TEXT("Set to 0 to if you want to disable the simulation/ejected camera streaming source."),
	ECVF_Default);
#endif

int32 UWorldPartition::GlobalEnableServerStreaming = 0;
FAutoConsoleVariableRef UWorldPartition::CVarEnableServerStreaming(
	TEXT("wp.Runtime.EnableServerStreaming"),
	UWorldPartition::GlobalEnableServerStreaming,
	TEXT("Set to 1 to enable server streaming, set to 2 to only enable it in PIE.\n")
	TEXT("Changing the value while the game is running won't be considered."),
	WorldPartition::ECVF_Runtime_ReadOnly);

bool UWorldPartition::bGlobalEnableServerStreamingOut = false;
FAutoConsoleVariableRef UWorldPartition::CVarEnableServerStreamingOut(
	TEXT("wp.Runtime.EnableServerStreamingOut"),
	UWorldPartition::bGlobalEnableServerStreamingOut,
	TEXT("Turn on/off to allow or not the server to stream out levels (only relevant when server streaming is enabled)\n")
	TEXT("Changing the value while the game is running won't be considered."),
	WorldPartition::ECVF_Runtime_ReadOnly);

bool UWorldPartition::bUseMakingVisibleTransactionRequests = false;
FAutoConsoleVariableRef UWorldPartition::CVarUseMakingVisibleTransactionRequests(
	TEXT("wp.Runtime.UseMakingVisibleTransactionRequests"),
	UWorldPartition::bUseMakingVisibleTransactionRequests,
	TEXT("Whether the client should wait for the server to acknowledge visibility update before making partitioned world streaming levels visible.\n")
	TEXT("Changing the value while the game is running won't be considered."),
	WorldPartition::ECVF_Runtime_ReadOnly);

bool UWorldPartition::bUseMakingInvisibleTransactionRequests = false;
FAutoConsoleVariableRef UWorldPartition::CVarUseMakingInvisibleTransactionRequests(
	TEXT("wp.Runtime.UseMakingInvisibleTransactionRequests"),
	UWorldPartition::bUseMakingInvisibleTransactionRequests,
	TEXT("Whether the client should wait for the server to acknowledge visibility update before making partitioned world streaming levels invisible.\n")
	TEXT("Changing the value while the game is running won't be considered."),
	WorldPartition::ECVF_Runtime_ReadOnly);

#if WITH_EDITOR
TMap<FName, FString> GetDataLayersDumpString(const UWorldPartition* WorldPartition)
{
	TMap<FName, FString> DataLayersDumpString;
	const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager();
	DataLayerManager->ForEachDataLayerInstance([&DataLayersDumpString](const UDataLayerInstance* DataLayerInstance)
	{
		DataLayersDumpString.FindOrAdd(DataLayerInstance->GetDataLayerFName()) = FString::Format(TEXT("{0}{1})"), { DataLayerInstance->GetDataLayerShortName(), DataLayerInstance->GetDataLayerFName().ToString() });
		return true;
	});
	
	return DataLayersDumpString;
}

FString GetActorDescDumpString(const FWorldPartitionActorDescInstance* ActorDescInstance, const TMap<FName, FString>& DataLayersDumpString)
{
	auto GetDataLayerString = [&DataLayersDumpString](const TArray<FName>& DataLayerNames)
	{
		if (DataLayerNames.IsEmpty())
		{
			return FString("None");
		}

		return FString::JoinBy(DataLayerNames, TEXT(", "), [&DataLayersDumpString](const FName& DataLayerName) 
		{ 
			if (const FString* DumpString = DataLayersDumpString.Find(DataLayerName))
			{
				return *DumpString;
			}
			return DataLayerName.ToString(); 
		});
	};

	check(ActorDescInstance);
	return FString::Printf(
		TEXT("%s DataLayerNames:%s") LINE_TERMINATOR, 
		*ActorDescInstance->ToString(FWorldPartitionActorDesc::EToStringMode::Full),
		*GetDataLayerString(ActorDescInstance->GetDataLayerInstanceNames().ToArray())
	);
}

static FAutoConsoleCommand DumpActorDesc(
	TEXT("wp.Editor.DumpActorDesc"),
	TEXT("Dump a specific actor descriptor on the console."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		TArray<FString> ActorPaths;
		if (Args.Num() > 0)
		{
			ActorPaths.Add(Args[0]);
		}
		else
		{
			for (FSelectionIterator SelectionIt(*GEditor->GetSelectedActors()); SelectionIt; ++SelectionIt)
			{
				if (const AActor* Actor = CastChecked<AActor>(*SelectionIt))
				{
					ActorPaths.Add(Actor->GetPathName());
				}
			}
		}

		if (!ActorPaths.IsEmpty())
		{
			if (UWorld* World = GEditor->GetEditorWorldContext().World())
			{
				if (!World->IsGameWorld())
				{
					if (UWorldPartition* WorldPartition = World->GetWorldPartition())
					{
						TMap<FName, FString> DataLayersDumpString = GetDataLayersDumpString(WorldPartition);
						for (const FString& ActorPath : ActorPaths)
						{
							if (const FWorldPartitionActorDescInstance* ActorDescInstance = WorldPartition->GetActorDescInstanceByPath(ActorPath))
							{
								UE_LOG(LogWorldPartition, Log, TEXT("%s"), *GetActorDescDumpString(ActorDescInstance, DataLayersDumpString));
							}
						}
					}
				}
			}
		}
	})
);

static FAutoConsoleCommand DumpActorDescs(
	TEXT("wp.Editor.DumpActorDescs"),
	TEXT("Dump the list of actor descriptors in a CSV file."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() > 0)
		{
			if (UWorld* World = GEditor->GetEditorWorldContext().World())
			{
				if (!World->IsGameWorld())
				{
					if (UWorldPartition* WorldPartition = World->GetWorldPartition())
					{
						WorldPartition->DumpActorDescs(Args[0]);
					}
				}
			}
		}
	})
);

class FLoaderAdapterAlwaysLoadedActors : public FLoaderAdapterShape
{
public:
	FLoaderAdapterAlwaysLoadedActors(UWorld* InWorld)
		: FLoaderAdapterShape(InWorld, FBox(FVector(-HALF_WORLD_MAX, -HALF_WORLD_MAX, -HALF_WORLD_MAX), FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX)), TEXT("Always Loaded"))
	{
		bIncludeSpatiallyLoadedActors = false;
		bIncludeNonSpatiallyLoadedActors = true;
	}

	void RefreshLoadedState()
	{
		FLoaderAdapterShape::RefreshLoadedState();
	}
};

UWorldPartition::FWorldPartitionExternalDirtyActorsTracker::FWorldPartitionExternalDirtyActorsTracker()
	: Super(nullptr, nullptr)
{}

UWorldPartition::FWorldPartitionExternalDirtyActorsTracker::FWorldPartitionExternalDirtyActorsTracker(UWorldPartition* InWorldPartition)
	: Super(InWorldPartition->GetTypedOuter<ULevel>(), InWorldPartition)
{}

void UWorldPartition::FWorldPartitionExternalDirtyActorsTracker::OnRemoveNonDirtyActor(TWeakObjectPtr<AActor> InActor, FWorldPartitionReference& InValue)
{
	check(InActor.IsValid());
	NonDirtyActors.Emplace(InActor, InValue);
}

void UWorldPartition::FWorldPartitionExternalDirtyActorsTracker::Tick(float InDeltaSeconds)
{
	Super::Tick(InDeltaSeconds);

	for (auto& [Actor, Reference] : NonDirtyActors)
	{
		// Resolve reference for newly added actors
		if (!Reference.IsValid() && Actor.IsValid())
		{
			Reference = FWorldPartitionReference(Owner, Actor->GetActorGuid());
		}

		// Transfer ownership of our last ref if actor can be pinned
		if (Reference.IsValid() && Reference->GetHardRefCount() <= 1 && Owner->PinnedActors && FLoaderAdapterPinnedActors::SupportsPinning(*Reference))
		{
			Owner->PinnedActors->AddActors({ Reference.ToHandle() });
		}
	}

	NonDirtyActors.Empty();
}

UWorldPartition::FWorldPartitionChangedEvent UWorldPartition::WorldPartitionChangedEvent;
#endif

#if !NO_LOGGING
static FAutoConsoleCommand SetLogWorldPartitionVerbosity(
	TEXT("wp.Runtime.SetLogWorldPartitionVerbosity"),
	TEXT("Change the WorldPartition log verbosity."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() == 1)
		{
			if (Args[0].Contains(TEXT("Verbose")))
			{
				LogWorldPartition.SetVerbosity(ELogVerbosity::Verbose);
			}
			else
			{
				LogWorldPartition.SetVerbosity(LogWorldPartition.GetCompileTimeVerbosity());
			}
		}
	})
);
#endif

UWorldPartition::UWorldPartition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, EditorHash(nullptr)
	, AlwaysLoadedActors(nullptr)
	, ForceLoadedActors(nullptr)
	, PinnedActors(nullptr)
	, WorldPartitionEditor(nullptr)
	, bStreamingWasEnabled(true)
	, bShouldCheckEnableStreamingWarning(false)
	, bForceGarbageCollection(false)
	, bForceGarbageCollectionPurge(false)
	, bEnablingStreamingJustified(false)
	, bIsPIE(false)
	, NumUserCreatedLoadedRegions(0)
#endif
	, InitState(EWorldPartitionInitState::Uninitialized)
	, bStreamingInEnabled(true)
	, DataLayerManager(nullptr)
	, StreamingPolicy(nullptr)
	, Replay(nullptr)
{
	bEnableStreaming = true;
	ServerStreamingMode = EWorldPartitionServerStreamingMode::ProjectDefault;
	ServerStreamingOutMode = EWorldPartitionServerStreamingOutMode::ProjectDefault;
	DataLayersLogicOperator = EWorldPartitionDataLayersLogicOperator::Or;
	StreamingStateEpoch = 0;

#if WITH_EDITOR
	bAllowShowingHLODsInEditor = true;
	WorldPartitionStreamingPolicyClass = UWorldPartitionLevelStreamingPolicy::StaticClass();
#endif
}

#if WITH_EDITOR
void UWorldPartition::OnGCPostReachabilityAnalysis()
{
	const TIndirectArray<FWorldContext>& WorldContextList = GEngine->GetWorldContexts();

	// Avoid running this process while a game world is live
	for (const FWorldContext& WorldContext : WorldContextList)
	{
		if (WorldContext.World() != nullptr && WorldContext.World()->IsGameWorld())
		{
			return;
		}
	}

	for (FRawObjectIterator It; It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(static_cast<UObject*>(It->Object)))
		{
			if (Actor->IsUnreachable() && !Actor->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) && Actor->IsMainPackageActor())
			{
				ForEachObjectWithPackage(Actor->GetPackage(), [this, Actor](UObject* Object)
				{
					if (Object->HasAnyFlags(RF_Standalone))
					{
						UE_LOG(LogWorldPartition, Log, TEXT("Actor %s is unreachable without properly detaching object %s in its package"), *Actor->GetPathName(), *Object->GetPathName());

						Object->ClearFlags(RF_Standalone);

						// Make sure we trigger a second GC at the next tick to properly destroy packages there were fixed in this pass
						bForceGarbageCollection = true;
						bForceGarbageCollectionPurge = true;
					}
					return true;
				}, false);
			}
		}
	}
}

// Returns whether the memory package is part of the known/valid package names
// used by World Partition for PIE/-game streaming.
bool UWorldPartition::IsValidPackageName(const FString& InPackageName)
{
	if (FPackageName::IsMemoryPackage(InPackageName))
	{
		// Remove PIE prefix
		FString PackageName = UWorld::RemovePIEPrefix(InPackageName);
		// Test if package is a valid world partition PIE package
		return GeneratedLevelStreamingPackageNames.Contains(PackageName);
	}
	return false;
}

void UWorldPartition::OnPreBeginPIE(bool bStartSimulate)
{
	OnBeginPlay();
}

void UWorldPartition::OnPrePIEEnded(bool bWasSimulatingInEditor)
{
	OnEndPlay();
}

void UWorldPartition::OnBeginPlay()
{
	check(!bIsPIE);
	bIsPIE = !IsRunningGame();

	// In PIE, we always want to populate the map check dialog
	FStreamingGenerationMapCheckErrorHandler MapCheckErrorHandler;
	FStreamingGenerationLogErrorHandler LogErrorHandler;
	
	FGenerateStreamingParams Params = FGenerateStreamingParams()
		.SetErrorHandler(bIsPIE ? (IStreamingGenerationErrorHandler*)&MapCheckErrorHandler : (IStreamingGenerationErrorHandler*)&LogErrorHandler);

	TArray<FString> OutGeneratedLevelStreamingPackageNames;
	FGenerateStreamingContext Context = FGenerateStreamingContext()
		.SetLevelPackagesToGenerate((bIsPIE || IsRunningGame()) ? &OutGeneratedLevelStreamingPackageNames : nullptr);

	GenerateStreaming(Params, Context);

	// Prepare GeneratedStreamingPackages
	check(GeneratedLevelStreamingPackageNames.IsEmpty());
	for (const FString& PackageName : OutGeneratedLevelStreamingPackageNames)
	{
		// Set as memory package to avoid wasting time in UWorldPartition::IsValidPackageName (GenerateStreaming for PIE runs on the editor world)
		FString Package = FPaths::RemoveDuplicateSlashes(FPackageName::IsMemoryPackage(PackageName) ? PackageName : TEXT("/Memory/") + PackageName);
		GeneratedLevelStreamingPackageNames.Add(Package);
	}

	RuntimeHash->OnBeginPlay();

	ExternalDataLayerManager->OnBeginPlay();
}

void UWorldPartition::OnCancelPIE()
{
	// Call OnEndPlay here since EndPlayMapDelegate is not called when cancelling PIE
	OnEndPlay();
}

void UWorldPartition::OnEndPlay()
{
	// No check here since CancelPIE can be called after PrePIEEnded
	if (bIsPIE)
	{
		FlushStreaming();
		ExternalDataLayerManager->OnEndPlay();
		RuntimeHash->OnEndPlay();
		bIsPIE = false;
	}
}

bool UWorldPartition::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UWorldPartition, ServerStreamingOutMode))
	{
		return bEnableStreaming && (ServerStreamingMode != EWorldPartitionServerStreamingMode::Disabled);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UWorldPartition, ServerStreamingMode))
	{
		return bEnableStreaming;
	}

	return true;
}

FName UWorldPartition::GetWorldPartitionEditorName() const
{
	if (SupportsStreaming())
	{
		return EditorHash->GetWorldPartitionEditorName();
	}
	return NAME_None;
}
#endif

bool UWorldPartition::CanInitialize(UWorld* InWorld) const
{
	check(InWorld);
	if (!IsInitialized() && InWorld->IsGameWorld())
	{
		if (UWorldPartitionSubsystem* WorldPartitionSubsystem = InWorld->GetSubsystem<UWorldPartitionSubsystem>())
		{
			if (WorldPartitionSubsystem->HasUninitializationPendingStreamingLevels(this))
			{
				return false;
			}
		}
	}
	return true;
}

void UWorldPartition::Initialize(UWorld* InWorld, const FTransform& InTransform)
{
	UE_SCOPED_TIMER(TEXT("WorldPartition initialize"), LogWorldPartition, Display);
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartition::Initialize);

	check(!World || (World == InWorld));
	if (!ensure(!IsInitialized()))
	{
		return;
	}

	if (IsTemplate())
	{
		return;
	}

	check(InWorld);
	check(CanInitialize(InWorld));
	World = InWorld;

	if (!InTransform.Equals(FTransform::Identity))
	{
		InstanceTransform = InTransform;
	}

	check(InitState == EWorldPartitionInitState::Uninitialized);
	InitState = EWorldPartitionInitState::Initializing;

	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	check(OuterWorld);

	RegisterDelegates();

	if (IsMainWorldPartition())
	{
		AWorldPartitionReplay::Initialize(World);
	}

	const bool bIsGame = IsRunningGame();
	const bool bIsEditor = !World->IsGameWorld();
	const bool bIsCooking = IsRunningCookCommandlet();
	const bool bIsPIEWorldTravel = (World->WorldType == EWorldType::PIE) && !StreamingPolicy;
	const bool bIsDedicatedServer = IsRunningDedicatedServer();

	UE_LOG(LogWorldPartition, Log, TEXT("UWorldPartition::Initialize : World = %s, World Type = %s, IsMainWorldPartition = %d, Location = %s, Rotation = %s, IsEditor = %d, IsGame = %d, IsPIEWorldTravel = %d, IsCooking = %d"),
		*OuterWorld->GetPathName(),
		LexToString(World->WorldType),
		IsMainWorldPartition() ? 1 : 0,
		*InTransform.GetLocation().ToCompactString(),
		*InTransform.Rotator().ToCompactString(),
		bIsEditor,
		bIsGame,
		bIsPIEWorldTravel,
		bIsCooking);

	if (World->IsGameWorld())
	{
		UE_LOG(LogWorldPartition, Log, TEXT("UWorldPartition::Initialize Context : World NetMode = %s, IsServer = %d, IsDedicatedServer = %d, IsServerStreamingEnabled = %d, IsServerStreamingOutEnabled = %d, IsUsingMakingVisibleTransaction = %d, IsUsingMakingInvisibleTransaction = %d"),
			*ToString(World->GetNetMode()),
			IsServer() ? 1 : 0, 
			bIsDedicatedServer ? 1 : 0, 
			IsServerStreamingEnabled() ? 1 : 0, 
			IsServerStreamingOutEnabled() ? 1 : 0, 
			UseMakingVisibleTransactionRequests() ? 1 : 0, 
			UseMakingInvisibleTransactionRequests() ? 1 : 0);
	}

	auto CreateAndInitializeDataLayerManager = [this]()
	{
		check(!DataLayerManager);
		DataLayerManager = NewObject<UDataLayerManager>(this, TEXT("DataLayerManager"), RF_Transient);
		DataLayerManager->Initialize();
	};

#if WITH_EDITOR
	if (bEnableStreaming)
	{
		bStreamingWasEnabled = true;
	}

	if (bIsGame || bIsCooking)
	{
		// Don't rely on the editor hash for cooking or -game
		EditorHash = nullptr;
		AlwaysLoadedActors = nullptr;
	}
	else if (bIsEditor)
	{
		CreateOrRepairWorldPartition(OuterWorld->GetWorldSettings());

		check(!StreamingPolicy);
		check(EditorHash);

		EditorHash->Initialize();

		AlwaysLoadedActors = new FLoaderAdapterAlwaysLoadedActors(OuterWorld);

		if (IsMainWorldPartition())
		{			
			PinnedActors = new FLoaderAdapterPinnedActors(OuterWorld);
		
			IWorldPartitionEditorModule& WorldPartitionEditorModule = FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");
			ForceLoadedActors = WorldPartitionEditorModule.GetEnableLoadingInEditor() ? nullptr : new FLoaderAdapterActorList(OuterWorld);
		}
	}

	check(RuntimeHash);
	RuntimeHash->SetFlags(RF_Transactional);

	if (bIsEditor || bIsGame || bIsPIEWorldTravel || bIsDedicatedServer)
	{
		FName ContainerPackageName = UActorDescContainerInstance::GetContainerPackageNameFromWorld(OuterWorld);
		ActorDescContainerInstance = RegisterActorDescContainerInstance(UActorDescContainerInstance::FInitializeParams(ContainerPackageName));
		CreateAndInitializeDataLayerManager();
		InitializeActorDescContainerEditorStreaming(ActorDescContainerInstance);
	}
#endif

#if !WITH_EDITOR
	check(!DataLayerManager);
	check(!ExternalDataLayerManager);
#endif

	// Create and initialize the DataLayerManager (When WorldPartition's ActorDescContainerInstance is created, we create/initialize the DataLayerManager before calling InitializeActorDescContainerEditorStreaming)
	if (!DataLayerManager)
	{
		CreateAndInitializeDataLayerManager();
	}

	// Create and initialize the ExternalDataLayerManager (In PIE, we use the exiting/duplicated ExternalDataLayerManager containing the duplicated ExternalStreamingObjects)
	if (!ExternalDataLayerManager)
	{
		ExternalDataLayerManager = NewObject<UExternalDataLayerManager>(this, TEXT("ExternalDataLayerManager"), RF_Transient | RF_Transactional);
	}
	ExternalDataLayerManager->Initialize();
	
#if WITH_EDITOR
	if (bIsEditor)
	{
		// Apply level transform on actors already part of the level
		if (!GetInstanceTransform().Equals(FTransform::Identity))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ApplyLevelTransform);

			check(!OuterWorld->PersistentLevel->bAlreadyMovedActors);
			for (AActor* Actor : OuterWorld->PersistentLevel->Actors)
			{
				if (Actor)
				{
					FLevelUtils::FApplyLevelTransformParams TransformParams(Actor->GetLevel(), GetInstanceTransform());
					TransformParams.Actor = Actor;
					TransformParams.bDoPostEditMove = true;
					FLevelUtils::ApplyLevelTransform(TransformParams);
				}
			}
			// Flag Level's bAlreadyMovedActors to true so that ULevelStreaming::PrepareLoadedLevel won't reapply the same transform again.
			OuterWorld->PersistentLevel->bAlreadyMovedActors = true;
		}
	}

	if (bIsEditor && !bIsCooking)
	{
		// Load the always loaded cell
		if (AlwaysLoadedActors)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LoadAlwaysLoaded);
			AlwaysLoadedActors->Load();
		}

		// Load more cells depending on the user's settings
		// Skipped when running from a commandlet and for subpartitions
		if (IsMainWorldPartition() && IsStreamingEnabled() && !IsRunningCommandlet() && !GIsAutomationTesting)
		{
			// Load last loaded regions
			if (GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetEnableLoadingOfLastLoadedRegions())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LoadLastLoadedRegions);
				LoadLastLoadedRegions();
			}
		}
	}
#endif //WITH_EDITOR

	InitState = EWorldPartitionInitState::Initialized;

#if WITH_EDITOR
	if (!bIsEditor)
	{
		if (bIsGame || bIsPIEWorldTravel || bIsDedicatedServer)
		{
			OnBeginPlay();
		}

		// Apply remapping of Persistent Level's SoftObjectPaths
		// Here we remap SoftObjectPaths so that they are mapped from the PersistentLevel Package to the Cell Packages using the mapping built by the policy
		FWorldPartitionLevelHelper::RemapLevelSoftObjectPaths(OuterWorld->PersistentLevel, this);
	}
#endif

	FWorldPartitionEvents::BroadcastWorldPartitionInitialized(World, this);
}

void UWorldPartition::Uninitialize()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartition::Uninitialize);

	if (IsInitialized())
	{
		check(World);

		UE_LOG(LogWorldPartition, Log, TEXT("UWorldPartition::Uninitialize : World = %s"), *GetTypedOuter<UWorld>()->GetPathName());

		InitState = EWorldPartitionInitState::Uninitializing;

		if (IsMainWorldPartition())
		{
			AWorldPartitionReplay::Uninitialize(World);
		}

		UnregisterDelegates();
		
		// Unload all loaded cells
		if (World->IsGameWorld())
		{
			UWorldPartitionSubsystem::UpdateStreamingStateInternal(World, this);
		}

#if WITH_EDITOR
		if (IsMainWorldPartition())
		{
			SavePerUserSettings();
		}

		if (World->IsGameWorld())
		{
			OnEndPlay();
		}
		
		if (AlwaysLoadedActors)
		{
			delete AlwaysLoadedActors;
			AlwaysLoadedActors = nullptr;
		}

		if (PinnedActors)
		{
			delete PinnedActors;
			PinnedActors = nullptr;
		}

		if (ForceLoadedActors)
		{
			delete ForceLoadedActors;
			ForceLoadedActors = nullptr;
		}

		if (RegisteredEditorLoaderAdapters.Num())
		{
			for (UWorldPartitionEditorLoaderAdapter* RegisteredEditorLoaderAdapter : RegisteredEditorLoaderAdapters)
			{
				RegisteredEditorLoaderAdapter->Release();
			}

			RegisteredEditorLoaderAdapters.Empty();
		}

#endif

		if (ExternalDataLayerManager)
		{
			ExternalDataLayerManager->DeInitialize();
			ExternalDataLayerManager = nullptr;
		}

		if (DataLayerManager)
		{
			DataLayerManager->DeInitialize();
			DataLayerManager = nullptr;
		}

#if WITH_EDITOR
		UninitializeActorDescContainers();
		ActorDescContainerInstance = nullptr;

		EditorHash = nullptr;
		bIsPIE = false;
#endif		

		InitState = EWorldPartitionInitState::Uninitialized;

		FWorldPartitionEvents::BroadcastWorldPartitionUninitialized(World, this);

		World = nullptr;
	}
}

UDataLayerManager* UWorldPartition::GetDataLayerManager() const
{
	return DataLayerManager;
}

UDataLayerManager* UWorldPartition::GetResolvingDataLayerManager() const
{
	if (UWorld* OwningWorld = GetWorld(); OwningWorld && !OwningWorld->IsGameWorld())
	{
		if (UWorldPartition* OwningWorldPartition = OwningWorld->GetWorldPartition())
		{
			return UDataLayerManager::GetDataLayerManager(OwningWorldPartition);
		}
	}
	return GetDataLayerManager();
}

UExternalDataLayerManager* UWorldPartition::GetExternalDataLayerManager() const
{
	return ExternalDataLayerManager;
}

bool UWorldPartition::IsInitialized() const
{
	return InitState == EWorldPartitionInitState::Initialized;
}

bool UWorldPartition::SupportsStreaming() const
{
	return World ? World->GetWorldSettings()->SupportsWorldPartitionStreaming() : false;
}

bool UWorldPartition::IsStreamingEnabled() const
{
	return bEnableStreaming && SupportsStreaming();
}

bool UWorldPartition::CanStream() const
{
	if (!IsInitialized())
	{
		return false;
	}

	const ULevel* PersistentLevel = GetTypedOuter<UWorld>()->PersistentLevel;
	// Is it a level streamed World Partition that was removed from its owning world
	// or is the World requesting unloading of all streaming levels.
	if (!PersistentLevel->GetWorld() || PersistentLevel->GetWorld()->GetShouldForceUnloadStreamingLevels())
	{
		return false;
	}
		
	// Is it part of a Sub-level that should be visible.
	if (ULevelStreaming* LevelStreaming = ULevelStreaming::FindStreamingLevel(PersistentLevel))
	{
		return !LevelStreaming->GetIsRequestingUnloadAndRemoval() && LevelStreaming->ShouldBeVisible();
	}

	return true;
}

bool UWorldPartition::IsMainWorldPartition() const
{
	check(World);
	return World == GetTypedOuter<UWorld>();
}

#if WITH_EDITOR
void UWorldPartition::OnLevelActorDeleted(AActor* Actor)
{
	if (GIsEditorLoadingPackage)
	{
		if (UActorDescContainerInstance* DescContainerInstance = GetActorDescContainerInstance())
		{
			DescContainerInstance->RemoveActor(Actor->GetActorGuid());
		}
	}
}

void UWorldPartition::OnPostBugItGoCalled(const FVector& Loc, const FRotator& Rot)
{
	if (GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetBugItGoLoadRegion())
	{
		const FVector LoadExtent(UWorldPartition::LoadingRangeBugItGo, UWorldPartition::LoadingRangeBugItGo, HALF_WORLD_MAX);
		const FBox LoadCellsBox(Loc - LoadExtent, Loc + LoadExtent);

		IWorldPartitionEditorModule& WorldPartitionEditorModule = FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");
		if (WorldPartitionEditorModule.GetEnableLoadingInEditor())
		{
			UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = CreateEditorLoaderAdapter<FLoaderAdapterShape>(World, LoadCellsBox, TEXT("BugItGo"));
			EditorLoaderAdapter->GetLoaderAdapter()->Load();
		}

		if (WorldPartitionEditor)
		{
			WorldPartitionEditor->FocusBox(LoadCellsBox);
		}
	}
}
#endif

void UWorldPartition::RegisterDelegates()
{
	check(World); 

#if WITH_EDITOR
	if (GEditor && !IsTemplate() && !World->IsGameWorld() && !IsRunningCookCommandlet())
	{
		if (IsMainWorldPartition())
		{
			FEditorDelegates::PreBeginPIE.AddUObject(this, &UWorldPartition::OnPreBeginPIE);
			FEditorDelegates::PrePIEEnded.AddUObject(this, &UWorldPartition::OnPrePIEEnded);
			FEditorDelegates::CancelPIE.AddUObject(this, &UWorldPartition::OnCancelPIE);
			FGameDelegates::Get().GetEndPlayMapDelegate().AddUObject(this, &UWorldPartition::OnEndPlay);
			FCoreUObjectDelegates::PostReachabilityAnalysis.AddUObject(this, &UWorldPartition::OnGCPostReachabilityAnalysis);
			GEditor->OnLevelActorDeleted().AddUObject(this, &UWorldPartition::OnLevelActorDeleted);
			GEditor->OnPostBugItGoCalled().AddUObject(this, &UWorldPartition::OnPostBugItGoCalled);
			GEditor->OnEditorClose().AddUObject(this, &UWorldPartition::SavePerUserSettings);
			FWorldDelegates::OnPostWorldRename.AddUObject(this, &UWorldPartition::OnWorldRenamed);			
		}

		if (!IsRunningCommandlet())
		{
			ExternalDirtyActorsTracker = MakeUnique<FWorldPartitionExternalDirtyActorsTracker>(this);
		}
	}
#endif

	if (World->IsGameWorld())
	{
		if (IsMainWorldPartition())
		{
			World->OnWorldMatchStarting.AddUObject(this, &UWorldPartition::OnWorldMatchStarting);

#if !UE_BUILD_SHIPPING
			FCoreDelegates::OnGetOnScreenMessages.AddUObject(this, &UWorldPartition::GetOnScreenMessages);
#endif
		}
	}
}

void UWorldPartition::UnregisterDelegates()
{
	check(World);

#if WITH_EDITOR
	if (GEditor && !IsTemplate() && !World->IsGameWorld() && !IsRunningCookCommandlet())
	{
		if (IsMainWorldPartition())
		{
			FWorldDelegates::OnPostWorldRename.RemoveAll(this);
			FEditorDelegates::PreBeginPIE.RemoveAll(this);
			FEditorDelegates::PrePIEEnded.RemoveAll(this);
			FEditorDelegates::CancelPIE.RemoveAll(this);
			FGameDelegates::Get().GetEndPlayMapDelegate().RemoveAll(this);

			if (!IsEngineExitRequested())
			{
				FCoreUObjectDelegates::PostReachabilityAnalysis.RemoveAll(this);
			}

			GEditor->OnLevelActorDeleted().RemoveAll(this);
			GEditor->OnPostBugItGoCalled().RemoveAll(this);
			GEditor->OnEditorClose().RemoveAll(this);
		}

		if (!IsRunningCommandlet())
		{
			ExternalDirtyActorsTracker.Reset();
		}
	}
#endif

	if (World->IsGameWorld())
	{
		if (IsMainWorldPartition())
		{
			World->OnWorldMatchStarting.RemoveAll(this);

#if !UE_BUILD_SHIPPING
			FCoreDelegates::OnGetOnScreenMessages.RemoveAll(this);
#endif
		}
	}
}

#if !UE_BUILD_SHIPPING
void UWorldPartition::GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages)
{
	if (StreamingPolicy)
	{
		StreamingPolicy->GetOnScreenMessages(OutMessages);
	}
}
#endif

void UWorldPartition::OnWorldMatchStarting()
{
	check(GetWorld()->IsGameWorld());
	// Wait for any level streaming to complete
	GetWorld()->BlockTillLevelStreamingCompleted();
}

#if WITH_EDITOR
UWorldPartition* UWorldPartition::CreateOrRepairWorldPartition(AWorldSettings* WorldSettings, TSubclassOf<UWorldPartitionEditorHash> EditorHashClass, TSubclassOf<UWorldPartitionRuntimeHash> RuntimeHashClass)
{
	UWorld* OuterWorld = WorldSettings->GetTypedOuter<UWorld>();
	UWorldPartition* WorldPartition = WorldSettings->GetWorldPartition();

	if (!WorldPartition)
	{
		WorldPartition = NewObject<UWorldPartition>(WorldSettings);
		WorldSettings->SetWorldPartition(WorldPartition);

		// New maps should include GridSize in name
		WorldSettings->bIncludeGridSizeInNameForFoliageActors = true;
		WorldSettings->bIncludeGridSizeInNameForPartitionedActors = true;

		if (IWorldPartitionEditorModule* WorldPartitionEditorModulePtr = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor"))
		{
			WorldSettings->InstancedFoliageGridSize = WorldPartitionEditorModulePtr->GetInstancedFoliageGridSize();
			WorldSettings->DefaultPlacementGridSize = WorldPartitionEditorModulePtr->GetPlacementGridSize();
		}

		WorldSettings->MarkPackageDirty();

		WorldPartition->DefaultHLODLayer = UHLODLayer::GetEngineDefaultHLODLayersSetup();

		AWorldDataLayers* WorldDataLayers = OuterWorld->GetWorldDataLayers();
		if (!WorldDataLayers)
		{
			WorldDataLayers = AWorldDataLayers::Create(OuterWorld);
			OuterWorld->SetWorldDataLayers(WorldDataLayers);
		}

		FWorldPartitionMiniMapHelper::GetWorldPartitionMiniMap(OuterWorld, true);

		WorldPartition->DataLayersLogicOperator = UWorldPartitionSettings::Get()->GetNewMapsDataLayersLogicOperator();
	}

	if (!WorldPartition->EditorHash)
	{
		if (!EditorHashClass)
		{
			EditorHashClass = UWorldPartitionSettings::Get()->GetEditorHashDefaultClass();
		}

		check(EditorHashClass);
		WorldPartition->EditorHash = NewObject<UWorldPartitionEditorHash>(WorldPartition, EditorHashClass);
		WorldPartition->EditorHash->SetDefaultValues();
	}

	if (!WorldPartition->RuntimeHash)
	{
		if (!RuntimeHashClass)
		{
			RuntimeHashClass = UWorldPartitionSettings::Get()->GetRuntimeHashDefaultClass();
		}

		check(RuntimeHashClass);
		WorldPartition->RuntimeHash = NewObject<UWorldPartitionRuntimeHash>(WorldPartition, RuntimeHashClass, NAME_None, RF_Transactional);
		WorldPartition->RuntimeHash->SetDefaultValues();
	}

	OuterWorld->PersistentLevel->bIsPartitioned = true;

	return WorldPartition;
}

bool UWorldPartition::RemoveWorldPartition(AWorldSettings* WorldSettings)
{
	if (UWorldPartition* WorldPartition = WorldSettings->GetWorldPartition())
	{
		if (!WorldPartition->IsStreamingEnabled())
		{
			ULevel* PersistentLevel = WorldSettings->GetLevel();

			TArray<FWorldPartitionReference> ActorReferences;
			ActorReferences.Reserve(PersistentLevel->Actors.Num());

			WorldSettings->Modify();
			
			for (AActor* Actor : PersistentLevel->Actors)
			{
				if (Actor)
				{
					if (Cast<AWorldDataLayers>(Actor) || Cast<AWorldPartitionMiniMap>(Actor) || Cast<AWorldPartitionHLOD>(Actor))
					{
						Actor->Destroy();
					}
					else if(Actor->GetExternalPackage())
					{
						ActorReferences.Emplace(WorldPartition, Actor->GetActorGuid());
					}
				}
			}

			WorldPartition->Uninitialize();
			WorldSettings->SetWorldPartition(nullptr);
			PersistentLevel->bIsPartitioned = false;

			if (WorldPartition->WorldPartitionEditor)
			{
				WorldPartition->WorldPartitionEditor->Reconstruct();
			}
			
			return true;
		}
	}
	return false;
}
#endif

const TArray<FWorldPartitionStreamingSource>& UWorldPartition::GetStreamingSources() const
{
	if (StreamingPolicy && GetWorld()->IsGameWorld())
	{
		return StreamingPolicy->GetStreamingSources();
	}

	static TArray<FWorldPartitionStreamingSource> EmptyStreamingSources;
	return EmptyStreamingSources;
}

bool UWorldPartition::IsServer() const
{
	if (UWorld* OwningWorld = GetWorld())
	{
		const ENetMode NetMode = OwningWorld->GetNetMode();
		return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
	}
	return false;
}

bool UWorldPartition::IsServerStreamingEnabled() const
{
	// Resolve once (we don't allow changing the state at runtime)
	if (!bCachedIsServerStreamingEnabled.IsSet())
	{
		bool bIsEnabled = false;
		UWorld* OwningWorld = GetWorld();
		if (OwningWorld && OwningWorld->IsGameWorld())
		{
			if (ServerStreamingMode == EWorldPartitionServerStreamingMode::ProjectDefault)
			{
				UWorldPartition* MainWorldPartition = OwningWorld->GetWorldPartition();
				if (MainWorldPartition && (this != MainWorldPartition))
				{
					bIsEnabled = MainWorldPartition->IsServerStreamingEnabled();
				}
				else
				{
					switch (UWorldPartition::GlobalEnableServerStreaming)
					{
					case 1:
						bIsEnabled = true;
						break;
#if WITH_EDITOR
					case 2:
						bIsEnabled = bIsPIE;
						break;
#endif
					}
				}
			}
			else
			{
				if ((ServerStreamingMode == EWorldPartitionServerStreamingMode::Enabled)
#if WITH_EDITOR
					|| (bIsPIE && (ServerStreamingMode == EWorldPartitionServerStreamingMode::EnabledInPIE))
#endif
					)
				{
					bIsEnabled = true;
				}
			}
		}

		bCachedIsServerStreamingEnabled = bIsEnabled;
	}
	
	return bCachedIsServerStreamingEnabled.Get(false);
}

bool UWorldPartition::IsServerStreamingOutEnabled() const
{
	// Resolve once (we don't allow changing the state at runtime)
	if (!bCachedIsServerStreamingOutEnabled.IsSet())
	{
		bool bEnableServerStreamingOut = false;
		UWorld* OwningWorld = GetWorld();
		if (OwningWorld && OwningWorld->IsGameWorld() && IsServerStreamingEnabled())
		{
			if (ServerStreamingMode == EWorldPartitionServerStreamingMode::ProjectDefault)
			{
				UWorldPartition* MainWorldPartition = OwningWorld->GetWorldPartition();
				if (MainWorldPartition && (this != MainWorldPartition))
				{
					bEnableServerStreamingOut = MainWorldPartition->IsServerStreamingOutEnabled();
				}
				else
				{
					bEnableServerStreamingOut = UWorldPartition::bGlobalEnableServerStreamingOut;
				}
			}
			else
			{
				bEnableServerStreamingOut = (ServerStreamingOutMode == EWorldPartitionServerStreamingOutMode::Enabled);
			}
		}
		bCachedIsServerStreamingOutEnabled = bEnableServerStreamingOut;
	}

	return bCachedIsServerStreamingOutEnabled.Get(false);
}

bool UWorldPartition::UseMakingVisibleTransactionRequests() const
{
	// Resolve once (we don't allow changing the state at runtime)
	if (!bCachedUseMakingVisibleTransactionRequests.IsSet())
	{
		UWorld* OwningWorld = GetWorld();
		bCachedUseMakingVisibleTransactionRequests = OwningWorld && OwningWorld->IsGameWorld() && UWorldPartition::bUseMakingVisibleTransactionRequests;
	}
	return bCachedUseMakingVisibleTransactionRequests.Get(false);
}

bool UWorldPartition::UseMakingInvisibleTransactionRequests() const
{
	// Resolve once (we don't allow changing the state at runtime)
	if (!bCachedUseMakingInvisibleTransactionRequests.IsSet())
	{
		UWorld* OwningWorld = GetWorld();
		bCachedUseMakingInvisibleTransactionRequests = OwningWorld && OwningWorld->IsGameWorld() && UWorldPartition::bUseMakingInvisibleTransactionRequests;
	}
	return bCachedUseMakingInvisibleTransactionRequests.Get(false);
}

int32 UWorldPartition::GetStreamingStateEpoch() const
{
	// Merge WorldPartition's StreamingStateEpoch and AWorldDataLayers DataLayersStateEpoch
	const UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const AWorldDataLayers* WorldDataLayers = OuterWorld->GetWorldDataLayers();
	return HashCombineFast(StreamingStateEpoch, WorldDataLayers ? WorldDataLayers->GetDataLayersStateEpoch() : 0);
}

bool UWorldPartition::IsSimulating(bool bIncludeTestEnableSimulationStreamingSource)
{
#if WITH_EDITOR
	return GEditor && GEditor->bIsSimulatingInEditor && GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->IsSimulateInEditorViewport() && (!bIncludeTestEnableSimulationStreamingSource || UWorldPartition::EnableSimulationStreamingSource);
#else
	return false;
#endif
}

#if WITH_EDITOR
void UWorldPartition::OnActorDescInstanceAdded(FWorldPartitionActorDescInstance* NewActorDescInstance)
{
	if (const UDataLayerManager* ResolvingDataLayerManager = GetResolvingDataLayerManager())
	{
		ResolvingDataLayerManager->ResolveActorDescInstanceDataLayers(NewActorDescInstance);
	}

	NewActorDescInstance->SetForceNonSpatiallyLoaded(!IsStreamingEnabledInEditor());

	HashActorDescInstance(NewActorDescInstance);

	if (ForceLoadedActors)
	{
		ForceLoadedActors->AddActors({ NewActorDescInstance->GetGuid() });
	}

	if (AlwaysLoadedActors && !NewActorDescInstance->GetIsSpatiallyLoaded())
	{
		AlwaysLoadedActors->RefreshLoadedState();
	}

	if (WorldPartitionEditor)
	{
		WorldPartitionEditor->Refresh();
	}
}

void UWorldPartition::OnActorDescInstanceRemoved(FWorldPartitionActorDescInstance* ActorDescInstance)
{
	if (PinnedActors)
	{
		PinnedActors->RemoveActors({ FWorldPartitionHandle(ActorDescInstance->GetContainerInstance(), ActorDescInstance->GetGuid()) });
	}

	UnhashActorDescInstance(ActorDescInstance);

	if (ForceLoadedActors)
	{
		ForceLoadedActors->RemoveActors({ ActorDescInstance->GetGuid() });
	}

	if (AlwaysLoadedActors && !ActorDescInstance->GetIsSpatiallyLoaded())
	{
		AlwaysLoadedActors->RefreshLoadedState();
	}

	if (WorldPartitionEditor)
	{
		WorldPartitionEditor->Refresh();
	}
}

void UWorldPartition::OnActorDescInstanceUpdating(FWorldPartitionActorDescInstance* ActorDescInstance)
{
	UnhashActorDescInstance(ActorDescInstance);
}

void UWorldPartition::OnActorDescInstanceUpdated(FWorldPartitionActorDescInstance* ActorDescInstance)
{
	if (const UDataLayerManager* ResolvingDataLayerManager = GetResolvingDataLayerManager())
	{
		ResolvingDataLayerManager->ResolveActorDescInstanceDataLayers(ActorDescInstance);
	}

	HashActorDescInstance(ActorDescInstance);

	if (WorldPartitionEditor)
	{
		WorldPartitionEditor->Refresh();
	}
}

bool UWorldPartition::ShouldHashUnhashActorDescInstances() const
{
	const bool bIsEditor = !GetWorld()->IsGameWorld();
	const bool bIsCooking = IsRunningCookCommandlet();
	const bool bHashActorDescs = EditorHash && bIsEditor && !bIsCooking;
	return bHashActorDescs;
}

void UWorldPartition::InitializeActorDescContainerEditorStreaming(UActorDescContainerInstance* InActorDescContainerInstance)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(InitializeActorDescContainerEditorStreaming);

	const bool bHashActorDescs = ShouldHashUnhashActorDescInstances();
	const bool bIsStreamingEnabled = IsStreamingEnabledInEditor();

	TArray<FGuid> ForceLoadedActorGuids;
	for (UActorDescContainerInstance::TIterator<> It(InActorDescContainerInstance); It; ++It)
	{
		It->SetForceNonSpatiallyLoaded(!bIsStreamingEnabled);

		if (ForceLoadedActors)
		{
			ForceLoadedActorGuids.Add(It->GetGuid());
		}

		if (bHashActorDescs)
		{
			HashActorDescInstance(*It);
		}
	}

	if (ForceLoadedActorGuids.Num())
	{
		check(ForceLoadedActors);
		ForceLoadedActors->AddActors(ForceLoadedActorGuids);
	}
}
#endif

const FTransform& UWorldPartition::GetInstanceTransform() const
{
	return InstanceTransform.IsSet() ? InstanceTransform.GetValue() : FTransform::Identity;
}

#if WITH_EDITOR
void UWorldPartition::SetEnableStreaming(bool bInEnableStreaming)
{
	if (bEnableStreaming != bInEnableStreaming)
	{
		const FScopedTransaction Transaction(LOCTEXT("EditorWorldPartitionSetEnableStreaming", "Set WorldPartition EnableStreaming"));

		SetFlags(RF_Transactional);
		Modify();
		bEnableStreaming = bInEnableStreaming;
		OnEnableStreamingChanged();
	}
}

void UWorldPartition::OnEnableStreamingChanged()
{
	for (FActorDescContainerInstanceCollection::TIterator<> Iterator(this); Iterator; ++Iterator)
	{
		UnhashActorDescInstance(*Iterator);
		Iterator->SetForceNonSpatiallyLoaded(!IsStreamingEnabledInEditor());
		HashActorDescInstance(*Iterator);
	}

	FLoaderAdapterAlwaysLoadedActors* OldAlwaysLoadedActors = AlwaysLoadedActors;

	AlwaysLoadedActors = new FLoaderAdapterAlwaysLoadedActors(GetTypedOuter<UWorld>());
	AlwaysLoadedActors->Load();

	OldAlwaysLoadedActors->Unload();
	delete OldAlwaysLoadedActors;

	if (WorldPartitionEditor)
	{
		WorldPartitionEditor->Reconstruct();
	}
}

void UWorldPartition::OnEnableLoadingInEditorChanged()
{
	if (ForceLoadedActors)
	{
		delete ForceLoadedActors;
		ForceLoadedActors = nullptr;
	}

	IWorldPartitionEditorModule& WorldPartitionEditorModule = FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");

	if (!WorldPartitionEditorModule.GetEnableLoadingInEditor())
	{
		UWorld* OuterWorld = GetTypedOuter<UWorld>();
		check(OuterWorld);

		ForceLoadedActors = new FLoaderAdapterActorList(OuterWorld);

		TArray<FGuid> ForceLoadedActorGuids;
		for (FActorDescContainerInstanceCollection::TIterator<> Iterator(this); Iterator; ++Iterator)
		{
			ForceLoadedActorGuids.Add(Iterator->GetGuid());
		}

		if (ForceLoadedActorGuids.Num())
		{
			ForceLoadedActors->AddActors(ForceLoadedActorGuids);
		}
	}
}

void UWorldPartition::HashActorDescInstance(FWorldPartitionActorDescInstance* ActorDescInstance)
{
	check(ActorDescInstance);
	check(EditorHash);

	FWorldPartitionHandle ActorHandle(ActorDescInstance);
	EditorHash->HashActor(ActorHandle);

	bShouldCheckEnableStreamingWarning = IsMainWorldPartition();
}

void UWorldPartition::UnhashActorDescInstance(FWorldPartitionActorDescInstance* ActorDescInstance)
{
	check(ActorDescInstance);
	check(EditorHash);

	FWorldPartitionHandle ActorHandle(ActorDescInstance);
	EditorHash->UnhashActor(ActorHandle);
}

bool UWorldPartition::IsStreamingEnabledInEditor() const
{
	return bOverrideEnableStreamingInEditor.IsSet() ? *bOverrideEnableStreamingInEditor : IsStreamingEnabled();
}
#endif

void UWorldPartition::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << ExternalDataLayerManager;
		Ar << StreamingPolicy;
		Ar << GeneratedLevelStreamingPackageNames;
		Ar << bIsPIE;
	}
	else
#endif
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionSerializeStreamingPolicyOnCook)
		{
			bool bCooked = Ar.IsCooking();
			Ar << bCooked;

			if (bCooked)
			{
				Ar << StreamingPolicy;
			}
		}
	
		if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionDataLayersLogicOperatorAdded)
		{
			DataLayersLogicOperator = EWorldPartitionDataLayersLogicOperator::Or;
		}
	}
}

UWorld* UWorldPartition::GetWorld() const
{
	if (World)
	{
		return World;
	}
	return Super::GetWorld();
}

bool UWorldPartition::ResolveSubobject(const TCHAR* SubObjectPath, UObject*& OutObject, bool bLoadIfExists)
{
	if (GetWorld())
	{
		if (GetWorld()->IsGameWorld())
		{
			if (StreamingPolicy)
			{
				if (UObject* SubObject = StreamingPolicy->GetSubObject(SubObjectPath))
				{
					OutObject = SubObject;
					return true;
				}
				else
				{
					OutObject = nullptr;
				}
			}
		}
#if WITH_EDITOR
		else
		{
			// Support for subobjects such as Actor.Component
			FString SubObjectName;
			FString SubObjectContext;	
			if (!FString(SubObjectPath).Split(TEXT("."), &SubObjectContext, &SubObjectName))
			{
				SubObjectName = SubObjectPath;
			}

			if (const FWorldPartitionActorDescInstance* ActorDescInstance = GetActorDescInstanceByPath(SubObjectName))
			{
				if (bLoadIfExists)
				{
					LoadedSubobjects.Emplace(this, ActorDescInstance->GetGuid());
				}

				OutObject = StaticFindObject(UObject::StaticClass(), GetWorld()->PersistentLevel, SubObjectPath);
				return true;
			}
		}
#endif
	}

	return false;
}

void UWorldPartition::BeginDestroy()
{
	check(InitState == EWorldPartitionInitState::Uninitialized);
	Super::BeginDestroy();
}

void UWorldPartition::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
#if WITH_EDITOR
	UWorldPartition* This = CastChecked<UWorldPartition>(InThis);

	// We need to keep all dirty actors alive, mainly for deleted actors. Normally, these actors are only referenced
	// by the transaction buffer, but we clear it when unloading regions, etc. and we don't want these actors to die.
	// Also, we must avoid reporting these references when not collecting garbage, as code such as package deletion
	// will skip packages with actors still referenced (via GatherObjectReferencersForDeletion).
	if (This->ExternalDirtyActorsTracker.IsValid() && IsGarbageCollecting())
	{
		Collector.AllowEliminatingReferences(false);
		for (auto& [WeakActor, Value] : This->ExternalDirtyActorsTracker->GetDirtyActors())
		{
			if (TObjectPtr<AActor> Actor = WeakActor.Get(true))
			{
				Collector.AddReferencedObject(Actor);
			}
		}
		Collector.AllowEliminatingReferences(true);
	}

	for (TObjectPtr<UActorDescContainerInstance>& ContainerInstance : This->ActorDescContainerInstanceCollection)
	{
		Collector.AddReferencedObject(ContainerInstance);
	}
#endif

	Super::AddReferencedObjects(InThis, Collector);
}

void UWorldPartition::Tick(float DeltaSeconds)
{
#if WITH_EDITOR
	if (EditorHash)
	{
		EditorHash->Tick(DeltaSeconds);
	}

	if (ExternalDirtyActorsTracker)
	{
		ExternalDirtyActorsTracker->Tick(DeltaSeconds);
	}

	if (bForceGarbageCollection)
	{
		GEngine->ForceGarbageCollection(bForceGarbageCollectionPurge);

		bForceGarbageCollection = false;
		bForceGarbageCollectionPurge = false;
	}

	if (bShouldCheckEnableStreamingWarning)
	{
		bShouldCheckEnableStreamingWarning = false;

		if (!IsStreamingEnabled() && SupportsStreaming())
		{
			bEnablingStreamingJustified = false;

			FBox AllActorsBounds(ForceInit);
			for (FActorDescContainerInstanceCollection::TConstIterator<> Iterator(this); Iterator; ++Iterator)
			{
				if (Iterator->GetActorDesc()->GetIsSpatiallyLoadedRaw() || Iterator->GetActorNativeClass()->IsChildOf<ALandscapeProxy>())
				{
					const FBox EditorBounds = Iterator->GetEditorBounds();
					if (EditorBounds.IsValid)
					{
						AllActorsBounds += EditorBounds;

						// Warn the user if the world becomes larger that WorldExtent in any axis
						if (AllActorsBounds.GetSize().GetMax() >= UWorldPartition::WorldExtentToEnableStreaming)
						{
							bEnablingStreamingJustified = true;
							break;
						}
					}
				}
			}
		}
	}
#endif
}

bool UWorldPartition::IsExternalStreamingObjectInjected(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject) const
{
	return RuntimeHash->IsExternalStreamingObjectInjected(InExternalStreamingObject);
}

bool UWorldPartition::InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject)
{
	bool bInjected = RuntimeHash->InjectExternalStreamingObject(InExternalStreamingObject);
	if (bInjected)
	{
		if (StreamingPolicy)
		{
			StreamingPolicy->InjectExternalStreamingObject(InExternalStreamingObject);
		}
		GetWorld()->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->OnExternalStreamingObjectInjected(InExternalStreamingObject);
		++StreamingStateEpoch;

#if DO_CHECK
		check(InExternalStreamingObject->TargetInjectedWorldPartition.IsExplicitlyNull());
		InExternalStreamingObject->TargetInjectedWorldPartition = this;
#endif
	}

	return bInjected;
}

bool UWorldPartition::RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject)
{
	bool bRemoved = RuntimeHash->RemoveExternalStreamingObject(InExternalStreamingObject);
	if (bRemoved)
	{
#if DO_CHECK
		check(InExternalStreamingObject->TargetInjectedWorldPartition.IsValid());
		InExternalStreamingObject->TargetInjectedWorldPartition = nullptr;
#endif

		if (StreamingPolicy)
		{
			StreamingPolicy->RemoveExternalStreamingObject(InExternalStreamingObject);
		}
		
		GetWorld()->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->OnExternalStreamingObjectRemoved(InExternalStreamingObject);
		++StreamingStateEpoch;
	}

	return bRemoved;
}

bool UWorldPartition::GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells) const
{
	if (StreamingPolicy)
	{
		return StreamingPolicy->GetIntersectingCells(InSources, OutCells);
	}
	return false;
}

bool UWorldPartition::CanAddCellToWorld(const IWorldPartitionCell* InCell) const
{
	if (GetWorld()->IsGameWorld() && StreamingPolicy)
	{
		if (const UWorldPartitionRuntimeCell* Cell = Cast<const UWorldPartitionRuntimeCell>(InCell))
		{
			return StreamingPolicy->CanAddCellToWorld(Cell);
		}
	}
	return true;
}

bool UWorldPartition::IsStreamingCompleted(const TArray<FWorldPartitionStreamingSource>* InStreamingSources) const
{
	if (GetWorld()->IsGameWorld() && StreamingPolicy)
	{
		++StreamingStateEpoch; // Update streaming state epoch to make sure we reevaluate streaming sources
		return StreamingPolicy->IsStreamingCompleted(InStreamingSources);
	}
	return true;
}

bool UWorldPartition::IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState) const
{
	if (GetWorld()->IsGameWorld() && StreamingPolicy)
	{
		++StreamingStateEpoch; // Update streaming state epoch to make sure we reevaluate streaming sources
		return StreamingPolicy->IsStreamingCompleted(QueryState, QuerySources, bExactState);
	}

	return true;
}

void UWorldPartition::OnCellShown(const UWorldPartitionRuntimeCell* InCell)
{
	check(IsInitialized());
	// Discard Cell's LevelStreaming notification when once WorldPartition is unitialized (can happen for instanced WorldPartition)
	if (GetWorld()->IsGameWorld())
	{
		if (IsStreamingEnabled())
		{
			GetWorld()->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->OnCellShown(InCell);
		}
		StreamingPolicy->OnCellShown(InCell);
	}
}

void UWorldPartition::OnCellHidden(const UWorldPartitionRuntimeCell* InCell)
{
	check(IsInitialized());
	// Discard Cell's LevelStreaming notification when once WorldPartition is unitialized (can happen for instanced WorldPartition)
	if (GetWorld()->IsGameWorld())
	{
		if (IsStreamingEnabled())
		{
			GetWorld()->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->OnCellHidden(InCell);
		}
		StreamingPolicy->OnCellHidden(InCell);
	}
}

bool UWorldPartition::DrawRuntimeHash2D(FWorldPartitionDraw2DContext& DrawContext)
{
	return StreamingPolicy->DrawRuntimeHash2D(DrawContext);
}

void UWorldPartition::DrawRuntimeHash3D()
{
	StreamingPolicy->DrawRuntimeHash3D();
}

void UWorldPartition::DrawRuntimeCellsDetails(class UCanvas* Canvas, FVector2D& Offset)
{
	StreamingPolicy->DrawRuntimeCellsDetails(Canvas, Offset);
}

EWorldPartitionStreamingPerformance UWorldPartition::GetStreamingPerformance() const
{
	return StreamingPolicy->GetStreamingPerformance();
}

bool UWorldPartition::IsStreamingInEnabled() const
{
	if (IsServer() && !IsServerStreamingEnabled())
	{
		return true;
	}
	return bStreamingInEnabled;
}

void UWorldPartition::DisableStreamingIn()
{
	UE_CLOG(!bStreamingInEnabled, LogWorldPartition, Warning, TEXT("UWorldPartition::DisableStreamingIn called while streaming was already disabled."));
	bStreamingInEnabled = false;
}

void UWorldPartition::EnableStreamingIn()
{
	UE_CLOG(bStreamingInEnabled, LogWorldPartition, Warning, TEXT("UWorldPartition::EnableStreamingIn called while streaming was already enabled."));
	bStreamingInEnabled = true;
}

bool UWorldPartition::ConvertEditorPathToRuntimePath(const FSoftObjectPath& InPath, FSoftObjectPath& OutPath) const
{
	return StreamingPolicy ? StreamingPolicy->ConvertEditorPathToRuntimePath(InPath, OutPath) : false;
}

#if WITH_EDITOR
void UWorldPartition::DrawRuntimeHashPreview()
{
	RuntimeHash->DrawPreview();
}

TArray<FBox> UWorldPartition::GetUserLoadedEditorRegions() const
{
	TArray<FBox> Result;

	for (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter : RegisteredEditorLoaderAdapters)
	{
		IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter();
		check(LoaderAdapter);
		if (LoaderAdapter->GetBoundingBox().IsSet() && LoaderAdapter->IsLoaded() && LoaderAdapter->GetUserCreated())
		{
			Result.Add(*LoaderAdapter->GetBoundingBox());
		}
	}

	return Result;
}

void UWorldPartition::SavePerUserSettings()
{
	check(IsMainWorldPartition());

	if (GIsEditor && !World->IsGameWorld() && !IsRunningCommandlet() && !IsEngineExitRequested())
	{
		GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->SetEditorLoadedRegions(GetWorld(), GetUserLoadedEditorRegions());

		TArray<FName> EditorLoadedLocationVolumes;
		for (FActorDescContainerInstanceCollection::TConstIterator<> Iterator(this); Iterator; ++Iterator)
		{
			if (ALocationVolume* LocationVolume = Cast<ALocationVolume>(Iterator->GetActor()); IsValid(LocationVolume))
			{
				check(LocationVolume->GetClass()->ImplementsInterface(UWorldPartitionActorLoaderInterface::StaticClass()));

				IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(LocationVolume)->GetLoaderAdapter();
				check(LoaderAdapter);

				if (LoaderAdapter->IsLoaded() && LoaderAdapter->GetUserCreated())
				{
					EditorLoadedLocationVolumes.Add(LocationVolume->GetFName());
				}
			}
		}
		GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->SetEditorLoadedLocationVolumes(GetWorld(), EditorLoadedLocationVolumes);
	}
}

void UWorldPartition::DumpActorDescs(const FString& Path)
{
	if (FArchive* LogFile = IFileManager::Get().CreateFileWriter(*Path))
	{
		TArray<const FWorldPartitionActorDescInstance*> ActorDescInstances;
		TMap<FName, FString> DataLayersDumpString = GetDataLayersDumpString(this);
		for (FActorDescContainerInstanceCollection::TConstIterator<> Iterator(this); Iterator; ++Iterator)
		{
			ActorDescInstances.Add(*Iterator);
		}
		ActorDescInstances.Sort([](const FWorldPartitionActorDescInstance& A, const FWorldPartitionActorDescInstance& B)
		{
			return A.GetGuid() < B.GetGuid();
		});
		for (const FWorldPartitionActorDescInstance* Iterator : ActorDescInstances)
		{
			FString LineEntry = GetActorDescDumpString(Iterator, DataLayersDumpString);
			LogFile->Serialize(TCHAR_TO_ANSI(*LineEntry), LineEntry.Len());
		}

		LogFile->Close();
		delete LogFile;
	}
}

void UWorldPartition::AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	FAssetRegistryTagsContextData Context(this, EAssetRegistryTagsCaller::Uncategorized);
	AppendAssetRegistryTags(Context);
	for (TPair<FName, FAssetRegistryTag>& Pair : Context.Tags)
	{
		OutTags.Add(MoveTemp(Pair.Value));
	}
}

void UWorldPartition::AppendAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	static const FName NAME_LevelIsPartitioned(TEXT("LevelIsPartitioned"));
	Context.AddTag(FAssetRegistryTag(NAME_LevelIsPartitioned, TEXT("1"), FAssetRegistryTag::TT_Hidden));

	if (!IsStreamingEnabled())
	{
		static const FName NAME_LevelHasStreamingDisabled(TEXT("LevelHasStreamingDisabled"));
		Context.AddTag(FAssetRegistryTag(NAME_LevelHasStreamingDisabled, TEXT("1"), FAssetRegistryTag::TT_Hidden));
	}

	// Append world references so we can perform changelists validations without loading it
	const ActorsReferencesUtils::FGetActorReferencesParams Params = ActorsReferencesUtils::FGetActorReferencesParams(GetWorld())
		.SetRequiredFlags(RF_HasExternalPackage);
	TArray<ActorsReferencesUtils::FActorReference> WorldExternalActorReferences = ActorsReferencesUtils::GetActorReferences(Params);
		
	if (WorldExternalActorReferences.Num())
	{
		FStringBuilderBase StringBuilder;
		for (const ActorsReferencesUtils::FActorReference& ActorReference : WorldExternalActorReferences)
		{
			StringBuilder.Append(ActorReference.Actor->GetActorGuid().ToString(EGuidFormats::Short));
			StringBuilder.AppendChar(TEXT(','));
		}
		StringBuilder.RemoveSuffix(1);

		static const FName NAME_WorldExternalActorsReferences(TEXT("WorldExternalActorsReferences"));
		Context.AddTag(FAssetRegistryTag(NAME_WorldExternalActorsReferences, StringBuilder.ToString(), FAssetRegistryTag::TT_Hidden));
	}
}

UActorDescContainerInstance* UWorldPartition::RegisterActorDescContainerInstance(const UActorDescContainerInstance::FInitializeParams& InParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartition::RegisterActorDescContainer);
	const bool bIsEditor = !World->IsGameWorld();
	const bool bIsGameWorld = !bIsEditor;
	const bool bIsCooking = IsRunningCookCommandlet();
	const bool bIsStreamedLevel = ULevelStreaming::FindStreamingLevel(GetTypedOuter<ULevel>()) != nullptr;
	
	if (!Contains(InParams.ContainerPackageName))
	{	
		// Initialize ContainerInstance hierarchy if we are the main world partition or if we are a game streamed world partition which means we have our own generate streaming
		const bool bCreateContainerInstanceHierarchy = IsMainWorldPartition() || (bIsGameWorld && bIsStreamedLevel) || InParams.bCreateContainerInstanceHierarchy;
		UActorDescContainerInstance::FInitializeParams InitParams(InParams.ContainerPackageName, bCreateContainerInstanceHierarchy);
		InitParams.ContentBundleGuid = InParams.ContentBundleGuid;
		InitParams.ExternalDataLayerAsset = InParams.ExternalDataLayerAsset;
		
		const FWorldDataLayersActorDesc* WorldDataLayerActorsDesc = nullptr;
		InitParams.FilterActorDescFunc = [this, &WorldDataLayerActorsDesc, &InParams](const FWorldPartitionActorDesc* ActorDesc)
		{
			if (InParams.FilterActorDescFunc && !InParams.FilterActorDescFunc(ActorDesc))
			{
				return false;
			}

			// Filter duplicate WorldDataLayers
			if (ActorDesc->GetActorNativeClass()->IsChildOf<AWorldDataLayers>())
			{
				const FWorldDataLayersActorDesc* FoundWorldDataLayerActorsDesc = StaticCast<const FWorldDataLayersActorDesc*>(ActorDesc);
				if (FoundWorldDataLayerActorsDesc != nullptr && WorldDataLayerActorsDesc != nullptr)
				{
					UE_LOG(LogWorldPartition, Warning, TEXT("Extra World Data Layer '%s' actor found. Clean up invalid actors to remove the error."), *ActorDesc->GetActorPackage().ToString());
					return false;
				}

				WorldDataLayerActorsDesc = FoundWorldDataLayerActorsDesc;
			}

			// Filter actors with duplicated GUID in WorldPartition (across containers):
			// difference with the duplicate check in UActorDescContainerInstance is that WorldPartition is a collection of containers so same Guid could exist across those containers
			// which wouldn't be validated by the container itself.
			if (GetActorDescInstance(ActorDesc->GetGuid()))
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Found existing actor descriptor guid `%s`: Actor: '%s' from package '%s'"),
					*ActorDesc->GetGuid().ToString(),
					*ActorDesc->GetActorName().ToString(),
					*ActorDesc->GetActorPackage().ToString());
				return false;
			}

			return true;
		};

		InitParams.OnInitializedFunc = [&InParams](UActorDescContainerInstance* InActorDescContainerInstance)
		{
			if (InParams.OnInitializedFunc)
			{
				InParams.OnInitializedFunc(InActorDescContainerInstance);
			}
		};

		UActorDescContainerInstance* ContainerInstanceToRegister = NewObject<UActorDescContainerInstance>(this, UActorDescContainerInstance::StaticClass(), NAME_None, RF_Transient);
		
		OnActorDescContainerInstancePreInitialize.ExecuteIfBound(InitParams, ContainerInstanceToRegister);

		ContainerInstanceToRegister->Initialize(InitParams);
			
		AddContainer(ContainerInstanceToRegister);

		if (ActorDescContainerInstance && EditorHash)
		{
			check(ActorDescContainerInstance->IsInitialized());
			// When world partition is already initialized, it's safe to call InitializeActorDescContainerEditorStreaming as the DataLayerManager is created
			InitializeActorDescContainerEditorStreaming(ContainerInstanceToRegister);
		}

		OnActorDescContainerInstanceRegistered.Broadcast(ContainerInstanceToRegister);

		return ContainerInstanceToRegister;
	}
	
	return nullptr;
}

bool UWorldPartition::UnregisterActorDescContainerInstance(UActorDescContainerInstance* InActorDescContainerInstance)
{
	if (Contains(InActorDescContainerInstance->GetContainerPackage()))
	{
		TArray<FGuid> ActorGuids;
		for (UActorDescContainerInstance::TIterator<> It(InActorDescContainerInstance); It; ++It)
		{
			FWorldPartitionHandle ActorHandle(this, It->GetGuid());
			if (ActorHandle.IsValid())
			{
				ActorGuids.Add(It->GetGuid());
			}
		}

		UnpinActors(ActorGuids);

		if (ForceLoadedActors)
		{
			ForceLoadedActors->RemoveActors(ActorGuids);
		}

		OnActorDescContainerInstanceUnregistered.Broadcast(InActorDescContainerInstance);

		// Un-hashing needs to be done for an initialized container instance that was previously hashed (even if WorldPartition is being uninitialized)
		if (ShouldHashUnhashActorDescInstances() && (IsInitialized() || InActorDescContainerInstance->IsInitialized()))
		{
			for (UActorDescContainerInstance::TIterator<> It(InActorDescContainerInstance); It; ++It)
			{
				UnhashActorDescInstance(*It);
			}
		}

		InActorDescContainerInstance->Uninitialize();

		verify(RemoveContainer(InActorDescContainerInstance));

		return true;
	}

	return false;
}

void UWorldPartition::UninitializeActorDescContainers()
{
	for (UActorDescContainerInstance* ContainerInstance : ActorDescContainerInstanceCollection)
	{
		ContainerInstance->Uninitialize();
	}

	Empty();
}

void UWorldPartition::PinActors(const TArray<FGuid>& ActorGuids)
{
	if (PinnedActors)
	{
		PinnedActors->AddActors(ActorGuids);
	}
}

void UWorldPartition::UnpinActors(const TArray<FGuid>& ActorGuids)
{
	if (PinnedActors)
	{
		PinnedActors->RemoveActors(ActorGuids);
	}
}

bool UWorldPartition::IsActorPinned(const FGuid& ActorGuid) const
{
	if (PinnedActors)
	{
		return PinnedActors->ContainsActor(ActorGuid);
	}
	return false;
}

void UWorldPartition::LoadLastLoadedRegions(const TArray<FBox>& EditorLastLoadedRegions)
{
	for (const FBox& EditorLastLoadedRegion : EditorLastLoadedRegions)
	{
		if (EditorLastLoadedRegion.IsValid)
		{
			UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = CreateEditorLoaderAdapter<FLoaderAdapterShape>(World, EditorLastLoadedRegion, TEXT("Last Loaded Region"));
			IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter();
			check(LoaderAdapter);
			LoaderAdapter->SetUserCreated(true);
			LoaderAdapter->Load();
		}
	}
}

void UWorldPartition::LoadLastLoadedRegions()
{
	check(IsMainWorldPartition());

	TArray<FBox> EditorLastLoadedRegions = GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetEditorLoadedRegions(World);
	LoadLastLoadedRegions(EditorLastLoadedRegions);

	TArray<FName> EditorLoadedLocationVolumes = GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetEditorLoadedLocationVolumes(World);
	for (const FName& EditorLoadedLocationVolume : EditorLoadedLocationVolumes)
	{
		if (ALocationVolume* LocationVolume = FindObject<ALocationVolume>(World->PersistentLevel, *EditorLoadedLocationVolume.ToString()))
		{
			LocationVolume->bIsAutoLoad = true;
		}
	}
}

void UWorldPartition::OnLoaderAdapterStateChanged(IWorldPartitionActorLoaderInterface::ILoaderAdapter* InLoaderAdapter)
{
	if (InLoaderAdapter->GetUserCreated())
	{
		NumUserCreatedLoadedRegions += InLoaderAdapter->IsLoaded() ? 1 : -1;
	}

	LoaderAdapterStateChanged.Broadcast(InLoaderAdapter);
}

void UWorldPartition::OnWorldRenamed(UWorld* RenamedWorld)
{
	if (GetWorld() == RenamedWorld)
	{
		ActorDescContainerInstance->SetContainerPackage(GetWorld()->GetPackage()->GetFName());
	}
}

void UWorldPartition::RemapSoftObjectPath(FSoftObjectPath& ObjectPath) const
{
	if (StreamingPolicy)
	{
		StreamingPolicy->RemapSoftObjectPath(ObjectPath);
	}
}

bool UWorldPartition::ConvertContainerPathToEditorPath(const FActorContainerID& InContainerID, const FSoftObjectPath& InPath, FSoftObjectPath& OutPath) const
{
	return StreamingPolicy ? StreamingPolicy->ConvertContainerPathToEditorPath(InContainerID, InPath, OutPath) : false;
}

FBox UWorldPartition::GetEditorWorldBounds() const
{
	check(EditorHash);

	if (IsStreamingEnabled())
	{
		const FBox EditorWorldBounds = EditorHash->GetEditorWorldBounds();
		
		if (EditorWorldBounds.IsValid)
		{
			return EditorWorldBounds;
		}
	}

	return EditorHash->GetNonSpatialBounds();
}

FBox UWorldPartition::GetRuntimeWorldBounds() const
{
	check(EditorHash);

	if (IsStreamingEnabled())
	{
		const FBox RuntimeWorldBounds = EditorHash->GetRuntimeWorldBounds();
		
		if (RuntimeWorldBounds.IsValid)
		{
			return RuntimeWorldBounds;
		}
	}

	return EditorHash->GetNonSpatialBounds();
}
#endif

#undef LOCTEXT_NAMESPACE

