// Copyright Epic Games, Inc. All Rights Reserved.

#include "Particles/ParticleSystemManager.h"
#include "Misc/App.h"
#include "Particles/ParticleSystemComponent.h"
#include "ParticleHelper.h"
#include "Particles/ParticleSystem.h"
#include "UObject/UObjectIterator.h"
#include "FXSystem.h"
#include "Distributions/Distribution.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticlePerfStatsManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ParticleSystemManager)

DECLARE_STATS_GROUP(TEXT("Particle World Manager"), STATGROUP_PSCWorldMan, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("PSC Manager Tick [GT]"), STAT_PSCMan_Tick, STATGROUP_PSCWorldMan);
DECLARE_CYCLE_STAT(TEXT("PSC Manager Async Batch [CNC]"), STAT_PSCMan_AsyncBatch, STATGROUP_PSCWorldMan);
DECLARE_CYCLE_STAT(TEXT("PSC Manager Finalize Batch [GT]"), STAT_PSCMan_FinalizeBatch, STATGROUP_PSCWorldMan);

//PRAGMA_DISABLE_OPTIMIZATION

int32 GbEnablePSCWorldManager = 1;
FAutoConsoleVariableRef CVarEnablePSCWorldManager(
	TEXT("fx.PSCMan.Enable"),
	GbEnablePSCWorldManager,
	TEXT("If PSC world manager is enabled."),
	ECVF_Scalability
);

int32 GParticleManagerAsyncBatchSize = INITIAL_PSC_MANAGER_ASYNC_BATCH_SIZE;
FAutoConsoleVariableRef CVarParticleManagerAsyncBatchSize(
	TEXT("fx.ParticleManagerAsyncBatchSize"),
	GParticleManagerAsyncBatchSize,
	TEXT("How many PSCs the ParticleWorldManager should tick per async task."),
	ECVF_Scalability
);

//////////////////////////////////////////////////////////////////////////

class FParticleManagerFinalizeTask
{
	FParticleSystemWorldManager* Owner;
	FPSCManagerAsyncTickBatch PSCsToFinalize;
public:
	FParticleManagerFinalizeTask(FParticleSystemWorldManager* InOwner, FPSCManagerAsyncTickBatch& InPSCsToFinalize)
		: Owner(InOwner)
		, PSCsToFinalize(InPSCsToFinalize)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParticleManagerFinalizeTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
		SCOPE_CYCLE_COUNTER(STAT_PSCMan_FinalizeBatch);

		check(CurrentThread == ENamedThreads::GameThread);

		for (int32 PSCHandle : PSCsToFinalize)
		{
			UParticleSystemComponent* PSC = Owner->GetManagedComponent(PSCHandle);
			FPSCTickData& TickData = Owner->GetTickData(PSCHandle);
			PSC->FinalizeTickComponent();
		}
	}
};

FAutoConsoleTaskPriority CPrio_ParticleManagerAsyncTask(
	TEXT("TaskGraph.TaskPriorities.ParticleManagerAsyncTask"),
	TEXT("Task and thread priority for FParticleManagerAsyncTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
);

class FParticleManagerAsyncTask
{
	FParticleSystemWorldManager* Owner;
	FPSCManagerAsyncTickBatch PSCsToTick;

public:
	FParticleManagerAsyncTask(FParticleSystemWorldManager* InOwner, FPSCManagerAsyncTickBatch& InPSCsToTick)
		: Owner(InOwner)
		, PSCsToTick(InPSCsToTick)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParticleManagerAsyncTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_ParticleManagerAsyncTask.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		//FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
		FTaskTagScope Scope(ETaskTag::EParallelGameThread);
		SCOPE_CYCLE_COUNTER(STAT_PSCMan_AsyncBatch);

// 		FString Ticked;
// 		for (int32 PSCHandle : PSCsToTick)
// 		{
// 			Ticked += TEXT("| ") + LexToString(PSCHandle);
// 		}
// 		//UE_LOG(LogParticles, Warning, TEXT("| Ticking Concurrent Batch %s |"), *Ticked);

		for (int32 PSCHandle : PSCsToTick)
		{
			UParticleSystemComponent* PSC = Owner->GetManagedComponent(PSCHandle);
			FPSCTickData& TickData = Owner->GetTickData(PSCHandle);
			PSC->ComputeTickComponent_Concurrent();
		}

		FGraphEventRef FinalizeTask = TGraphTask<FParticleManagerFinalizeTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(Owner, PSCsToTick);
		MyCompletionGraphEvent->DontCompleteUntil(FinalizeTask);
	}
};

//////////////////////////////////////////////////////////////////////////


#if !UE_BUILD_SHIPPING
const UEnum* FParticleSystemWorldManager::TickGroupEnum = nullptr;
#endif

TMap<UWorld*, FParticleSystemWorldManager*> FParticleSystemWorldManager::WorldManagers;

void FParticleSystemWorldManager::OnStartup()
{
	FWorldDelegates::OnPreWorldInitialization.AddStatic(&OnWorldInit);
	FWorldDelegates::OnPostWorldCleanup.AddStatic(&OnWorldCleanup);
	FWorldDelegates::OnPreWorldFinishDestroy.AddStatic(&OnPreWorldFinishDestroy);
	FWorldDelegates::OnWorldBeginTearDown.AddStatic(&OnWorldBeginTearDown);
#if WITH_PARTICLE_PERF_STATS
	FParticlePerfStatsManager::OnStartup();
#endif
}

void FParticleSystemWorldManager::OnShutdown()
{
	for (TPair<UWorld*, FParticleSystemWorldManager*>& Pair : WorldManagers)
	{
		if (Pair.Value)
		{
			delete Pair.Value;
		}
	}
	WorldManagers.Empty();
#if WITH_PARTICLE_PERF_STATS
	FParticlePerfStatsManager::OnShutdown();
#endif
}

void FParticleSystemWorldManager::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	check(WorldManagers.Find(World) == nullptr);

#if !UE_BUILD_SHIPPING
	if (TickGroupEnum == nullptr)
	{
		TickGroupEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Engine.ETickingGroup"));
	}
#endif
	FParticleSystemWorldManager* NewWorldMan = new FParticleSystemWorldManager(World);
	WorldManagers.Add(World) = NewWorldMan;
	UE_LOG(LogParticles, Verbose, TEXT("| OnWorldInit | WorldMan: %p | World: %p | %s |"), NewWorldMan, World, *World->GetFullName());
}

void FParticleSystemWorldManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (FParticleSystemWorldManager** WorldMan = WorldManagers.Find(World))
	{
		UE_LOG(LogParticles, Verbose, TEXT("| OnWorldCleanup | WorldMan: %p | World: %p | %s |"), *WorldMan, World, *World->GetFullName());
		delete (*WorldMan);
		WorldManagers.Remove(World);
	}
}

void FParticleSystemWorldManager::OnPreWorldFinishDestroy(UWorld* World)
{
	if (FParticleSystemWorldManager** WorldMan = WorldManagers.Find(World))
	{
		UE_LOG(LogParticles, Verbose, TEXT("| OnPreWorldFinishDestroy | WorldMan: %p | World: %p | %s |"), *WorldMan, World, *World->GetFullName());
		delete (*WorldMan);
		WorldManagers.Remove(World);
	}
}

void FParticleSystemWorldManager::OnWorldBeginTearDown(UWorld* World)
{
	if (FParticleSystemWorldManager** WorldMan = WorldManagers.Find(World))
	{
		UE_LOG(LogParticles, Verbose, TEXT("| OnWorldBeginTearDown | WorldMan: %p | World: %p | %s |"), *WorldMan, World, *World->GetFullName());
		delete (*WorldMan);
		WorldManagers.Remove(World);
	}
}

//////////////////////////////////////////////////////////////////////////


FParticleSystemWorldManager::FParticleSystemWorldManager(UWorld* InWorld)
	: World(InWorld)
{
	TickFunctions.SetNum(TG_NewlySpawned);

	bCachedParticleWorldManagerEnabled = GbEnablePSCWorldManager;

	for (int32 TickGroup = 0; TickGroup < TickFunctions.Num(); ++TickGroup)
	{
		FParticleSystemWorldManagerTickFunction& TickFunc = TickFunctions[TickGroup];
		TickFunc.TickGroup = (ETickingGroup)TickGroup;
		TickFunc.EndTickGroup = TickFunc.TickGroup;
		TickFunc.bCanEverTick = true;
		TickFunc.bStartWithTickEnabled = true;
		TickFunc.bAllowTickOnDedicatedServer = false;
		TickFunc.bHighPriority = true;
		TickFunc.Owner = this;
		TickFunc.RegisterTickFunction(InWorld->PersistentLevel);

		TickLists_Concurrent.Add(this);
		TickLists_GT.Add(this);
	}

	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FParticleSystemWorldManager::HandlePostGarbageCollect);
}

FParticleSystemWorldManager::~FParticleSystemWorldManager()
{
	UE_LOG(LogParticles, Verbose, TEXT("| FParticleSystemWorldManager::~FParticleSystemWorldManager() | %p |"), this);
	Cleanup();
}

void FParticleSystemWorldManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (World)
	{
		//UE_LOG(LogParticles, Warning, TEXT("| AddReferencedObjects - %d - 0x%p - %s |"), ManagedPSCs.Num(), World, *World->GetFullName());
		// World doesn't need to be added to the reference list. It will be handle via OnWorldInit & OnWorldCleanup & OnPreWorldFinishDestroy

		for (int32 PSCIndex = 0; PSCIndex < ManagedPSCs.Num(); ++PSCIndex)
		{
			// If a managed component is streamed out or destroyed, drop references to it and its prerequisite			
			if (IsValid(ManagedPSCs[PSCIndex]))
			{
				Collector.AddReferencedObject(ManagedPSCs[PSCIndex]);
			}
			else
			{
				ManagedPSCs[PSCIndex] = nullptr; // Null entries will be cleaned up after GC
			}

			// If prerequisite has been marked for deletion forget it
			if (IsValid(PSCTickData[PSCIndex].PrereqComponent))
			{
				Collector.AddReferencedObject(PSCTickData[PSCIndex].PrereqComponent);
			}
			else
			{
				PSCTickData[PSCIndex].PrereqComponent = nullptr; 
			}
		}

		for (int32 PSCIndex = 0; PSCIndex < PendingRegisterPSCs.Num(); ++PSCIndex)
		{
			if (IsValid(PendingRegisterPSCs[PSCIndex]))
			{
				//UE_LOG(LogParticles, Warning, TEXT("| Add Pending PSC Ref %d - 0x%p |"), PSCIndex, PendingRegisterPSCs[PSCIndex]);
				Collector.AddReferencedObject(PendingRegisterPSCs[PSCIndex]);
			}
			else
			{
				PendingRegisterPSCs[PSCIndex] = nullptr; // Array will be emptied next time we handled pending entries
			}
		}
	}
	else
	{
		//UE_LOG(LogParticles, Warning, TEXT("| AddReferencedObjects - NULL WORLD |"), ManagedPSCs.Num(), World, *World->GetFullName());
	}
}

FString FParticleSystemWorldManager::GetReferencerName() const
{
	return TEXT("FParticleSystemWorldManager");
}

void FParticleSystemWorldManager::HandlePostGarbageCollect()
{
	UE_LOG(LogParticles, Verbose, TEXT("| HandlePostGarbageCollect | WorldMan: %p |"), this);
	for (int32 PSCIndex = ManagedPSCs.Num() - 1; PSCIndex >= 0; --PSCIndex)
	{
		if (ManagedPSCs[PSCIndex] == nullptr)
		{
			RemovePSC(PSCIndex);
		}
	}
}

void FParticleSystemWorldManager::Cleanup()
{
	UE_LOG(LogParticles, Verbose, TEXT("| FParticleSystemWorldManager::Cleanup() | %p |"), this);

	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);

	// Clear out pending particle system components.
	for (UParticleSystemComponent* PendingRegisterPSC : PendingRegisterPSCs)
	{
		if (PendingRegisterPSC != nullptr)
		{
			PendingRegisterPSC->SetManagerHandle(INDEX_NONE);
			PendingRegisterPSC->SetPendingManagerAdd(false);
		}
	}
	PendingRegisterPSCs.Reset();

	// Clear out actively managed particle system components.
	for (int32 PSCIndex = ManagedPSCs.Num() - 1; PSCIndex >= 0; --PSCIndex)
	{
		RemovePSC(PSCIndex);
	}

	World = nullptr;
}

bool FParticleSystemWorldManager::RegisterComponent(UParticleSystemComponent* PSC)
{
	int32 Handle = PSC->GetManagerHandle();
	if (Handle == INDEX_NONE)
	{
		if (!PSC->IsPendingManagerAdd())
		{
			Handle = PendingRegisterPSCs.Add(ObjectPtrWrap(PSC));
			PSC->SetManagerHandle(Handle);
			PSC->SetPendingManagerAdd(true);

			UE_LOG(LogParticles, Verbose, TEXT("| Register New: %p | Man: %p | %d | %s"), PSC, this, Handle, *PSC->Template->GetName());
			return true;
		}
		else
		{
			UE_LOG(LogParticles, Verbose, TEXT("| Register Existing Pending PSC: %p | Man: %p | %d | %s"), PSC, this, Handle, *PSC->Template->GetName());
			return false;
		}
	}
	else if(!PSC->IsPendingManagerAdd())
	{
		FPSCTickData& TickData = PSCTickData[Handle];
		if(TickData.bPendingUnregister)
		{
			//If we're already set to unregister we must flag that we want to be re-registered immediately.
			//We have to re-register rather than just clear this flag so that all tick group checks etc are performed correctly.
			TickData.bPendingReregister = true;

			UE_LOG(LogParticles, Verbose, TEXT("| Re-Register Pending Unregister PSC: %p | Man: %p | %d | %s"), PSC, this, Handle, *PSC->Template->GetName());
		}
		else
		{

			UE_LOG(LogParticles, Verbose, TEXT("| Register Existing PSC: %p | Man: %p | %d | %s"), PSC, this, Handle, *PSC->Template->GetName());
		}

		return true;
	}

	return true;
}


void FParticleSystemWorldManager::UnregisterComponent(UParticleSystemComponent* PSC)
{
	int32 Handle = PSC->GetManagerHandle();
	if (Handle != INDEX_NONE)
	{
		if (PSC->IsPendingManagerAdd())
		{
			UE_LOG(LogParticles, Verbose, TEXT("| UnRegister Pending PSC: %p | Man: %p | %d | %s"), PSC, this, Handle, *PSC->Template->GetName());

			//Clear existing handle
			if (PendingRegisterPSCs[Handle])
			{
				PendingRegisterPSCs[Handle]->SetManagerHandle(INDEX_NONE);
			}
			else
			{
				// Handle scenario where registration and destruction of a component happens 
				// without FParticleSystemWorldManager tick in between and component being nulled
				// after being marked as PendingKill
				PSC->SetManagerHandle(INDEX_NONE);
			}

			PendingRegisterPSCs.RemoveAtSwap(Handle, 1, EAllowShrinking::No);

			//Update handle for moved PCS.
			if (PendingRegisterPSCs.IsValidIndex(Handle))
			{
				PendingRegisterPSCs[Handle]->SetManagerHandle(Handle);
			}

			PSC->SetPendingManagerAdd(false);
		}
		else
		{
			FPSCTickData& TickData = PSCTickData[Handle];

			//Don't remove us immediately from the arrays as this can occur mid tick. Just mark us for removal next frame.
			TickData.bPendingUnregister = true;
			PSC->SetPendingManagerRemove(true);
			TickData.bPendingReregister = false;//Clear if we were due for re register.
			UE_LOG(LogParticles, Verbose, TEXT("| UnRegister PSC: %p | Man: %p | %d | %s"), PSC, this, Handle, *PSC->Template->GetName());
		}
	}
	//handled dependencies
}

void FParticleSystemWorldManager::AddPSC(UParticleSystemComponent* PSC)
{
	if (IsValid(PSC))  // Don't add PSC if it has been marked for deletion
	{
		int32 Handle = ManagedPSCs.Add(ObjectPtrWrap(PSC));
		PSCTickData.AddDefaulted();
		FPSCTickData& TickData = PSCTickData[Handle];

		PSC->SetManagerHandle(Handle);
		PSC->SetPendingManagerAdd(false);

		ETickingGroup TickGroup = TG_DuringPhysics;
		bool bCanTickConcurrent = PSC->CanTickInAnyThread();

		UActorComponent* Prereq = PSC->GetAttachParent();
		if (Prereq)
		{
			int32 PrereqEndTickGroup = (int32)Prereq->PrimaryComponentTick.EndTickGroup;
			TickGroup = (ETickingGroup)FMath::Min(PrereqEndTickGroup + 1, (int32)TG_LastDemotable);
		}
		else
		{
			//Anything with no prereqs can be scheduled early.
			//Should possibly also check for actor params/bone socket modules etc here?
			TickGroup = TG_PrePhysics;
		}

		TickData.bPendingUnregister = false;//Ensure we're not set to unregister if we were.
		TickData.bPendingReregister = false;
	   	TickData.TickGroup = TickGroup;
		TickData.bCanTickConcurrent = bCanTickConcurrent;
		TickData.PrereqComponent = Prereq;

#if PSC_MAN_USE_STATIC_TICK_LISTS
		FTickList& TickList = TickData.bCanTickConcurrent ? TickLists_Concurrent[(int32)TickData.TickGroup] : TickLists_GT[(int32)TickData.TickGroup];
		TickList.Add(Handle);
#endif

		UE_LOG(LogParticles, Verbose, TEXT("| Add PSC - PSC: %p | Man: %p | %d | %d |Num: %d |"), ManagedPSCs[Handle].Get(), this, Handle, TickList[TickData.TickListHandle], ManagedPSCs.Num());
	}
}

void FParticleSystemWorldManager::RemovePSC(int32 PSCIndex)
{
	UParticleSystemComponent* PSC = ManagedPSCs[PSCIndex];
	FPSCTickData& TickData = PSCTickData[PSCIndex];

	//Should re-register after we remove?
	//This is needed if we register again while the PSC is in the pending unregister state.
	bool bReRegister = TickData.bPendingReregister;

	if (PSC)
	{
		PSC->SetManagerHandle(INDEX_NONE);
		PSC->SetPendingManagerRemove(false);
	}


	UE_LOG(LogParticles, Verbose, TEXT("| Remove PSC - PSC: %p | Man: %p | %d |Num: %d |"), ManagedPSCs[PSCIndex].Get(), this, PSCIndex, ManagedPSCs.Num());

#if PSC_MAN_USE_STATIC_TICK_LISTS
	FTickList& TickList = TickData.bCanTickConcurrent ? TickLists_Concurrent[(int32)TickData.TickGroup] : TickLists_GT[(int32)TickData.TickGroup];

	UE_LOG(LogParticles, Verbose, TEXT("| Remove PSC - PSC: %p | Man: %p | %d | %d |Num: %d |"), ManagedPSCs[PSCIndex].Get(), this, PSCIndex, TickList[TickData.TickListHandle], ManagedPSCs.Num());

	TickList.Remove(PSCIndex);

	ManagedPSCs.RemoveAtSwap(PSCIndex, 1, EAllowShrinking::No);
	PSCTickData.RemoveAtSwap(PSCIndex, 1, EAllowShrinking::No);

	if (ManagedPSCs.IsValidIndex(PSCIndex))
	{
		if (ManagedPSCs[PSCIndex])
		{
			ManagedPSCs[PSCIndex]->SetManagerHandle(PSCIndex);
		}

		//Also update the entry in the tick list for the swapped PSC.
		FPSCTickData& SwappedTickData = PSCTickData[PSCIndex];
		FTickList& SwappedTickList = SwappedTickData.bCanTickConcurrent ? TickLists_Concurrent[(int32)SwappedTickData.TickGroup] : TickLists_GT[(int32)SwappedTickData.TickGroup];
		SwappedTickList[SwappedTickData.TickListHandle] = PSCIndex;
	}
#else

	ManagedPSCs.RemoveAtSwap(PSCIndex, 1, EAllowShrinking::No);
	PSCTickData.RemoveAtSwap(PSCIndex, 1, EAllowShrinking::No);

	if (ManagedPSCs.IsValidIndex(PSCIndex))
	{
		if (ManagedPSCs[PSCIndex])
		{
			ManagedPSCs[PSCIndex]->SetManagerHandle(PSCIndex);
		}
	}
#endif

	if (bReRegister)
	{
		AddPSC(PSC);
	}
}

FORCEINLINE void FParticleSystemWorldManager::FlushAsyncTicks(const FGraphEventRef& TickGroupCompletionGraphEvent)
{
	if (AsyncTickBatch.Num() > 0)
	{
		LLM_SCOPE(ELLMTag::Particles);
		FGraphEventRef AsyncTask = TGraphTask<FParticleManagerAsyncTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this, AsyncTickBatch);

#if PSC_MAN_TG_WAIT_FOR_ASYNC
		TickGroupCompletionGraphEvent->DontCompleteUntil(AsyncTask);
#endif

		//Pass to each PSC so they can wait on their respective task if needed.
		for (int32 PSCHandle : AsyncTickBatch)
		{
			UParticleSystemComponent* PSC = GetManagedComponent(PSCHandle);
			PSC->SetAsyncWork(AsyncTask);
		}
		AsyncTickBatch.Reset();
	}
};

FORCEINLINE void FParticleSystemWorldManager::QueueAsyncTick(int32 Handle, const FGraphEventRef& TickGroupCompletionGraphEvent)
{
	AsyncTickBatch.Add(Handle);
	if (AsyncTickBatch.Num() == GParticleManagerAsyncBatchSize)
	{
		FlushAsyncTicks(TickGroupCompletionGraphEvent);
	}
}

void FParticleSystemWorldManager::BuildTickLists(int32 StartIndex, ETickingGroup CurrTickGroup)
{
	//Reset all tick lists.
	if (StartIndex == 0)
	{
		for (FTickList& TickList : TickLists_GT) { TickList.Reset(); }
		for (FTickList& TickList : TickLists_Concurrent) { TickList.Reset(); }
	}

	int32 MinTickGroup = (int32)CurrTickGroup;
	for (int32 Handle = StartIndex; Handle < PSCTickData.Num(); ++Handle)
	{
		FPSCTickData& TickData = PSCTickData[Handle];
		int32 TickGroupToUse = FMath::Max(MinTickGroup, (int32)TickData.TickGroup);

		//TODO: Add budgeting here...
		if (TickData.bCanTickConcurrent)
		{
			TickLists_Concurrent[TickGroupToUse].Add(Handle);
		}
		else
		{
			TickLists_GT[TickGroupToUse].Add(Handle);
		}
	}
}

template<bool bAsync>
void FParticleSystemWorldManager::ProcessTickList(float DeltaTime, ELevelTick TickType, ETickingGroup TickGroup, TArray<FTickList>& TickLists, const FGraphEventRef& TickGroupCompletionGraphEvent)
{
	FTickList& TickList = TickLists[(int32)TickGroup];

	TArray<int32, TInlineAllocator<32>> ToDefer;
	for (int32 Handle : TickList.Get())
	{
		UParticleSystemComponent* PSC = ManagedPSCs[Handle];
		FPSCTickData& TickData = PSCTickData[Handle];
		checkSlow(PSC);

		if (!TickData.bPendingUnregister)
		{
			bool bDoTick = true;
			//TODO: CHECK for location in our array. Since we're not threading anyway we can just rely on location in the buffer. Can just remove at swap in the ToTickArray?
			if (TickData.PrereqComponent && TickData.PrereqComponent->PrimaryComponentTick.IsCompletionHandleValid())
			{
				bDoTick = TickGroup == TG_LastDemotable || TickData.PrereqComponent->PrimaryComponentTick.GetCompletionHandle()->IsComplete();
			}

			if (bDoTick)
			{
				//UE_LOG(LogParticles, Warning, TEXT("| Ticking %d | PSC: %p "), PSCIndex, PSC);

				/////////////////////////////////////////////////////////////////////////////////////////////////////////
				// FORT-319316 - Tracking down why we sometimes have a PSC with no world in the manager?
				if (!PSC->GetWorld())
				{
					UE_LOG(LogParticles, Warning, TEXT("PSC(%s) has no world but is inside PSC Manager. Template(%s) IsValid(%d) IsTickManaged(%d) ManagerHandle(%d) PendingAdd(%d) PendingRemove(%d)"), *GetFullNameSafe(PSC), *GetFullNameSafe(PSC->Template), IsValid(PSC), PSC->IsTickManaged(), PSC->GetManagerHandle(), PSC->IsPendingManagerAdd(), PSC->IsPendingManagerRemove());
					PSC->SetPendingManagerAdd(false);
					PSC->SetPendingManagerRemove(true);
					TickData.bPendingUnregister = true;
					TickData.bPendingReregister = false;
					continue;
				}
				/////////////////////////////////////////////////////////////////////////////////////////////////////////

				if (PSC->CanSkipTickDueToVisibility())
				{
					continue;
				}
				
				AActor* PSCOwner = PSC->GetOwner();
				const float TimeDilation = (PSCOwner ? PSCOwner->CustomTimeDilation : 1.f);

				//TODO: Replace call to TickComponent with new call that allows us to pull duplicated work up to share across all ticks.
				if (bAsync)
				{
					PSC->TickComponent(DeltaTime * TimeDilation, TickType, nullptr);
					PSC->MarshalParamsForAsyncTick();
					QueueAsyncTick(Handle, TickGroupCompletionGraphEvent);
				}
				else
				{
					PSC->TickComponent(DeltaTime * TimeDilation, TickType, nullptr);
					PSC->ComputeTickComponent_Concurrent();
					PSC->FinalizeTickComponent();
				}
			}
			else
			{
				ToDefer.Add(Handle);
			}
		}
	}

	if (TickGroup != TG_LastDemotable)
	{
		int32 NextTickGroup = (int32)TickGroup + 1;
		FTickList& NextTickList = TickLists[NextTickGroup];
		//Push any we couldn't tick yet into later tick lists.
		check(ToDefer.Num() == 0 || TickGroup != TG_LastDemotable);//We have to tick everything in last demotable. Maybe do something better in future.
		if (TickGroup != TG_LastDemotable)
		{
			for (int32 i = 0; i < ToDefer.Num(); ++i)
			{
#if PSC_MAN_USE_STATIC_TICK_LISTS
				TickList.Remove(ToDefer[i]);
#endif
				NextTickList.Add(ToDefer[i]);
				FPSCTickData& TickData = PSCTickData[ToDefer[i]];
				TickData.TickGroup = (ETickingGroup)NextTickGroup;
			}
		}
	}
	ToDefer.Reset();

	if (bAsync)
	{
		FlushAsyncTicks(TickGroupCompletionGraphEvent);
	}
}

void FParticleSystemWorldManager::ClearPendingUnregister()
{
	// Remove any PSC that have been unregistered.
	for (int32 PSCIndex = ManagedPSCs.Num() - 1; PSCIndex >= 0; --PSCIndex)
	{
		FPSCTickData& TickData = PSCTickData[PSCIndex];
		if (TickData.bPendingUnregister)
		{
			RemovePSC(PSCIndex);
		}
	}
}

void FParticleSystemWorldManager::Tick(ETickingGroup TickGroup, float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	//UE_LOG(LogParticles, Verbose, TEXT("| ---- PSC World Manager Tick ----- | TG %s | World: %p - %s |"), *TickGroupEnum->GetNameByValue(TickGroup).ToString(), World, *World->GetFullName());

	SCOPE_CYCLE_COUNTER(STAT_PSCMan_Tick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	
	//Do some book keeping in the first tick group, PrePhysics.
	int32 BuildListStart = ManagedPSCs.Num();
	if (TickGroup == TG_PrePhysics)
	{
		HandleManagerEnabled();
		ClearPendingUnregister();
		BuildListStart = 0;
	}

	if (!bCachedParticleWorldManagerEnabled)
	{
		check(ManagedPSCs.Num() == 0);
		check(PSCTickData.Num() == 0);
		return;
	}

	//Add any pending PSCs to the main arrays.
	for (UParticleSystemComponent* PendingPSC : PendingRegisterPSCs)
	{
		AddPSC(PendingPSC);
	}

	PendingRegisterPSCs.Reset();

	if (!PSC_MAN_USE_STATIC_TICK_LISTS)
	{
		BuildTickLists(BuildListStart, TickGroup);
	}
	
	check(PSCTickData.Num() == ManagedPSCs.Num());

	//Do GT only ticks.
	ProcessTickList<false>(DeltaTime, TickType, TickGroup, TickLists_GT, nullptr);
		   
	//UE_LOG(LogParticles, Warning, TEXT("| Queue Concurrent Ticks | TG: %s |"), *TickGroupEnum->GetNameByValue(TickGroup).ToString());

	bool bAllowTickConcurrent = !FXConsoleVariables::bFreezeParticleSimulation && FXConsoleVariables::bAllowAsyncTick && FApp::ShouldUseThreadingForPerformance() && GDistributionType != 0;
	if (bAllowTickConcurrent)
	{
		//TODO: Currently waiting on this tick group but we should be able to allow these tasks to run over the whole frame if we can implement some synchronization so they don't overrun the EOF updates.
		ProcessTickList<true>(DeltaTime, TickType, TickGroup, TickLists_Concurrent, MyCompletionGraphEvent);
	}
	else
	{
		ProcessTickList<false>(DeltaTime, TickType, TickGroup, TickLists_Concurrent, nullptr);
	}
}

void FParticleSystemWorldManager::HandleManagerEnabled()
{
	if (GbEnablePSCWorldManager != bCachedParticleWorldManagerEnabled)
	{
		bCachedParticleWorldManagerEnabled = GbEnablePSCWorldManager;

		//SetComponentTick on all PSCs. This will set their correct tick state for the current GbEnablePSCWorldManager.
		for (TObjectIterator<UParticleSystemComponent> PSCIt; PSCIt; ++PSCIt)
		{
			UParticleSystemComponent* PSC = *PSCIt;
			check(PSC);
			UWorld* PSCWorld = PSC->GetWorld();
			if (PSCWorld == World)
			{
				if (PSC->IsComponentTickEnabled())
				{
					PSC->SetComponentTickEnabled(true);
				}
			}
		}

		if (bCachedParticleWorldManagerEnabled)
		{
			//UE_LOG(LogParticles, Warning, TEXT("| Enabling Particle System World Manager |"));

			//Enable all tick functions
			for (FParticleSystemWorldManagerTickFunction& TickFunc : TickFunctions)
			{
				TickFunc.SetTickFunctionEnable(true);
			}
		}
		else
		{
			//UE_LOG(LogParticles, Warning, TEXT("| Disabling Particle System World Manager |"));

			//Disable all but leave pre physics in tact to poll the cvar for changes. TODO: Remove this for shipping? Don't allow changing at RT in Shipped?
			for (FParticleSystemWorldManagerTickFunction& TickFunc : TickFunctions)
			{
				TickFunc.SetTickFunctionEnable(TickFunc.TickGroup == TG_PrePhysics);
			}
		}
	}
}

FAutoConsoleCommandWithWorld GDumpPSCManStateCommand(
	TEXT("fx.PSCMan.Dump"),
	TEXT("Dumps state information for all current Particle System Managers."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
{
	if (FParticleSystemWorldManager* PSCMan = FParticleSystemWorldManager::Get(World))
	{
		PSCMan->Dump();
	}
}));

void FParticleSystemWorldManager::Dump()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogParticles, Log, TEXT("|-------------------------------------------------------------------------------------------------------|"));
	UE_LOG(LogParticles, Log, TEXT("|	   	               Managed Particle System Component Tick State Info                                |"));
	UE_LOG(LogParticles, Log, TEXT("|-------------------------------------------------------------------------------------------------------|"));
	for (int32 Handle = 0; Handle < ManagedPSCs.Num(); ++Handle)
	{
		UParticleSystemComponent* PSC = ManagedPSCs[Handle];
		FPSCTickData& TickData = PSCTickData[Handle];

		int32 NumParticles = PSC->GetNumActiveParticles();
		FString SigString = TEXT("NA");
		if (PSC->bIsManagingSignificance)
		{
			SigString = TEXT("false");
			for (UParticleEmitter* Emitter : PSC->Template->Emitters)
			{
				if (Emitter->IsSignificant(PSC->RequiredSignificance))
				{
					SigString = TEXT("true");
					break;
				}
			}
		}

		bool bVis = PSC->CanConsiderInvisible();
		bool bActive = PSC->IsActive();
		UE_LOG(LogParticles, Log, TEXT("| %d | %s |0x%p | Active: %d | Sig: %s | Vis: %d | Num: %d | %s | Prereq: 0x%p - %s |"),
					 Handle, *TickGroupEnum->GetNameByValue(TickData.TickGroup).ToString() , PSC, bActive, *SigString, bVis, NumParticles, *PSC->GetFullName(), TickData.PrereqComponent.Get(), TickData.PrereqComponent ? *TickData.PrereqComponent->GetFullName() : TEXT(""));
	}
#endif
}


//////////////////////////////////////////////////////////////////////////

void FParticleSystemWorldManagerTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(Owner);
	Owner->Tick(TickGroup, DeltaTime, TickType, CurrentThread, MyCompletionGraphEvent);
}

FString FParticleSystemWorldManagerTickFunction::DiagnosticMessage()
{
	static const UEnum* EnumType = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Engine.ETickingGroup"));

	return TEXT("FParticleSystemManager::Tick(") + EnumType->GetNameStringByIndex(static_cast<uint32>(TickGroup)) + TEXT(")");
}

FName FParticleSystemWorldManagerTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("ParticleSystemManager"));
}

//////////////////////////////////////////////////////////////////////////

FPSCTickData::FPSCTickData()
	: PrereqComponent(nullptr)
#if PSC_MAN_USE_STATIC_TICK_LISTS
	, TickListHandle(INDEX_NONE)
#endif
	, TickGroup(TG_PrePhysics)
	, bCanTickConcurrent(0)
	, bPendingUnregister(0)
	, bPendingReregister(0)
{

}

//////////////////////////////////////////////////////////////////////////
void FParticleSystemWorldManager::FTickList::Add(int32 Handle)
{
	check(Owner);
#if PSC_MAN_USE_STATIC_TICK_LISTS
	FPSCTickData& TickData = Owner->GetTickData(Handle);
	check(TickData.TickListHandle == INDEX_NONE);

	TickData.TickListHandle = TickList.Add(Handle);
#else
	TickList.Add(Handle);
#endif
}

void FParticleSystemWorldManager::FTickList::Remove(int32 Handle)
{
	check(Owner);

#if PSC_MAN_USE_STATIC_TICK_LISTS
	FPSCTickData& TickData = Owner->GetTickData(Handle);
	check(TickList.IsValidIndex(TickData.TickListHandle));

	TickList.RemoveAtSwap(TickData.TickListHandle, 1, EAllowShrinking::No);

	if (TickList.IsValidIndex(TickData.TickListHandle))
	{
		int32 SwappedHandle = TickList[TickData.TickListHandle];
		FPSCTickData& SwappedTickData = Owner->GetTickData(SwappedHandle);
		check(SwappedTickData.TickListHandle == TickList.Num());
		SwappedTickData.TickListHandle = TickData.TickListHandle;
	}

	TickData.TickListHandle = INDEX_NONE;
#else
	check(false);//Shouldn't need to remove anything if using dynamic lists.
#endif
}

void FParticleSystemWorldManager::FTickList::Reset()
{
	TickList.Reset();
}

//PRAGMA_ENABLE_OPTIMIZATION
