// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraWorldManager.h"
#include "NiagaraModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraTypes.h"
#include "NiagaraEvents.h"
#include "NiagaraSettings.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystemInstance.h"
#include "Scalability.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "EngineModule.h"
#include "NiagaraStats.h"
#include "NiagaraComponentPool.h"
#include "NiagaraComponent.h"
#include "NiagaraEffectType.h"
#include "NiagaraDebugHud.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Particles/FXBudget.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraCullProxyComponent.h"
#include "GameDelegates.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraWorldManager)

#if WITH_EDITORONLY_DATA
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Niagara Manager Update Scalability Managers [GT]"), STAT_UpdateScalabilityManagers, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara Manager Tick [GT]"), STAT_NiagaraWorldManTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara Manager Wait On Render [GT]"), STAT_NiagaraWorldManWaitOnRender, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara Manager Wait Pre Garbage Collect [GT]"), STAT_NiagaraWorldManWaitPreGC, STATGROUP_Niagara); 
DECLARE_CYCLE_STAT(TEXT("Niagara Manager Refresh Owner Allows Scalability"), STAT_NiagaraWorldManRefreshOwnerAllowsScalability, STATGROUP_Niagara);

static int GNiagaraAllowAsyncWorkToEndOfFrame = 1;
static FAutoConsoleVariableRef CVarNiagaraAllowAsyncWorkToEndOfFrame(
	TEXT("fx.Niagara.AllowAsyncWorkToEndOfFrame"),
	GNiagaraAllowAsyncWorkToEndOfFrame,
	TEXT("Allow async work to continue until the end of the frame, if false it will complete within the tick group it's started in."),
	ECVF_Default
); 

static int GNiagaraWaitOnPreGC = 1;
static FAutoConsoleVariableRef CVarNiagaraWaitOnPreGC(
	TEXT("fx.Niagara.WaitOnPreGC"),
	GNiagaraWaitOnPreGC,
	TEXT("Toggles whether Niagara will wait for all async tasks to complete before any GC calls."),
	ECVF_Default
);

static int GNiagaraSpawnPerTickGroup = 1;
static FAutoConsoleVariableRef CVarNiagaraSpawnPerTickGroup(
	TEXT("fx.Niagara.WorldManager.SpawnPerTickGroup"),
	GNiagaraSpawnPerTickGroup,
	TEXT("Will attempt to spawn new systems earlier (default enabled)."),
	ECVF_Default
);

int GNigaraAllowPrimedPools = 1;
static FAutoConsoleVariableRef CVarNigaraAllowPrimedPools(
	TEXT("fx.Niagara.AllowPrimedPools"),
	GNigaraAllowPrimedPools,
	TEXT("Allow Niagara pools to be primed."),
	ECVF_Default
);

static int32 GbAllowVisibilityCullingForDynamicBounds = 1;
static FAutoConsoleVariableRef CVarAllowVisibilityCullingForDynamicBounds(
	TEXT("fx.Niagara.AllowVisibilityCullingForDynamicBounds"),
	GbAllowVisibilityCullingForDynamicBounds,
	TEXT("Allow async work to continue until the end of the frame, if false it will complete within the tick group it's started in."),
	ECVF_Default
);

FAutoConsoleCommandWithWorld DumpNiagaraWorldManagerCommand(
	TEXT("DumpNiagaraWorldManager"),
	TEXT("Dump Information About the Niagara World Manager Contents"),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld* World)
		{
			FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World);
			if (WorldManager != nullptr && GLog != nullptr)
			{
				WorldManager->DumpDetails(*GLog);
			}
		}
	)
);

static int GEnableNiagaraVisCulling = 1;
static FAutoConsoleVariableRef CVarEnableNiagaraVisCulling(
	TEXT("fx.Niagara.Scalability.VisibilityCulling"),
	GEnableNiagaraVisCulling,
	TEXT("When non-zero, high level scalability culling based on visibility is enabled."),
	ECVF_Default
);

static int GEnableNiagaraDistanceCulling = 1;
static FAutoConsoleVariableRef CVarEnableNiagaraDistanceCulling(
	TEXT("fx.Niagara.Scalability.DistanceCulling"),
	GEnableNiagaraDistanceCulling,
	TEXT("When non-zero, high level scalability culling based on distance is enabled."),
	ECVF_Default
);

static int GEnableNiagaraInstanceCountCulling = 1;
static FAutoConsoleVariableRef CVarEnableNiagaraInstanceCountCulling(
	TEXT("fx.Niagara.Scalability.InstanceCountCulling"),
	GEnableNiagaraInstanceCountCulling,
	TEXT("When non-zero, high level scalability culling based on instance count is enabled."),
	ECVF_Default
);

static int GEnableNiagaraGlobalBudgetCulling = 1;
static FAutoConsoleVariableRef CVarEnableNiagaraGlobalBudgetCulling(
	TEXT("fx.Niagara.Scalability.GlobalBudgetCulling"),
	GEnableNiagaraGlobalBudgetCulling,
	TEXT("When non-zero, high level scalability culling based on global time budget is enabled."),
	ECVF_Default
);

float GWorldLoopTime = 0.0f;
static FAutoConsoleVariableRef CVarWorldLoopTime(
	TEXT("fx.Niagara.Debug.GlobalLoopTime"),
	GWorldLoopTime,
	TEXT("If > 0 all Niagara FX will reset every N seconds. \n"),
	ECVF_Default
);

int GNiagaraAllowCullProxies = 1;
static FAutoConsoleVariableRef CVarAllowCullProxies(
	TEXT("fx.Niagara.AllowCullProxies"),
	GNiagaraAllowCullProxies,
	TEXT("Toggles whether Niagara will use Cull Proxy systems in place of systems culled by scalability."),
	ECVF_Default
);

float GNiagaraCSVSplitTime = 180.0f;
static FAutoConsoleVariableRef CVarCSVSplitTime(
	TEXT("fx.Niagara.CSVSplitTime"),
	GNiagaraCSVSplitTime,
	TEXT("Length of Niagara's split time events passed to the CSV profiler. There are used to give check more confined stat averages."),
	ECVF_Default
);

FAutoConsoleCommandWithWorldAndArgs GCmdNiagaraPlaybackMode(
	TEXT("fx.Niagara.Debug.PlaybackMode"),
	TEXT("Set playback mode\n")
	TEXT("0 - Play\n")
	TEXT("1 - Paused\n")
	TEXT("2 - Step\n"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if ( FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World) )
			{
				if (Args.Num() != 1)
				{
					UE_LOG(LogNiagara, Log, TEXT("fx.Niagara.Debug.PlaybackMode %d"), (int32)WorldManager->GetDebugPlaybackMode());
				}
				else
				{
					const ENiagaraDebugPlaybackMode PlaybackMode = FMath::Clamp((ENiagaraDebugPlaybackMode)FCString::Atoi(*Args[0]), ENiagaraDebugPlaybackMode::Play, ENiagaraDebugPlaybackMode::Step);
					WorldManager->SetDebugPlaybackMode(PlaybackMode);
				}
			}
		}
	)
);

FAutoConsoleCommandWithWorldAndArgs GCmdNiagaraPlaybackRate(
	TEXT("fx.Niagara.Debug.PlaybackRate"),
	TEXT("Set playback rate\n"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if ( FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World) )
			{
				if (Args.Num() != 1)
				{
					UE_LOG(LogNiagara, Log, TEXT("fx.Niagara.Debug.PlaybackRate %5.2f"), (int32)WorldManager->GetDebugPlaybackRate());
				}
				else
				{
					const float PlaybackRate = FCString::Atof(*Args[0]);
					WorldManager->SetDebugPlaybackRate(PlaybackRate);
				}
			}
		}
	)
);

FAutoConsoleCommandWithWorldAndArgs GCmdNiagaraScalabilityCullingMode(
	TEXT("fx.Niagara.Scalability.CullingMode"),
	TEXT("Set scalability culling mode\n")
	TEXT("0 - Enabled. Culling is enabled as normal.\n")
	TEXT("1 - Paused. No culling will occur but FX will still be tracked internally so culling can be resumed correctly later.\n")
	TEXT("2 - Disabled. No culling will occur and no FX will be tracked. Culling may not work correctly for some FX if enabled again after this.\n"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World))
			{
				if (Args.Num() != 1 || Args[0].IsNumeric() == false)
				{
					UE_LOG(LogNiagara, Log, TEXT("fx.Niagara.ScalabililityCullingMode %d"), (int32)WorldManager->GetScalabilityCullingMode());
					UE_LOG(LogNiagara, Log, TEXT("Set scalability culling mode"));
					UE_LOG(LogNiagara, Log, TEXT("0 - Enabled. Culling is enabled as normal."));
					UE_LOG(LogNiagara, Log, TEXT("1 - Paused. No culling will occur but FX will still be tracked internally so culling can be resumed correctly later."));
					UE_LOG(LogNiagara, Log, TEXT("2 - Disabled. No culling will occur and no FX will be tracked. Culling may not work correctly for some FX if enabled again after this."));
				}
				else
				{
					const ENiagaraScalabilityCullingMode CullingMode = FMath::Clamp((ENiagaraScalabilityCullingMode)FCString::Atoi(*Args[0]), ENiagaraScalabilityCullingMode::Enabled, ENiagaraScalabilityCullingMode::Disabled);
					WorldManager->SetScalabilityCullingMode(CullingMode);
				}
			}
			else
			{
				UE_LOG(LogNiagara, Warning, TEXT("Cannot set Niagara Scalability Culling Mode on a null world."));
			}
		}
	)
);

FDelegateHandle FNiagaraWorldManager::OnWorldInitHandle;
FDelegateHandle FNiagaraWorldManager::OnWorldCleanupHandle;
FDelegateHandle FNiagaraWorldManager::OnPostWorldCleanupHandle;
FDelegateHandle FNiagaraWorldManager::OnPreWorldFinishDestroyHandle;
FDelegateHandle FNiagaraWorldManager::OnWorldBeginTearDownHandle;
FDelegateHandle FNiagaraWorldManager::TickWorldHandle;
FDelegateHandle FNiagaraWorldManager::OnWorldPreSendAllEndOfFrameUpdatesHandle;
FDelegateHandle FNiagaraWorldManager::PreGCHandle;
FDelegateHandle FNiagaraWorldManager::PostReachabilityAnalysisHandle;
FDelegateHandle FNiagaraWorldManager::PostGCHandle;
FDelegateHandle FNiagaraWorldManager::PreGCBeginDestroyHandle;
FDelegateHandle FNiagaraWorldManager::ViewTargetChangedHandle;
TMap<class UWorld*, class FNiagaraWorldManager*> FNiagaraWorldManager::WorldManagers;

namespace FNiagaraUtilities
{
	int GetNiagaraTickGroup(ETickingGroup TickGroup)
	{
		const int ActualTickGroup = FMath::Clamp(TickGroup - NiagaraFirstTickGroup, 0, NiagaraNumTickGroups - 1);
		return ActualTickGroup;
	}
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraWorldManagerTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(Owner);
	Owner->Tick(TickGroup, DeltaTime, TickType, CurrentThread, MyCompletionGraphEvent);
}

FString FNiagaraWorldManagerTickFunction::DiagnosticMessage()
{
	static const UEnum* EnumType = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Engine.ETickingGroup"));

	return TEXT("FParticleSystemManager::Tick(") + EnumType->GetNameStringByIndex(static_cast<uint32>(TickGroup)) + TEXT(")");
}

FName FNiagaraWorldManagerTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("ParticleSystemManager"));
}

//////////////////////////////////////////////////////////////////////////

ENiagaraScalabilityCullingMode FNiagaraWorldManager::ScalabilityCullingMode = ENiagaraScalabilityCullingMode::Enabled;

void FNiagaraWorldManager::SetScalabilityCullingMode(ENiagaraScalabilityCullingMode NewMode) 
{
	//also reset fx budgets on state change.
	if (ScalabilityCullingMode != NewMode)
	{
		FFXBudget::Reset();
		ScalabilityCullingMode = NewMode;

		auto UpdateScalability = [](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.UpdateScalabilityManagers(0.01f, false);
		};
		ForAllWorldManagers(UpdateScalability);
	}
}


FNiagaraWorldManager::FNiagaraWorldManager()
	: World(nullptr)
	, ActiveNiagaraTickGroup(-1)
	, bAppHasFocus(true)
{
}

void FNiagaraWorldManager::Init(UWorld* InWorld)
{
	// Reset variables that are not reset during OnWorldCleanup; Init/OnWorldCleanup can be called multiple times
	ComponentPool = nullptr; // Discard the existing ComponentPool
	bPoolIsPrimed = false;
	ActiveNiagaraTickGroup = -1;
	bAppHasFocus = true;
	bIsTearingDown = false;
	WorldLoopTime = 0.0f;
	RequestedDebugPlaybackMode = ENiagaraDebugPlaybackMode::Play;
	DebugPlaybackMode = ENiagaraDebugPlaybackMode::Play;
	DebugPlaybackRate = 1.0f;

	World = InWorld;
	for (int32 TickGroup = 0; TickGroup < NiagaraNumTickGroups; ++TickGroup)
	{
		FNiagaraWorldManagerTickFunction& TickFunc = TickFunctions[TickGroup];
		TickFunc.TickGroup = ETickingGroup(NiagaraFirstTickGroup + TickGroup);
		TickFunc.EndTickGroup = GNiagaraAllowAsyncWorkToEndOfFrame ? TG_LastDemotable : (ETickingGroup)TickFunc.TickGroup;
		TickFunc.bCanEverTick = true;
		TickFunc.bStartWithTickEnabled = true;
		TickFunc.bAllowTickOnDedicatedServer = false;
		TickFunc.bHighPriority = true;
		TickFunc.Owner = this;
		TickFunc.RegisterTickFunction(InWorld->PersistentLevel);
	}

	ComponentPool = NewObject<UNiagaraComponentPool>();

	//Ideally we'd do this here but it's too early in the init process and the world does not have a Scene yet.
	//Possibly a later hook we can use.
	//PrimePoolForAllSystems();

#if WITH_NIAGARA_DEBUGGER
	NiagaraDebugHud.Reset(new FNiagaraDebugHud(World));
#endif
}

FNiagaraWorldManager::~FNiagaraWorldManager()
{
	OnWorldCleanup(true, true);
}

FNiagaraWorldManager* FNiagaraWorldManager::Get(const UWorld* World)
{
	FNiagaraWorldManager** OutWorld = WorldManagers.Find(World);
	if (OutWorld == nullptr)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Calling FNiagaraWorldManager::Get \"%s\", but Niagara has never encountered this world before. "
			" This means that WorldInit never happened. This may happen in some edge cases in the editor, like saving invisible child levels, "
			"in which case the calling context needs to be safe against this returning nullptr."), World ? *World->GetName() : TEXT("nullptr"));
		return nullptr;
	}
	return *OutWorld;
}

void FNiagaraWorldManager::OnStartup()
{
	OnWorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&FNiagaraWorldManager::OnWorldInit);
	OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&FNiagaraWorldManager::OnWorldCleanup);
	OnPostWorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddStatic(&FNiagaraWorldManager::OnPostWorldCleanup);
	OnPreWorldFinishDestroyHandle = FWorldDelegates::OnPreWorldFinishDestroy.AddStatic(&FNiagaraWorldManager::OnPreWorldFinishDestroy);
	OnWorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddStatic(&FNiagaraWorldManager::OnWorldBeginTearDown);
	TickWorldHandle = FWorldDelegates::OnWorldPostActorTick.AddStatic(&FNiagaraWorldManager::TickWorld);
	OnWorldPreSendAllEndOfFrameUpdatesHandle = FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates.AddLambda(
		[](UWorld* InWorld)
		{
			if ( FNiagaraWorldManager* FoundManager = WorldManagers.FindRef(InWorld) )
			{
				FoundManager->PreSendAllEndOfFrameUpdates();
			}
		}
	);

	PreGCHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddStatic(&FNiagaraWorldManager::OnPreGarbageCollect);
	PostReachabilityAnalysisHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddStatic(&FNiagaraWorldManager::OnPostReachabilityAnalysis);
	PostGCHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic(&FNiagaraWorldManager::OnPostGarbageCollect);
	PreGCBeginDestroyHandle = FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.AddStatic(&FNiagaraWorldManager::OnPreGarbageCollectBeginDestroy);

	if ( FApp::CanEverRender() )
	{
		ViewTargetChangedHandle = FGameDelegates::Get().GetViewTargetChangedDelegate().AddLambda(
			[](APlayerController* PC, AActor* OldTarget, AActor* NewTarget)
			{
				OnRefreshOwnerAllowsScalability();
			}
		);
	}
}

void FNiagaraWorldManager::OnShutdown()
{
	FWorldDelegates::OnPreWorldInitialization.Remove(OnWorldInitHandle);
	FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);
	FWorldDelegates::OnPostWorldCleanup.Remove(OnPostWorldCleanupHandle);
	FWorldDelegates::OnPreWorldFinishDestroy.Remove(OnPreWorldFinishDestroyHandle);
	FWorldDelegates::OnWorldBeginTearDown.Remove(OnWorldBeginTearDownHandle);
	FWorldDelegates::OnWorldPostActorTick.Remove(TickWorldHandle);
	FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates.Remove(OnWorldPreSendAllEndOfFrameUpdatesHandle);

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGCHandle);
	FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(PostReachabilityAnalysisHandle);
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGCHandle);
	FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.Remove(PreGCBeginDestroyHandle);
	
	if ( ViewTargetChangedHandle.IsValid() )
	{
		FGameDelegates::Get().GetViewTargetChangedDelegate().Remove(ViewTargetChangedHandle);
	}

	//Should have cleared up all world managers by now.
	check(WorldManagers.Num() == 0);
	for (TPair<UWorld*, FNiagaraWorldManager*> Pair : WorldManagers)
	{
		delete Pair.Value;
		Pair.Value = nullptr;
	}
}

void FNiagaraWorldManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	// World doesn't need to be added to the reference list. It will be handled via OnWorldInit & OnWorldCleanup & OnPreWorldFinishDestroy in INiagaraModule

	Collector.AddReferencedObjects(ParameterCollections);
	Collector.AddReferencedObject(ComponentPool);
	for (auto& Pair : ScalabilityManagers)
	{
		UNiagaraEffectType* EffectType = Pair.Key;
		FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;

		Collector.AddReferencedObject(EffectType);
		ScalabilityMan.AddReferencedObjects(Collector);
	}

	Collector.AddReferencedObjects(CullProxyMap);
}

FString FNiagaraWorldManager::GetReferencerName() const
{
	return TEXT("FNiagaraWorldManager");
}

UNiagaraCullProxyComponent* FNiagaraWorldManager::GetCullProxy(UNiagaraComponent* Component)
{
	UNiagaraSystem* System = Component->GetAsset();
	if (ensure(System) && GNiagaraAllowCullProxies)
	{
		UNiagaraCullProxyComponent*& CullProxy = CullProxyMap.FindOrAdd(System);

		if (CullProxy == nullptr)
		{
			CullProxy = NewObject<UNiagaraCullProxyComponent>(GetWorld());
			CullProxy->SetAsset(System);
			CullProxy->SetAllowScalability(false);

			CullProxy->bAutoActivate = true;
			CullProxy->SetAutoDestroy(false);
			CullProxy->bAllowAnyoneToDestroyMe = true;
			CullProxy->RegisterComponentWithWorld(World);
			CullProxy->SetAbsolute(true, true, true);
			CullProxy->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
			CullProxy->SetRelativeScale3D(FVector::OneVector);

			CullProxy->Activate(true);
			CullProxy->SetPaused(true);

			//Override the LODDistance to just below the max so the system is as cheap for simulation and rendering as possible. 
			float MaxDist = System->GetScalabilitySettings().MaxDistance;
			CullProxy->SetPreviewLODDistance(true, MaxDist * 0.999f, MaxDist);

			//Have to copy user parameters from first requesting component so we at least have some valid values.
			//Though this could be somewhat dangerous with DIs etc.
			Component->GetOverrideParameters().CopyParametersTo(CullProxy->GetOverrideParameters(), false, FNiagaraParameterStore::EDataInterfaceCopyMethod::Value);
		}
	
		return CullProxy;
	}
	return nullptr;
}

UNiagaraParameterCollectionInstance* FNiagaraWorldManager::GetParameterCollection(UNiagaraParameterCollection* Collection)
{
	if (!Collection || bIsTearingDown)
	{
		return nullptr;
	}

	UNiagaraParameterCollectionInstance** OverrideInst = ParameterCollections.Find(Collection);
	if (!OverrideInst)
	{
		UNiagaraParameterCollectionInstance* DefaultInstance = Collection->GetDefaultInstance();
		OverrideInst = &ParameterCollections.Add(Collection);
		*OverrideInst = CastChecked<UNiagaraParameterCollectionInstance>(StaticDuplicateObject(DefaultInstance, World));
#if WITH_EDITORONLY_DATA
		//Bind to the default instance so that changes to the collection propagate through.
		DefaultInstance->GetParameterStore().Bind(&(*OverrideInst)->GetParameterStore());
#endif

		(*OverrideInst)->Bind(World);
	}

	check(OverrideInst && *OverrideInst);
	return *OverrideInst;
}

void FNiagaraWorldManager::CleanupParameterCollections()
{
#if WITH_EDITOR
	for (TPair<UNiagaraParameterCollection*, UNiagaraParameterCollectionInstance*> CollectionInstPair : ParameterCollections)
	{
		UNiagaraParameterCollection* Collection = CollectionInstPair.Key;
		UNiagaraParameterCollectionInstance* CollectionInst = CollectionInstPair.Value;
		//Ensure that the default instance is not bound to the override.
		UNiagaraParameterCollectionInstance* DefaultInst = Collection->GetDefaultInstance();
		DefaultInst->GetParameterStore().Unbind(&CollectionInst->GetParameterStore());
	}
#endif
	ParameterCollections.Empty();
}

FNiagaraSystemSimulationRef FNiagaraWorldManager::GetSystemSimulation(ETickingGroup TickGroup, UNiagaraSystem* System)
{
	LLM_SCOPE(ELLMTag::Niagara);

	int32 ActualTickGroup = FNiagaraUtilities::GetNiagaraTickGroup(TickGroup);
	if (ActiveNiagaraTickGroup == ActualTickGroup)
	{
		int32 DemotedTickGroup = FNiagaraUtilities::GetNiagaraTickGroup((ETickingGroup)(TickGroup + 1));
		ActualTickGroup = DemotedTickGroup == ActualTickGroup ? 0 : DemotedTickGroup;
	}

	FNiagaraSystemSimulationRef* SimPtr = SystemSimulations[ActualTickGroup].Find(System);
	if (SimPtr != nullptr)
	{
		return *SimPtr;
	}
	
	FNiagaraSystemSimulationRef Sim = MakeShared<FNiagaraSystemSimulation, ESPMode::ThreadSafe>();
	SystemSimulations[ActualTickGroup].Add(System, Sim);
	Sim->Init(System, World, false, TickGroup);

#if WITH_EDITOR
	System->OnSystemPostEditChange().AddRaw(this, &FNiagaraWorldManager::OnSystemPostChange);
#endif

	return Sim;
}

void FNiagaraWorldManager::DestroySystemSimulation(UNiagaraSystem* System)
{
	for ( int TG=0; TG < NiagaraNumTickGroups; ++TG )
	{
		FNiagaraSystemSimulationRef* Simulation = SystemSimulations[TG].Find(System);
		if (Simulation != nullptr)
		{
			SimulationsWithPostActorWork.Remove(*Simulation);
			SimulationsWithEndOfFrameWait.Remove(*Simulation);

			(*Simulation)->Destroy();
			SystemSimulations[TG].Remove(System);
		}
	}
	ComponentPool->RemoveComponentsBySystem(System);

#if WITH_EDITOR
	System->OnSystemPostEditChange().RemoveAll(this);
#endif
}

void FNiagaraWorldManager::DestroySystemInstance(FNiagaraSystemInstancePtr& InPtr)
{
	check(IsInGameThread());
	check(InPtr != nullptr);
	DeferredDeletionQueue.Emplace(MoveTemp(InPtr));
}

#if WITH_EDITOR
void FNiagaraWorldManager::OnSystemPostChange(UNiagaraSystem* System)
{
	if (UNiagaraEffectType* EffectType = System->GetEffectType())
	{
		if (FNiagaraScalabilityManager* ScalabilityMan = ScalabilityManagers.Find(EffectType))
		{
			ScalabilityMan->OnSystemPostChange(System);
		}
	}
}
#endif//WITH_EDITOR

void FNiagaraWorldManager::OnComputeDispatchInterfaceDestroyed_Internal(FNiagaraGpuComputeDispatchInterface* InComputeDispatchInterface)
{
	// Process the deferred deletion queue before deleting the ComputeDispatchInterface of this world.
	// This is required because the ComputeDispatchInterface is accessed in FNiagaraEmitterInstance::~FNiagaraEmitterInstance
	if (FNiagaraGpuComputeDispatchInterface::Get(World) == InComputeDispatchInterface)
	{
		DeferredDeletionQueue.Empty();
	}
}

void FNiagaraWorldManager::OnWorldCleanup(bool bSessionEnded, bool bCleanupResources)
{
	DeferredMethods.ExecuteAndClear();
	ComponentPool->Cleanup(World);

	for (int TG = 0; TG < NiagaraNumTickGroups; ++TG)
	{
		for (TPair<UNiagaraSystem*, FNiagaraSystemSimulationRef>& SimPair : SystemSimulations[TG])
		{
#if WITH_EDITOR
			SimPair.Key->OnSystemPostEditChange().RemoveAll(this);
#endif
			SimPair.Value->Destroy();
		}
		SystemSimulations[TG].Empty();
	}
	CleanupParameterCollections();

	SimulationsWithPostActorWork.Empty();
	SimulationsWithEndOfFrameWait.Empty();

	DeferredDeletionQueue.Empty();

	ScalabilityManagers.Empty();

	CullProxyMap.Empty();
	DIGeneratedData.Empty();
}

void FNiagaraWorldManager::OnPostWorldCleanup(bool bSessionEnded, bool bCleanupResources)
{
	ComponentPool->Cleanup(World);
}

void FNiagaraWorldManager::PreGarbageCollect()
{
	if (GNiagaraWaitOnPreGC)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraWorldManWaitPreGC);
		// We must wait for system simulation & instance async ticks to complete before garbage collection can start
		// The reason for this is that our async ticks could be referencing GC objects, i.e. skel meshes, etc, and we don't want them to become unreachable while we are potentially using them
		for (int TG = 0; TG < NiagaraNumTickGroups; ++TG)
		{
			for (TPair<UNiagaraSystem*, FNiagaraSystemSimulationRef>& SimPair : SystemSimulations[TG])
			{
				SimPair.Value->WaitForInstancesTickComplete();
			}
		}
	}
}

void FNiagaraWorldManager::PostReachabilityAnalysis()
{
	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		ComponentIt->GetOverrideParameters().MarkUObjectsDirty();
	}
}


void FNiagaraWorldManager::PostGarbageCollect()
{
	//Clear out and scalability managers who's EffectTypes have been GCd.
	while (ScalabilityManagers.Remove(nullptr)) {}
}

void FNiagaraWorldManager::PreGarbageCollectBeginDestroy()
{
	//Clear out and scalability managers who's EffectTypes have been GCd.
	while (ScalabilityManagers.Remove(nullptr)) {}

	//Also tell the scalability managers to clear out any references the GC has nulled.
	for (auto& Pair : ScalabilityManagers)
	{
		FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;
		UNiagaraEffectType* EffectType = Pair.Key;
		ScalabilityMan.PreGarbageCollectBeginDestroy();
	}
}

void FNiagaraWorldManager::RefreshOwnerAllowsScalability()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraWorldManRefreshOwnerAllowsScalability);

	//Refresh all component's owner allows scalability flags and register/unreister with the manager accordingly.
	for (TObjectIterator<UNiagaraComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		ComponentIt->ResolveOwnerAllowsScalability();
	}

	//Force a full refresh of scalability state next tick.
	for (auto& Pair : ScalabilityManagers)
	{
		UNiagaraEffectType* EffectType = Pair.Key;
		FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;
		ScalabilityMan.OnRefreshOwnerAllowsScalability();
	}
}

void FNiagaraWorldManager::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	FNiagaraWorldManager*& NewManager = WorldManagers.FindOrAdd(World);
	if (!NewManager)
	{
		NewManager = new FNiagaraWorldManager();
	}
	NewManager->Init(World);
}

void FNiagaraWorldManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	//Cleanup world manager contents but not the manager itself.
	FNiagaraWorldManager** Manager = WorldManagers.Find(World);
	if (Manager)
	{
		(*Manager)->OnWorldCleanup(bSessionEnded, bCleanupResources);
	}
}

void FNiagaraWorldManager::OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	//Cleanup world manager contents but not the manager itself.
	FNiagaraWorldManager** Manager = WorldManagers.Find(World);
	if (Manager)
	{
		(*Manager)->OnPostWorldCleanup(bSessionEnded, bCleanupResources);
	}
}

void FNiagaraWorldManager::OnPreWorldFinishDestroy(UWorld* World)
{
	FNiagaraWorldManager** Manager = WorldManagers.Find(World);
	if (Manager)
	{
		delete (*Manager);
		WorldManagers.Remove(World);
	}
}

void FNiagaraWorldManager::OnWorldBeginTearDown(UWorld* World)
{
	FNiagaraWorldManager** Manager = WorldManagers.Find(World);
	if (Manager)
	{
		(*Manager)->bIsTearingDown = true;
	}
// 	FNiagaraWorldManager** Manager = WorldManagers.Find(World);
// 	if (Manager)
// 	{
// 		delete (*Manager);
// 		WorldManagers.Remove(World);
// 	}
// 	FNiagaraWorldManager** Manager = WorldManagers.Find(World);
// 	if (Manager)
// 	{
// 		Manager->SystemSimulations
// 	}
}

void FNiagaraWorldManager::OnComputeDispatchInterfaceDestroyed(FNiagaraGpuComputeDispatchInterface* InComputeDispatchInterface)
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->OnComputeDispatchInterfaceDestroyed_Internal(InComputeDispatchInterface);
	}
}

void FNiagaraWorldManager::DestroyAllSystemSimulations(class UNiagaraSystem* System)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FNiagaraWorldManager::DestroyAllSystemSimulations);

	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->DestroySystemSimulation(System);
	}
}

void FNiagaraWorldManager::TickWorld(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	Get(World)->PostActorTick(DeltaSeconds);
}

void FNiagaraWorldManager::OnPreGarbageCollect()
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->PreGarbageCollect();
	}
}

void FNiagaraWorldManager::OnPostReachabilityAnalysis()
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->PostReachabilityAnalysis();
	}
}

void FNiagaraWorldManager::OnPostGarbageCollect()
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->PostGarbageCollect();
	}
}

void FNiagaraWorldManager::OnPreGarbageCollectBeginDestroy()
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->PreGarbageCollectBeginDestroy();
	}
}

void FNiagaraWorldManager::OnRefreshOwnerAllowsScalability()
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->RefreshOwnerAllowsScalability();
	}
}

void FNiagaraWorldManager::PostActorTick(float DeltaSeconds)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraWorldManTick);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NiagaraPostActorTick_GT);

	DeltaSeconds *= DebugPlaybackRate;

	// Update any systems with post actor work
	// - Instances that need to move to a higher tick group
	// - Instances that are pending spawn
	// - Instances that were spawned and we need to ensure the async tick is complete
	if (SimulationsWithPostActorWork.Num() > 0)
	{
		// Ensure completion of all systems
		for (int32 i = 0; i < SimulationsWithPostActorWork.Num(); ++i)
		{
			FNiagaraSystemSimulationRef& Simulation = SimulationsWithPostActorWork[i];
			if ( Simulation->IsValid() )
			{
				Simulation->WaitForInstancesTickComplete();
			}
		}

		// Update tick groups
		for (int32 i = 0; i < SimulationsWithPostActorWork.Num(); ++i)
		{
			FNiagaraSystemSimulationRef& Simulation = SimulationsWithPostActorWork[i];
			if (Simulation->IsValid())
			{
				Simulation->UpdateTickGroups_GameThread();
			}
		}

		// Execute spawning
		TArray<FNiagaraSystemSimulationRef> LocalSimulationsWithPostActorWork;
		Swap(SimulationsWithPostActorWork, LocalSimulationsWithPostActorWork);
		for (FNiagaraSystemSimulationRef& Simulation : LocalSimulationsWithPostActorWork)
		{
			if (Simulation->IsValid())
			{
				Simulation->Spawn_GameThread(DeltaSeconds, true);
			}
		}

		// Wait for any spawning that may be required to complete in PostActorTick
		for (int32 i = 0; i < SimulationsWithPostActorWork.Num(); ++i)
		{
			FNiagaraSystemSimulationRef& Simulation = SimulationsWithPostActorWork[i];
			if (Simulation->IsValid())
			{
				Simulation->WaitForInstancesTickComplete();
			}
		}
		SimulationsWithPostActorWork.Reset();
	}

	// Clear cached player view location list, it should never be used outside of the world tick
	CachedViewInfo.Reset();

	// Delete any instances that were pending deletion
	//-TODO: This could be done after each system sim has run
	DeferredDeletionQueue.Empty();

	// Update tick groups
	for (FNiagaraWorldManagerTickFunction& TickFunc : TickFunctions )
	{
		TickFunc.EndTickGroup = GNiagaraAllowAsyncWorkToEndOfFrame ? TG_LastDemotable : (ETickingGroup)TickFunc.TickGroup;
	}

#if WITH_NIAGARA_DEBUGGER
	// Tick debug HUD for the world
	if (NiagaraDebugHud != nullptr)
	{
		NiagaraDebugHud->GatherSystemInfo();
	}
#endif

	if ( DebugPlaybackMode == ENiagaraDebugPlaybackMode::Step )
	{
		RequestedDebugPlaybackMode = ENiagaraDebugPlaybackMode::Paused;
		DebugPlaybackMode = ENiagaraDebugPlaybackMode::Paused;
	}

	for (auto it = CullProxyMap.CreateIterator(); it; ++it)
	{
		UNiagaraSystem* System = it->Key;

		if (GNiagaraAllowCullProxies && it->Value && System->GetCullProxyMode() != ENiagaraCullProxyMode::None)
		{
			it->Value->TickCullProxy();
		}
		else
		{
			if (it->Value)
			{
				it->Value->DestroyComponent();
			}
			it.RemoveCurrent();
		}
	}

#if WITH_PARTICLE_PERF_CSV_STATS
	if (FCsvProfiler* CSVProfiler = FCsvProfiler::Get())
	{
		if (CSVProfiler->IsCapturing() && FParticlePerfStats::GetCSVStatsEnabled())
		{
			//Record custom events marking split times at set intervals. Allows us to generate summary tables for averages over shorter bursts.
			if (GNiagaraCSVSplitTime > 0.0f)
			{
				float WorldTime = World->GetTimeSeconds();
				float PrevWorldTime = WorldTime - DeltaSeconds;

				int32 CurrentSplitIdx = (int32)(WorldTime / GNiagaraCSVSplitTime);
				int32 PrevSplitIdx = (int32)(PrevWorldTime / GNiagaraCSVSplitTime);
				if (CurrentSplitIdx > PrevSplitIdx)
				{
					CSVProfiler->RecordEvent(CSV_CATEGORY_INDEX(Particles), FString::Printf(TEXT("SplitTime%d"), CurrentSplitIdx));
				}
			}

			for (auto& Pair : ScalabilityManagers)
			{
				FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;
				UNiagaraEffectType* EffectType = Pair.Key;
				check(EffectType);

				ScalabilityMan.CSVProfilerUpdate(CSVProfiler);
			}
		}
	}
#endif 
}

void FNiagaraWorldManager::PreSendAllEndOfFrameUpdates()
{
	if ( SimulationsWithPostActorWork.Num() > 0 )
	{
		for (const auto& Simulation : SimulationsWithPostActorWork)
		{
			if (Simulation->IsValid())
			{
				Simulation->WaitForInstancesTickComplete();
			}
		}
		// Note: We do not clear the SimulationsWithPostActorWork array as we just want to safely wait for any async work,
		//       they still require additional processing, i.e. Tick Group Changes / Spawning
	}

	for (const auto& Simulation : SimulationsWithEndOfFrameWait)
	{
		if (Simulation->IsValid())
		{
			Simulation->WaitForInstancesTickComplete();
		}
	}
	SimulationsWithEndOfFrameWait.Reset();
}

void FNiagaraWorldManager::MarkSimulationForPostActorWork(FNiagaraSystemSimulation* SystemSimulation)
{
	check(SystemSimulation != nullptr);
	check(!SystemSimulation->GetIsSolo());
	if ( !SimulationsWithPostActorWork.ContainsByPredicate([&](const FNiagaraSystemSimulationRef& Existing) { return &Existing.Get() == SystemSimulation; }) )
	{
		SimulationsWithPostActorWork.Add(SystemSimulation->AsShared());
	}
}

void FNiagaraWorldManager::MarkSimulationsForEndOfFrameWait(FNiagaraSystemSimulation* SystemSimulation)
{
	check(SystemSimulation);
	check(!SystemSimulation->GetIsSolo());
	if (!SimulationsWithEndOfFrameWait.ContainsByPredicate([&](const FNiagaraSystemSimulationRef& Existing) { return &Existing.Get() == SystemSimulation; }))
	{
		SimulationsWithEndOfFrameWait.Add(SystemSimulation->AsShared());
	}
}

bool FNiagaraWorldManager::PrepareCachedViewInfo(const APlayerController* PlayerController, FNiagaraCachedViewInfo& OutViewInfo)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (PlayerController && LocalPlayer && LocalPlayer->ViewportClient)
	{
		FSceneViewProjectionData ProjectionData;
		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, /*out*/ ProjectionData))
		{
			FVector POVLoc;
			FRotator POVRotation;
			PlayerController->GetPlayerViewPoint(POVLoc, POVRotation);
			FRotationTranslationMatrix ViewToWorld(POVRotation, POVLoc);

			FWorldCachedViewInfo WorldViewInfo;
			WorldViewInfo.ViewMatrix = FTranslationMatrix(-ProjectionData.ViewOrigin) * ProjectionData.ViewRotationMatrix;
			WorldViewInfo.ProjectionMatrix = ProjectionData.ProjectionMatrix;
			WorldViewInfo.ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
			WorldViewInfo.ViewToWorld = ViewToWorld;

			OutViewInfo.Init(WorldViewInfo);
			return true;
		}
	}

	return false;
}

void FNiagaraWorldManager::Tick(ETickingGroup TickGroup, float DeltaSeconds, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(TickGroup >= NiagaraFirstTickGroup && TickGroup <= NiagaraLastTickGroup);

	DeferredMethods.ExecuteAndClear();

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraWorldManTick);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);

	DeltaSeconds *= DebugPlaybackRate;

	// We do book keeping in the first tick group
	if ( TickGroup == NiagaraFirstTickGroup )
	{		
		// Update playback mode
		DebugPlaybackMode = RequestedDebugPlaybackMode;

		// Utility loop feature to trigger all systems to loop on a timer.
		if (GWorldLoopTime > 0.0f)
		{
			if (WorldLoopTime <= 0.0f)
			{
				WorldLoopTime = GWorldLoopTime;
				for (TObjectIterator<UNiagaraComponent> It; It; ++It)
				{
					if (UNiagaraComponent* Comp = *It)
					{
						if(Comp->GetWorld() == GetWorld())
						{
							Comp->ResetSystem();
						}
					}
				}
			}
			WorldLoopTime -= DeltaSeconds;
		}

		//Ensure the pools have been primed.
		//WorldInit is too early.
		if(!bPoolIsPrimed)
		{
			PrimePoolForAllSystems();
			bPoolIsPrimed = true;
		}

		FNiagaraSharedObject::FlushDeletionList();

#if WITH_EDITOR //PLATFORM_DESKTOP
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_Niagara_IsThisApplicationForeground);
			bAppHasFocus = FApp::HasFocus();
		}
#else
		bAppHasFocus = true;
#endif

		// If we are in paused don't do anything
		if (DebugPlaybackMode == ENiagaraDebugPlaybackMode::Paused)
		{
			return;
		}

		bool bUseWorldCachedViews = !World->GetPlayerControllerIterator();
#if WITH_EDITOR
		if (GCurrentLevelEditingViewportClient && (GCurrentLevelEditingViewportClient->GetWorld() == World))
		{
			bUseWorldCachedViews = true;
		}
#endif
		// Cache player view info for all system instances to access
		//-TODO: Do we need to do this per tick group?
		if (bUseWorldCachedViews)
		{
			for (int32 i = 0; i < World->CachedViewInfoRenderedLastFrame.Num(); ++i)
			{
				FWorldCachedViewInfo& WorldViewInfo = World->CachedViewInfoRenderedLastFrame[i];

				FNiagaraCachedViewInfo& ViewInfo = CachedViewInfo.AddDefaulted_GetRef();
				ViewInfo.Init(WorldViewInfo);
			}
		}
		else
		{
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PlayerController = Iterator->Get();
				if (PlayerController && PlayerController->IsLocalPlayerController())
				{
					FNiagaraCachedViewInfo& ViewInfo = CachedViewInfo.AddDefaulted_GetRef();
					
					const bool bIsValid = PrepareCachedViewInfo(PlayerController, ViewInfo);
					if (!bIsValid)
					{
						CachedViewInfo.RemoveAt(CachedViewInfo.Num() - 1);
					}
				}
			}
		}		

		UpdateScalabilityManagers(DeltaSeconds, false);

		//Tick our collections to push any changes to bound stores.
		//-TODO: Do we need to do this per tick group?
		for (TPair<UNiagaraParameterCollection*, UNiagaraParameterCollectionInstance*> CollectionInstPair : ParameterCollections)
		{
			check(CollectionInstPair.Value);
			CollectionInstPair.Value->Tick(World);
		}
	}

	// If we are in paused don't do anything
	if ( DebugPlaybackMode == ENiagaraDebugPlaybackMode::Paused )
	{
		return;
	}

	// Tick generated data
	for (auto& GeneratedData : DIGeneratedData)
	{
		GeneratedData.Value->Tick(TickGroup, DeltaSeconds);
	}

	// Now tick all system instances. 
	const int ActualTickGroup = FNiagaraUtilities::GetNiagaraTickGroup(TickGroup);

	ActiveNiagaraTickGroup = ActualTickGroup;

	TArray<UNiagaraSystem*, TInlineAllocator<4>> DeadSystems;
	for (TPair<UNiagaraSystem*, FNiagaraSystemSimulationRef>& SystemSim : SystemSimulations[ActualTickGroup])
	{
		FNiagaraSystemSimulation*  Sim = &SystemSim.Value.Get();

		if (Sim->IsValid())
		{
			Sim->Tick_GameThread(DeltaSeconds, MyCompletionGraphEvent);
		}
		else
		{
			DeadSystems.Add(SystemSim.Key);
		}
	}

	ActiveNiagaraTickGroup = -1;

	for (UNiagaraSystem* DeadSystem : DeadSystems)
	{
		SystemSimulations[ActualTickGroup].Remove(DeadSystem);
	}

	// Loop over all simulations that have been marked for post actor (i.e. ones whos TG is changing or have pending spawn systems)
	if (GNiagaraSpawnPerTickGroup && (SimulationsWithPostActorWork.Num() > 0))
	{
		//We update scalability managers here so that any new systems can be culled or setup with other scalability based parameters correctly for their spawn.
		UpdateScalabilityManagers(DeltaSeconds, true);

		QUICK_SCOPE_CYCLE_COUNTER(STAT_NiagaraSpawnPerTickGroup_GT);
		for (int32 i = 0; i < SimulationsWithPostActorWork.Num(); ++i)
		{
			const auto& Simulation = SimulationsWithPostActorWork[i];
			if (Simulation->IsValid() && (Simulation->GetTickGroup() < TickGroup))
			{
				Simulation->Spawn_GameThread(DeltaSeconds, false);
			}
		}
	}
}

void FNiagaraWorldManager::DumpDetails(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("=== FNiagaraWorldManager Dumping Detailed Information"));

	static const UEnum* TickingGroupEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Engine.ETickingGroup"));

	for ( int TG=0; TG < NiagaraNumTickGroups; ++TG )
	{
		if (SystemSimulations[TG].Num() == 0 )
		{
			continue;
		}

		Ar.Logf(TEXT("TickingGroup %s"), *TickingGroupEnum->GetNameStringByIndex(TG + NiagaraFirstTickGroup));

		for (TPair<UNiagaraSystem*, FNiagaraSystemSimulationRef>& SystemSim : SystemSimulations[TG])
		{
			FNiagaraSystemSimulation* Sim = &SystemSim.Value.Get();
			if ( !Sim->IsValid() )
			{
				continue;
			}

			Ar.Logf(TEXT("\tSimulation %s"), *Sim->GetSystem()->GetFullName());
			Sim->DumpTickInfo(Ar);
		}
	}
}

UWorld* FNiagaraWorldManager::GetWorld()
{
	return World;
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraWorldManager::UpdateScalabilityManagers(float DeltaSeconds, bool bNewSpawnsOnly)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateScalabilityManagers);

	for (auto& Pair : ScalabilityManagers)
	{
		FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;
		UNiagaraEffectType* EffectType = Pair.Key;
		check(EffectType);

		if (bNewSpawnsOnly)
		{
			ScalabilityMan.Update(this, DeltaSeconds, true);
		}
		else
		{
			ScalabilityMan.Update(this, DeltaSeconds, false);
		}
	}
}

void FNiagaraWorldManager::RegisterWithScalabilityManager(UNiagaraComponent* Component, UNiagaraEffectType* EffectType)
{
	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Disabled)
	{
		return;
	}

	check(Component);
	if ( EffectType )
	{
		FNiagaraScalabilityManager* ScalabilityManager = ScalabilityManagers.Find(EffectType);

		if (!ScalabilityManager)
		{
			ScalabilityManager = &ScalabilityManagers.Add(EffectType);
			ScalabilityManager->EffectType = EffectType;
		}

		ScalabilityManager->Register(Component);
	}
}

void FNiagaraWorldManager::UnregisterWithScalabilityManager(UNiagaraComponent* Component, UNiagaraEffectType* EffectType)
{
	check(Component);
	if ( EffectType )
	{
		//Possibly the manager has been GCd.
		if (FNiagaraScalabilityManager* ScalabilityManager = ScalabilityManagers.Find(EffectType))
		{
			ScalabilityManager->Unregister(Component);
		}
		else
		{
			//The component does this itself in unregister.
			//Component->ScalabilityManagerHandle = INDEX_NONE;
		}
	}
}

bool FNiagaraWorldManager::ShouldPreCull(UNiagaraSystem* System, UNiagaraComponent* Component)
{
	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Enabled && System)
	{
		if (UNiagaraEffectType* EffectType = System->GetEffectType())
		{
			if (CanPreCull(EffectType))
			{
				FNiagaraScalabilityState State;
				const FNiagaraSystemScalabilitySettings& ScalabilitySettings = System->GetScalabilitySettings();
				CalculateScalabilityState(System, ScalabilitySettings, EffectType, Component, true, FFXBudget::GetWorstAdjustedUsage(), State);
				return State.bCulled;
			}
		}
	}
	return false;
}

bool FNiagaraWorldManager::ShouldPreCull(UNiagaraSystem* System, FVector Location)
{
	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Enabled && System)
	{
		if (UNiagaraEffectType* EffectType = System->GetEffectType())
		{
			if (CanPreCull(EffectType))
			{
				FNiagaraScalabilityState State;
				const FNiagaraSystemScalabilitySettings& ScalabilitySettings = System->GetScalabilitySettings();
				CalculateScalabilityState(System, ScalabilitySettings, EffectType, Location, true, FFXBudget::GetWorstAdjustedUsage(), State);
				//TODO: Tell the debugger about recently PreCulled systems.
				return State.bCulled;
			}
		}
	}
	return false;
}

void FNiagaraWorldManager::CalculateScalabilityState(UNiagaraSystem* System, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, UNiagaraEffectType* EffectType, FVector Location, bool bIsPreCull, float WorstGlobalBudgetUse, FNiagaraScalabilityState& OutState)
{
	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Disabled)
	{
		return;
	}

	OutState.bCulled = false;

	DistanceCull(EffectType, ScalabilitySettings, Location, OutState);

	if (GEnableNiagaraVisCulling)
	{
		if (System->bFixedBounds)//We have no component in this path so we require fixed bounds for view based checks.
		{
			FBoxSphereBounds Bounds(System->GetFixedBounds());
			Bounds.Origin = Location;
			float TimeSinceRendered = 0.0f;
			ViewBasedCulling(EffectType, ScalabilitySettings, Bounds.GetSphere(), TimeSinceRendered, bIsPreCull, OutState);
		}
	}

	//If we have no significance handler there is no concept of relative significance for these systems so we can just pre cull if we go over the instance count.
	if (GEnableNiagaraInstanceCountCulling && bIsPreCull && EffectType->GetSignificanceHandler() == nullptr)
	{
		InstanceCountCull(EffectType, System, ScalabilitySettings, OutState);
	}

	//Cull if any of our budgets are exceeded.
	bool bEnabled = GEnableNiagaraGlobalBudgetCulling && FFXBudget::Enabled() && INiagaraModule::UseGlobalFXBudget();
 	if (!OutState.bCulled && bEnabled && ScalabilitySettings.BudgetScaling.bCullByGlobalBudget)
 	{
 		GlobalBudgetCull(ScalabilitySettings, WorstGlobalBudgetUse, OutState);
 	}

	//TODO: More progressive scalability options?
}

extern float GLastRenderTimeSafetyBias;
void FNiagaraWorldManager::CalculateScalabilityState(UNiagaraSystem* System, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, UNiagaraEffectType* EffectType, UNiagaraComponent* Component, bool bIsPreCull, float WorstGlobalBudgetUse, FNiagaraScalabilityState& OutState)
{
	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Disabled)
	{
		return;
	}

	OutState.bCulled = false;

	DistanceCull(EffectType, ScalabilitySettings, Component, OutState);
	
	if (GEnableNiagaraVisCulling)
	{
		FBoxSphereBounds Bounds = Component->CalcBounds(Component->GetComponentToWorld());		

		float TimeSinceRendered = FMath::Max(0.0f, GetWorld()->LastRenderTime - Component->GetLastRenderTime() - World->GetDeltaSeconds() - GLastRenderTimeSafetyBias);		

		if(bIsPreCull)
		{
			if (System->bFixedBounds)//We need valid bounds for view based culling so if we're preculling, restrict to systems with fixed bounds.
			{
				ViewBasedCulling(EffectType, ScalabilitySettings, Bounds.GetSphere(), TimeSinceRendered, bIsPreCull, OutState);
			}
		}
		else
		{
			if (GbAllowVisibilityCullingForDynamicBounds || System->bFixedBounds)
			{
				ViewBasedCulling(EffectType, ScalabilitySettings, Bounds.GetSphere(), TimeSinceRendered, bIsPreCull, OutState);
			}
		}
	}

	//Only apply hard instance count cull limit for precull if we have no significance handler.
	if (GEnableNiagaraInstanceCountCulling && bIsPreCull && EffectType->GetSignificanceHandler() == nullptr)
	{
		InstanceCountCull(EffectType, System, ScalabilitySettings, OutState);
	}

	bool bEnabled = GEnableNiagaraGlobalBudgetCulling && FFXBudget::Enabled() && INiagaraModule::UseGlobalFXBudget();
 	if (!OutState.bCulled && bEnabled && ScalabilitySettings.BudgetScaling.bCullByGlobalBudget)
	{
 		GlobalBudgetCull(ScalabilitySettings, WorstGlobalBudgetUse, OutState);
 	}

	//TODO: More progressive scalability options?
}

bool FNiagaraWorldManager::CanPreCull(UNiagaraEffectType* EffectType)
{
	checkSlow(EffectType);
	return EffectType->CullReaction == ENiagaraCullReaction::Deactivate || EffectType->CullReaction == ENiagaraCullReaction::DeactivateImmediate;
}

void FNiagaraWorldManager::SortedSignificanceCull(UNiagaraEffectType* EffectType, UNiagaraComponent* Component, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, float Significance, int32& EffectTypeInstCount, uint16& SystemInstCount, FNiagaraScalabilityState& OutState)
{
	//Cull all but the N most significance FX.
	bool bCull = false;
	
	if(GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Enabled && GEnableNiagaraInstanceCountCulling)
	{
		int32 SystemInstanceMax = ScalabilitySettings.MaxSystemInstances;
		int32 EffectTypeInstanceMax = ScalabilitySettings.MaxInstances;

		bCull = ScalabilitySettings.bCullMaxInstanceCount && EffectTypeInstCount >= EffectTypeInstanceMax;
		bCull |= ScalabilitySettings.bCullPerSystemMaxInstanceCount && SystemInstCount >= SystemInstanceMax;

		bool bBudgetCullEnabled = GEnableNiagaraGlobalBudgetCulling && FFXBudget::Enabled() && INiagaraModule::UseGlobalFXBudget();
		if (bCull)
		{
#if DEBUG_SCALABILITY_STATE
			//Clear budget culled flag if we're culling for other reasons already. Help us determine the real impact of budget culling.
			OutState.bCulledByGlobalBudget = false;
#endif
		}
		else if (bBudgetCullEnabled && ScalabilitySettings.BudgetScaling.bCullByGlobalBudget)
	 	{
			float Usage = FFXBudget::GetWorstAdjustedUsage();

			if (ScalabilitySettings.bCullMaxInstanceCount && ScalabilitySettings.BudgetScaling.bScaleMaxInstanceCountByGlobalBudgetUse)
			{
				float Scale = ScalabilitySettings.BudgetScaling.MaxInstanceCountScaleByGlobalBudgetUse.Evaluate(Usage);
				EffectTypeInstanceMax *= Scale;
				bCull = EffectTypeInstCount >= EffectTypeInstanceMax;
			}
			if (ScalabilitySettings.bCullPerSystemMaxInstanceCount && ScalabilitySettings.BudgetScaling.bScaleSystemInstanceCountByGlobalBudgetUse)
			{
				float Scale = ScalabilitySettings.BudgetScaling.MaxSystemInstanceCountScaleByGlobalBudgetUse.Evaluate(Usage);
				SystemInstanceMax *= Scale;
				bCull |= SystemInstCount >= SystemInstanceMax;
			}

#if DEBUG_SCALABILITY_STATE
			OutState.bCulledByGlobalBudget |= bCull;
#endif
	  	}
	}

	OutState.bCulled |= bCull;

	//Only increment the instance counts if this is not culled. Including other causes of culling.
	if(OutState.bCulled == false)
	{
		++EffectTypeInstCount;
		++SystemInstCount;
	}

#if DEBUG_SCALABILITY_STATE
	OutState.bCulledByInstanceCount = bCull;
#endif
}

void FNiagaraWorldManager::ViewBasedCulling(UNiagaraEffectType* EffectType, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, FSphere BoundingSphere, float ComponentTimeSinceRendered, bool bIsPrecull, FNiagaraScalabilityState& OutState)
{
	if (GetScalabilityCullingMode() != ENiagaraScalabilityCullingMode::Enabled)
	{
		return;
	}

	bool bInsideAnyView = !ScalabilitySettings.VisibilityCulling.bCullByViewFrustum;

	//Iterator over all views to calculate and check the bounds against the view frustum.
	if (ScalabilitySettings.VisibilityCulling.bCullByViewFrustum)
	{
		for (FNiagaraCachedViewInfo& ViewInfo : CachedViewInfo)
		{
			//First check the view frustums to see if any part of the sphere is inside.
			bool bInsideThisView = true;
			if (bInsideAnyView == false)//If we already know we're in any view (or not doing frustum checks) then we can skip the rest.
			{
				for (FPlane& FrustumPlane : ViewInfo.FrutumPlanes)
				{
					if (FrustumPlane.IsValid())
					{
						//((dot(Plane, BSphere.xyz) - Plane.w) > BSphere.w);
						bool bInside = FrustumPlane.PlaneDot(BoundingSphere.Center) <= BoundingSphere.W;
						if (!bInside)
						{
							bInsideThisView = false;
							break;
						}
					}
				}
			}

			if (bInsideThisView)
			{
				bInsideAnyView = true;
			}
		}

		// If we have no CachedViewInfo this can mean one of two things, we have no views or we are outside of the main loop (i.e. reregister context) so start the sim anyway
		bInsideAnyView |= CachedViewInfo.Num() == 0;
	}

	float TimeSinceInsideView = 0.0f;
	if (bInsideAnyView)
	{
		OutState.LastVisibleTime = World->GetTimeSeconds();
	}
	else
	{
		TimeSinceInsideView = World->GetTimeSeconds() - OutState.LastVisibleTime;
	}

	bool bCullByOutsideViewFrustum = ScalabilitySettings.VisibilityCulling.bCullByViewFrustum &&
		(!bIsPrecull || ScalabilitySettings.VisibilityCulling.bAllowPreCullingByViewFrustum) &&
		OutState.LastVisibleTime > ScalabilitySettings.VisibilityCulling.MaxTimeOutsideViewFrustum;

	//Check for the component having been rendered recently. If the app doesn't have focus we skip this to avoid issues when alt-tabbing away from the game/editor.
	float TimeSinceWorldRendered = World->GetTimeSeconds() - World->LastRenderTime;	
	bool bCullByNotRendered =	bAppHasFocus && 
								ScalabilitySettings.VisibilityCulling.bCullWhenNotRendered && 
		ComponentTimeSinceRendered > ScalabilitySettings.VisibilityCulling.MaxTimeWithoutRender;

	//TODO: Pull screen size out into it's own debug flag in the scalability state.
	bool bCull = bCullByNotRendered || !bInsideAnyView;

	OutState.bCulled |= bCull;
#if DEBUG_SCALABILITY_STATE
	OutState.bCulledByVisibility = bCull;
#endif
}

void FNiagaraWorldManager::InstanceCountCull(UNiagaraEffectType* EffectType, UNiagaraSystem* System, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, FNiagaraScalabilityState& OutState)
{
	if (GetScalabilityCullingMode() != ENiagaraScalabilityCullingMode::Enabled)
	{
		return;
	}

	int32 SystemInstanceMax = ScalabilitySettings.MaxSystemInstances;
	int32 EffectTypeInstanceMax = ScalabilitySettings.MaxInstances;

	bool bCull = ScalabilitySettings.bCullMaxInstanceCount && EffectType->NumInstances >= EffectTypeInstanceMax;
	bCull |= ScalabilitySettings.bCullPerSystemMaxInstanceCount && System->GetActiveInstancesCount() >= SystemInstanceMax;

	bool bBudgetCullEnabled = GEnableNiagaraGlobalBudgetCulling && FFXBudget::Enabled() && INiagaraModule::UseGlobalFXBudget();
	//Apply budget based adjustments separately so we can mark this cull as being due to budgetting or not.
	if (bCull)
	{
#if DEBUG_SCALABILITY_STATE
		//Clear budget culled flag if we're culling for other reasons already. Help us determine the real impact of budget culling.
		OutState.bCulledByGlobalBudget = false;
#endif
	}
	else if (bBudgetCullEnabled && (ScalabilitySettings.BudgetScaling.bScaleMaxInstanceCountByGlobalBudgetUse || ScalabilitySettings.BudgetScaling.bScaleSystemInstanceCountByGlobalBudgetUse))
	{
		float Usage = FFXBudget::GetWorstAdjustedUsage();

		if (ScalabilitySettings.BudgetScaling.bScaleMaxInstanceCountByGlobalBudgetUse)
		{
			float Scale = ScalabilitySettings.BudgetScaling.MaxInstanceCountScaleByGlobalBudgetUse.Evaluate(Usage);
			EffectTypeInstanceMax *= Scale;
		}
		if (ScalabilitySettings.BudgetScaling.bScaleSystemInstanceCountByGlobalBudgetUse)
		{
			float Scale = ScalabilitySettings.BudgetScaling.MaxSystemInstanceCountScaleByGlobalBudgetUse.Evaluate(Usage);
			SystemInstanceMax *= Scale;
		}
		bCull = ScalabilitySettings.bCullMaxInstanceCount && EffectType->NumInstances >= EffectTypeInstanceMax;
		bCull |= ScalabilitySettings.bCullPerSystemMaxInstanceCount && System->GetActiveInstancesCount() >= SystemInstanceMax;
		OutState.bCulled |= bCull;
#if DEBUG_SCALABILITY_STATE
		OutState.bCulledByGlobalBudget |= bCull;
#endif
	}

	OutState.bCulled |= bCull;
#if DEBUG_SCALABILITY_STATE
	OutState.bCulledByInstanceCount = bCull;
#endif
}

void FNiagaraWorldManager::DistanceCull(UNiagaraEffectType* EffectType, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, UNiagaraComponent* Component, FNiagaraScalabilityState& OutState)
{
	float LODDistance = 0.0f;

	if (Component->bEnablePreviewLODDistance)
	{
		LODDistance = Component->PreviewLODDistance;
	}
	else if(GetCachedViewInfo().Num() > 0)
	{
		float ClosestDistSq = FLT_MAX;
		FVector Location = Component->GetComponentLocation();
		for (const FNiagaraCachedViewInfo& ViewInfo : GetCachedViewInfo())
		{
			ClosestDistSq = FMath::Min(ClosestDistSq, FVector::DistSquared(ViewInfo.ViewToWorld.GetOrigin(), Location));
		}

		LODDistance = FMath::Sqrt(ClosestDistSq);
	}

	//Directly drive the system lod distance from here.
	float MaxDist = ScalabilitySettings.MaxDistance;
	Component->SetLODDistance(LODDistance, FMath::Max(MaxDist, 1.0f));

	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Enabled && GEnableNiagaraDistanceCulling && ScalabilitySettings.bCullByDistance)
	{
		bool bCull = LODDistance > MaxDist;
		OutState.bCulled |= bCull;

		bool bBudgetCullEnabled = GEnableNiagaraGlobalBudgetCulling && FFXBudget::Enabled() && INiagaraModule::UseGlobalFXBudget();
		//Check the budget adjusted range separately so we can tell what is down to budgets and what's distance.
		if (bCull)
		{
#if DEBUG_SCALABILITY_STATE
			//Clear budget culled flag if we're culling for other reasons already. Help us determine the real impact of budget culling.
			OutState.bCulledByGlobalBudget = false;
#endif
		}
		else if (bBudgetCullEnabled && ScalabilitySettings.BudgetScaling.bScaleMaxDistanceByGlobalBudgetUse)
		{
			float Usage = FFXBudget::GetWorstAdjustedUsage();
			float Scale = ScalabilitySettings.BudgetScaling.MaxDistanceScaleByGlobalBudgetUse.Evaluate(Usage);
			MaxDist *= Scale;

			bCull = LODDistance > MaxDist;

#if DEBUG_SCALABILITY_STATE
			OutState.bCulledByGlobalBudget |= bCull;
#endif
		}

#if DEBUG_SCALABILITY_STATE
		OutState.bCulledByDistance = bCull;
#endif
		OutState.bCulled |= bCull;
	}
}

void FNiagaraWorldManager::DistanceCull(UNiagaraEffectType* EffectType, const FNiagaraSystemScalabilitySettings& ScalabilitySettings, FVector Location, FNiagaraScalabilityState& OutState)
{
	if (GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Enabled && GetCachedViewInfo().Num() > 0)
	{
		float ClosestDistSq = FLT_MAX;
		for (const FNiagaraCachedViewInfo& ViewInfo : GetCachedViewInfo())
		{
			ClosestDistSq = FMath::Min(ClosestDistSq, FVector::DistSquared(ViewInfo.ViewToWorld.GetOrigin(), Location));
		}

		if (GEnableNiagaraDistanceCulling && ScalabilitySettings.bCullByDistance)
		{
			float MaxDist = ScalabilitySettings.MaxDistance;
			float ClosestDist = FMath::Sqrt(ClosestDistSq);
			bool bCull = ClosestDist > MaxDist;

			bool bBudgetCullEnabled = GEnableNiagaraGlobalBudgetCulling && FFXBudget::Enabled() && INiagaraModule::UseGlobalFXBudget();
			//Check the budget adjusted range separately so we can tell what is down to budgets and what's distance.
			if (bCull)
			{
#if DEBUG_SCALABILITY_STATE
				//Clear budget culled flag if we're culling for other reasons already. Help us determine the real impact of budget culling.
				OutState.bCulledByGlobalBudget = false;
#endif
			}
			else if (bBudgetCullEnabled && ScalabilitySettings.BudgetScaling.bScaleMaxDistanceByGlobalBudgetUse)
			{
				float Usage = FFXBudget::GetWorstAdjustedUsage();
				float Scale = ScalabilitySettings.BudgetScaling.MaxDistanceScaleByGlobalBudgetUse.Evaluate(Usage);
				MaxDist *= Scale;

				bCull = ClosestDist > MaxDist;

#if DEBUG_SCALABILITY_STATE
				OutState.bCulledByGlobalBudget |= bCull;
#endif
			}

			OutState.bCulled |= bCull;
#if DEBUG_SCALABILITY_STATE
			OutState.bCulledByDistance = bCull;
#endif
		}
	}
}

void FNiagaraWorldManager::GlobalBudgetCull(const FNiagaraSystemScalabilitySettings& ScalabilitySettings, float WorstGlobalBudgetUse, FNiagaraScalabilityState& OutState)
{
 	bool bCull = GetScalabilityCullingMode() == ENiagaraScalabilityCullingMode::Enabled && WorstGlobalBudgetUse >= ScalabilitySettings.BudgetScaling.MaxGlobalBudgetUsage;
 	OutState.bCulled |= bCull;
#if DEBUG_SCALABILITY_STATE
	OutState.bCulledByGlobalBudget |= bCull;
#endif
}

bool FNiagaraWorldManager::GetScalabilityState(UNiagaraComponent* Component, FNiagaraScalabilityState& OutState) const
{
	if ( Component )
	{
		const int32 ScalabilityHandle = Component->GetScalabilityManagerHandle();
		if (ScalabilityHandle != INDEX_NONE)
		{
			if (UNiagaraSystem* System = Component->GetAsset())
			{
				if (UNiagaraEffectType* EffectType = System->GetEffectType())
				{
					if (const FNiagaraScalabilityManager* ScalabilityManager = ScalabilityManagers.Find(EffectType))
					{
						OutState = ScalabilityManager->State[ScalabilityHandle];
						return true;
					}
				}
			}
		}
	}
	return false;
}

void FNiagaraWorldManager::InvalidateCachedSystemScalabilityDataForAllWorlds()
{
	for (auto& Pair : WorldManagers)
	{
		if (Pair.Value)
		{
			Pair.Value->InvalidateCachedSystemScalabilityData();
		}
	}
}

void FNiagaraWorldManager::InvalidateCachedSystemScalabilityData()
{
	for (auto& Pair : ScalabilityManagers)
	{
		FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;
		ScalabilityMan.InvalidateCachedSystemData();
	}
}

void FNiagaraWorldManager::PrimePoolForAllWorlds(UNiagaraSystem* System)
{
	if (GNigaraAllowPrimedPools)
	{
		for (auto& Pair : WorldManagers)
		{
			if (Pair.Value)
			{
				Pair.Value->PrimePool(System);
			}
		}
	}
}

void FNiagaraWorldManager::PrimePoolForAllSystems()
{
	if (GNigaraAllowPrimedPools && World && World->IsGameWorld() && !World->bIsTearingDown)
	{
		//Prime the pool for all currently loaded systems.
		for (TObjectIterator<UNiagaraSystem> It; It; ++It)
		{
			if (UNiagaraSystem* Sys = *It)
			{
				ComponentPool->PrimePool(Sys, World);
			}
		}
	}
}

void FNiagaraWorldManager::PrimePool(UNiagaraSystem* System)
{
	if (GNigaraAllowPrimedPools && World && World->IsGameWorld() && !World->bIsTearingDown)
	{
		ComponentPool->PrimePool(System, World);
	}
}

bool FNiagaraWorldManager::IsComponentLocalPlayerLinked(const USceneComponent* InComponent)
{
	check(InComponent);

	//Is our owner or instigator a locally viewed pawn?
	if (const AActor* Owner = InComponent->GetOwner())
	{
		if (const APawn* OwnerPawn = Cast<const APawn>(Owner))
		{
			if (OwnerPawn->IsLocallyViewed())
			{
				return true;
			}
		}

		if (const APawn* Instigator = Owner->GetInstigator())
		{
			if (Instigator->IsLocallyViewed())
			{
				return true;
			}
		}
	}

	//Walk the attachment hierarchy to check for locally viewed pawns.
	if (InComponent->GetAttachParent() && IsComponentLocalPlayerLinked(InComponent->GetAttachParent()))
	{
		return true;
	}

	return false;
}

#if DEBUG_SCALABILITY_STATE

void FNiagaraWorldManager::DumpScalabilityState()
{
	UE_LOG(LogNiagara, Display, TEXT("========================================================================"));
	UE_LOG(LogNiagara, Display, TEXT("Niagara World Manager Scalability State. %0xP - %s"), World, *World->GetPathName());
	UE_LOG(LogNiagara, Display, TEXT("========================================================================"));

	for (auto& Pair : ScalabilityManagers)
	{
		FNiagaraScalabilityManager& ScalabilityMan = Pair.Value;
		ScalabilityMan.Dump();
	}

	UE_LOG(LogNiagara, Display, TEXT("========================================================================"));
}


FAutoConsoleCommandWithWorld GDumpNiagaraScalabilityData(
	TEXT("fx.DumpNiagaraScalabilityState"),
	TEXT("Dumps state information for all Niagara Scalability Mangers."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
{
	FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World);
	WorldMan->DumpScalabilityState();
}));

#endif

void FNiagaraCachedViewInfo::Init(const FWorldCachedViewInfo& WorldViewInfo)
{
	ViewMat = WorldViewInfo.ViewMatrix;
	ProjectionMat = WorldViewInfo.ProjectionMatrix;
	ViewToWorld = WorldViewInfo.ViewToWorld;
	ViewProjMat = WorldViewInfo.ViewProjectionMatrix;

	FrutumPlanes.SetNumUninitialized(6);
	if (!ViewProjMat.GetFrustumNearPlane(FrutumPlanes[0]))
	{
		FrutumPlanes[0] = FPlane(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if (!ViewProjMat.GetFrustumFarPlane(FrutumPlanes[1]))
	{
		FrutumPlanes[1] = FPlane(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if (!ViewProjMat.GetFrustumTopPlane(FrutumPlanes[2]))
	{
		FrutumPlanes[2] = FPlane(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if (!ViewProjMat.GetFrustumBottomPlane(FrutumPlanes[3]))
	{
		FrutumPlanes[3] = FPlane(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if (!ViewProjMat.GetFrustumLeftPlane(FrutumPlanes[4]))
	{
		FrutumPlanes[4] = FPlane(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if (!ViewProjMat.GetFrustumRightPlane(FrutumPlanes[5]))
	{
		FrutumPlanes[5] = FPlane(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

