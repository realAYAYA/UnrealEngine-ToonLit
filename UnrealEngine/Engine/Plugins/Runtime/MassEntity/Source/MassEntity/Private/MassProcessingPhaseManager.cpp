// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassProcessingPhaseManager.h"
#include "MassProcessingTypes.h"
#include "MassDebugger.h"
#include "MassProcessor.h"
#include "MassExecutor.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "VisualLogger/VisualLogger.h"
#include "Engine/World.h"
#include "MassCommandBuffer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassProcessingPhaseManager)

#define LOCTEXT_NAMESPACE "Mass"

DECLARE_CYCLE_STAT(TEXT("Mass Phase Tick"), STAT_Mass_PhaseTick, STATGROUP_Mass);

namespace UE::Mass::Tweakables
{
	bool bFullyParallel = MASS_DO_PARALLEL;
	bool bMakePrePhysicsTickFunctionHighPriority = true;

	FAutoConsoleVariableRef CVars[] = {
		{TEXT("mass.FullyParallel"), bFullyParallel, TEXT("Enables mass processing distribution to all available thread (via the task graph)")},
		{TEXT("mass.MakePrePhysicsTickFunctionHighPriority"), bMakePrePhysicsTickFunctionHighPriority, TEXT("Whether to make the PrePhysics tick function high priority - can minimise GameThread waits by starting parallel work as soon as possible")},
	};
}

namespace UE::Mass::Private
{
	ETickingGroup PhaseToTickingGroup[int(EMassProcessingPhase::MAX)]
	{
		ETickingGroup::TG_PrePhysics, // EMassProcessingPhase::PrePhysics
		ETickingGroup::TG_StartPhysics, // EMassProcessingPhase::StartPhysics
		ETickingGroup::TG_DuringPhysics, // EMassProcessingPhase::DuringPhysics
		ETickingGroup::TG_EndPhysics,	// EMassProcessingPhase::EndPhysics
		ETickingGroup::TG_PostPhysics,	// EMassProcessingPhase::PostPhysics
		ETickingGroup::TG_LastDemotable, // EMassProcessingPhase::FrameEnd
	};
} // UE::Mass::Private

//----------------------------------------------------------------------//
//  FMassProcessingPhase
//----------------------------------------------------------------------//
FMassProcessingPhase::FMassProcessingPhase()
{
	bCanEverTick = true;
	bStartWithTickEnabled = false;
	SupportedTickTypes = (1 << LEVELTICK_All) | (1 << LEVELTICK_TimeOnly);
}

void FMassProcessingPhase::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (ShouldTick(TickType) == false)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_Mass_PhaseTick);
	SCOPE_CYCLE_COUNTER(STAT_Mass_Total);

	checkf(PhaseManager, TEXT("Manager is null which is not a supported case. Either this FMassProcessingPhase has not been initialized properly or it's been left dangling after the FMassProcessingPhase owner got destroyed."));

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FMassProcessingPhase::ExecuteTick %s"), *UEnum::GetValueAsString(Phase)));

	PhaseManager->OnPhaseStart(*this);
	{
		LLM_SCOPE_BYNAME(TEXT("Mass/PhaseStartDelegate"));
		OnPhaseStart.Broadcast(DeltaTime);
	}

	check(PhaseProcessor);
	
	FMassEntityManager& EntityManager = PhaseManager->GetEntityManagerRef();
	FMassProcessingContext Context(EntityManager, DeltaTime);

	bIsDuringMassProcessing = true;

	if (bRunInParallelMode)
	{
		bool bWorkRequested = false;
		if (PhaseProcessor->IsEmpty() == false)
		{
			const FGraphEventRef PipelineCompletionEvent = UE::Mass::Executor::TriggerParallelTasks(*PhaseProcessor, Context, [this, DeltaTime]()
				{
					OnParallelExecutionDone(DeltaTime);
				});

			if (PipelineCompletionEvent.IsValid())
			{
				MyCompletionGraphEvent->DontCompleteUntil(PipelineCompletionEvent);
				bWorkRequested = true;
			}
		}
		if (bWorkRequested == false)
		{
			OnParallelExecutionDone(DeltaTime);
		}
	}
	else
	{
		UE::Mass::Executor::Run(*PhaseProcessor, Context);

		{
			LLM_SCOPE_BYNAME(TEXT("Mass/PhaseEndDelegate"));
			OnPhaseEnd.Broadcast(DeltaTime);
		}
		PhaseManager->OnPhaseEnd(*this);
		bIsDuringMassProcessing = false;
	}
}

void FMassProcessingPhase::OnParallelExecutionDone(const float DeltaTime)
{
	bIsDuringMassProcessing = false;
	{
		LLM_SCOPE_BYNAME(TEXT("Mass/PhaseEndDelegate"));
		OnPhaseEnd.Broadcast(DeltaTime);
	}
	check(PhaseManager);
	PhaseManager->OnPhaseEnd(*this);
}

FString FMassProcessingPhase::DiagnosticMessage()
{
	return (PhaseManager ? PhaseManager->GetName() : TEXT("NULL-MassProcessingPhaseManager")) + TEXT("[ProcessingPhaseTick]");
}

FName FMassProcessingPhase::DiagnosticContext(bool bDetailed)
{
	return TEXT("MassProcessingPhase");
}

void FMassProcessingPhase::Initialize(FMassProcessingPhaseManager& InPhaseManager, const EMassProcessingPhase InPhase, const ETickingGroup InTickGroup, UMassCompositeProcessor& InPhaseProcessor)
{
	PhaseManager = &InPhaseManager;
	Phase = InPhase;
	TickGroup = InTickGroup;
	PhaseProcessor = &InPhaseProcessor;
}

//----------------------------------------------------------------------//
// FPhaseProcessorConfigurator
//----------------------------------------------------------------------//
void FMassPhaseProcessorConfigurationHelper::Configure(TArrayView<UMassProcessor* const> DynamicProcessors
	, EProcessorExecutionFlags InWorldExecutionFlags, const TSharedPtr<FMassEntityManager>& EntityManager
	, FMassProcessorDependencySolver::FResult* OutOptionalResult)
{
	FMassRuntimePipeline TmpPipeline(InWorldExecutionFlags);
	TmpPipeline.CreateFromArray(PhaseConfig.ProcessorCDOs, ProcessorOuter);
	for (UMassProcessor* Processor : DynamicProcessors)
	{
		checkf(Processor != nullptr, TEXT("Dynamic processor provided to MASS is null."));
		if (Processor->GetProcessingPhase() == Phase)
		{
			TmpPipeline.AppendProcessor(*Processor);
		}
	}

	TArray<FMassProcessorOrderInfo> SortedProcessors;
	FMassProcessorDependencySolver Solver(TmpPipeline.GetMutableProcessors(), bIsGameRuntime);

	Solver.ResolveDependencies(SortedProcessors, EntityManager, OutOptionalResult);

	PhaseProcessor.UpdateProcessorsCollection(SortedProcessors, InWorldExecutionFlags);

#if WITH_MASSENTITY_DEBUG
	for (const FMassProcessorOrderInfo& ProcessorOrderInfo : SortedProcessors)
	{
		TmpPipeline.RemoveProcessor(*ProcessorOrderInfo.Processor);
	}
	
	if (TmpPipeline.Num())
	{
		UE_VLOG_UELOG(&PhaseProcessor, LogMass, Verbose, TEXT("Discarding processors due to not having anything to do (no relevant Archetypes):"));
		for (const UMassProcessor* Processor : TmpPipeline.GetProcessors())
		{
			UE_VLOG_UELOG(&PhaseProcessor, LogMass, Verbose, TEXT("\t%s"), *Processor->GetProcessorName());
		}
	}
#endif // WITH_MASSENTITY_DEBUG

	if (Solver.IsSolvingForSingleThread() == false)
	{
		PhaseProcessor.BuildFlatProcessingGraph(SortedProcessors);
	}

	if (bInitializeCreatedProcessors)
	{
		PhaseProcessor.Initialize(ProcessorOuter);
	}
}

//----------------------------------------------------------------------//
// FMassProcessingPhaseManager
//----------------------------------------------------------------------//
void FMassProcessingPhaseManager::Initialize(UObject& InOwner, TConstArrayView<FMassProcessingPhaseConfig> InProcessingPhasesConfig, const FString& DependencyGraphFileName)
{
	UWorld* World = InOwner.GetWorld();
#if WITH_EDITOR
	const bool bCreateProcessorGraphPreview = (World != nullptr) && (World->IsEditorWorld() && !World->IsGameWorld());
#endif // WITH_EDITOR
	Owner = &InOwner;
	ProcessingPhasesConfig = InProcessingPhasesConfig;

	ProcessorExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(World, ProcessorExecutionFlags);

	for (int PhaseAsInt = 0; PhaseAsInt < int(EMassProcessingPhase::MAX); ++PhaseAsInt)
	{		
		const EMassProcessingPhase Phase = EMassProcessingPhase(PhaseAsInt);

		UMassCompositeProcessor* PhaseProcessor = NewObject<UMassCompositeProcessor>(&InOwner, UMassCompositeProcessor::StaticClass()
			, *FString::Printf(TEXT("ProcessingPhase_%s"), *UEnum::GetDisplayValueAsText(Phase).ToString()));
	
		check(PhaseProcessor);
		ProcessingPhases[PhaseAsInt].Initialize(*this, Phase, UE::Mass::Private::PhaseToTickingGroup[PhaseAsInt], *PhaseProcessor);

		REDIRECT_OBJECT_TO_VLOG(PhaseProcessor, &InOwner);
		PhaseProcessor->SetProcessingPhase(Phase);
		PhaseProcessor->SetGroupName(FName(FString::Printf(TEXT("%s Group"), *UEnum::GetDisplayValueAsText(Phase).ToString())));

#if WITH_MASSENTITY_DEBUG
		FStringOutputDevice Ar;
		PhaseProcessor->DebugOutputDescription(Ar);
		UE_VLOG(&InOwner, LogMass, Log, TEXT("Setting new group processor for phase %s:\n%s"), *UEnum::GetValueAsString(Phase), *Ar);
#endif // WITH_MASSENTITY_DEBUG

#if WITH_EDITOR
		if (bCreateProcessorGraphPreview)
		{
			// populating the phase processor with initial data for editor world so that the default processing graph
			// can be investigated in the editor without running PIE.
			// Runtime processing graph are initialized at runtime base on the actual archetypes instantiated at call time.
			FMassProcessorDependencySolver::FResult Result;
			Result.DependencyGraphFileName = DependencyGraphFileName;
			FMassPhaseProcessorConfigurationHelper Configurator(*PhaseProcessor, ProcessingPhasesConfig[PhaseAsInt], InOwner, EMassProcessingPhase(PhaseAsInt));
			Configurator.bIsGameRuntime = false;
			// passing EProcessorExecutionFlags::All here to gather all available processors since bCreateProcessorGraphPreview 
			// is true when we want to preview processors that might be available at runtime.
			Configurator.Configure({}, EProcessorExecutionFlags::All, /*EntityManager=*/nullptr, &Result);
		}
#endif // WITH_EDITOR
	}

	bIsAllowedToTick = true;
}

void FMassProcessingPhaseManager::Deinitialize()
{
	for (FMassProcessingPhase& Phase : ProcessingPhases)
	{
		Phase.PhaseProcessor = nullptr;
	}
}

const FGraphEventRef& FMassProcessingPhaseManager::TriggerPhase(const EMassProcessingPhase Phase, const float DeltaTime, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(Phase != EMassProcessingPhase::MAX);

	if (bIsAllowedToTick)
	{
		ProcessingPhases[(int)Phase].ExecuteTick(DeltaTime, LEVELTICK_All, ENamedThreads::GameThread, MyCompletionGraphEvent);
	}

	return MyCompletionGraphEvent;
}

void FMassProcessingPhaseManager::Start(UWorld& World)
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);

	if (ensure(EntitySubsystem))
	{
		Start(EntitySubsystem->GetMutableEntityManager().AsShared());
	}
	else
	{
		UE_VLOG_UELOG(Owner.Get(), LogMass, Error, TEXT("Called %s while missing the EntitySubsystem"), ANSI_TO_TCHAR(__FUNCTION__));
	}
}

void FMassProcessingPhaseManager::Start(const TSharedPtr<FMassEntityManager>& InEntityManager)
{
	EntityManager = InEntityManager;

	OnNewArchetypeHandle = EntityManager->GetOnNewArchetypeEvent().AddRaw(this, &FMassProcessingPhaseManager::OnNewArchetype);

	if (UWorld* World = InEntityManager->GetWorld())
	{
		EnableTickFunctions(*World);
	}

	bIsAllowedToTick = true;
}

void FMassProcessingPhaseManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FMassProcessingPhase& Phase : ProcessingPhases)
	{
		if (Phase.PhaseProcessor)
		{
			Collector.AddReferencedObject(Phase.PhaseProcessor);
		}
	}

	for (auto& DynamicProcessor : DynamicProcessors)
	{
		if (DynamicProcessor)
		{
			Collector.AddReferencedObject(DynamicProcessor);
		}
	}
}

void FMassProcessingPhaseManager::EnableTickFunctions(const UWorld& World)
{
	check(EntityManager);

	const bool bIsGameWorld = World.IsGameWorld();

	for (FMassProcessingPhase& Phase : ProcessingPhases)
	{
		if (UE::Mass::Tweakables::bMakePrePhysicsTickFunctionHighPriority && (Phase.Phase == EMassProcessingPhase::PrePhysics))
		{
			constexpr bool bHighPriority = true;
			Phase.SetPriorityIncludingPrerequisites(bHighPriority);
		}

		Phase.RegisterTickFunction(World.PersistentLevel);
		Phase.SetTickFunctionEnable(true);
#if WITH_MASSENTITY_DEBUG
		if (Phase.PhaseProcessor && bIsGameWorld)
		{
			// not logging this in the editor mode since it messes up the game-recorded vislog display (with its progressively larger timestamp)
			FStringOutputDevice Ar;
			Phase.PhaseProcessor->DebugOutputDescription(Ar);
			UE_VLOG_UELOG(Owner.Get(), LogMass, Log, TEXT("Enabling phase %s tick:\n%s")
				, *UEnum::GetValueAsString(Phase.Phase), *Ar);
		}
#endif // WITH_MASSENTITY_DEBUG
	}

	if (bIsGameWorld)
	{
		// not logging this in the editor mode since it messes up the game-recorded vislog display (with its progressively larger timestamp)
		UE_VLOG_UELOG(Owner.Get(), LogMass, Log, TEXT("MassProcessingPhaseManager %s.%s has been started")
			, *GetNameSafe(Owner.Get()), *GetName());
	}
}

void FMassProcessingPhaseManager::Stop()
{
	bIsAllowedToTick = false;

	if (EntityManager)
	{
		EntityManager->GetOnNewArchetypeEvent().Remove(OnNewArchetypeHandle);
		EntityManager.Reset();
	}
	
	for (FMassProcessingPhase& Phase : ProcessingPhases)
	{
		Phase.SetTickFunctionEnable(false);
	}

	if (UObject* LocalOwner = Owner.Get())
	{
		UWorld* World = LocalOwner->GetWorld();
		if (World && World->IsGameWorld())
		{
			// not logging this in editor mode since it messes up the game-recorded vislog display (with its progressively larger timestamp) 
			UE_VLOG_UELOG(LocalOwner, LogMass, Log, TEXT("MassProcessingPhaseManager %s.%s has been stopped")
				, *GetNameSafe(LocalOwner), *GetName());
		}
	}
}

void FMassProcessingPhaseManager::OnPhaseStart(const FMassProcessingPhase& Phase)
{
	ensure(CurrentPhase == EMassProcessingPhase::MAX);
	CurrentPhase = Phase.Phase;

	const int32 PhaseAsInt = int32(Phase.Phase);
	if (Owner.IsValid()
		&& ensure(Phase.Phase != EMassProcessingPhase::MAX)
		&& (ProcessingGraphBuildStates[PhaseAsInt].bNewArchetypes || ProcessingGraphBuildStates[PhaseAsInt].bProcessorsNeedRebuild)
		// if not a valid index then we're not able to recalculate dependencies 
		&& ensure(ProcessingPhasesConfig.IsValidIndex(PhaseAsInt)))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Mass Rebuild Phase Graph");

		FPhaseGraphBuildState& GraphBuildState = ProcessingGraphBuildStates[PhaseAsInt];
		if (GraphBuildState.bInitialized == false 
			|| ProcessingGraphBuildStates[PhaseAsInt].bProcessorsNeedRebuild
			|| FMassProcessorDependencySolver::IsResultUpToDate(GraphBuildState.LastResult, EntityManager) == false)
		{
			UMassCompositeProcessor* PhaseProcessor = ProcessingPhases[PhaseAsInt].PhaseProcessor;
			check(PhaseProcessor);

			GraphBuildState.LastResult.Reset();

			FMassPhaseProcessorConfigurationHelper Configurator(*PhaseProcessor, ProcessingPhasesConfig[PhaseAsInt], *Owner.Get(), Phase.Phase);
			Configurator.Configure(DynamicProcessors, ProcessorExecutionFlags, EntityManager, &GraphBuildState.LastResult);

			GraphBuildState.bInitialized = true;

#if WITH_MASSENTITY_DEBUG
			UObject* OwnerPtr = Owner.Get();
			// print it all out to vislog
			UE_VLOG_UELOG(OwnerPtr, LogMass, Verbose, TEXT("Phases initialization done. Current composition:"));

			FStringOutputDevice OutDescription;
			PhaseProcessor->DebugOutputDescription(OutDescription);
			UE_VLOG_UELOG(OwnerPtr, LogMass, Verbose, TEXT("--- %s"), *OutDescription);
			OutDescription.Reset();
#endif // WITH_MASSENTITY_DEBUG
		}

		ProcessingGraphBuildStates[PhaseAsInt].bProcessorsNeedRebuild = false;
		ProcessingGraphBuildStates[PhaseAsInt].bNewArchetypes = false;
	}
}

void FMassProcessingPhaseManager::OnPhaseEnd(FMassProcessingPhase& Phase)
{
	ensure(CurrentPhase == Phase.Phase);
	CurrentPhase = EMassProcessingPhase::MAX;

	// switch between parallel and single-thread versions only after a given batch of processing has been wrapped up	
	if (Phase.IsConfiguredForParallelMode() != UE::Mass::Tweakables::bFullyParallel)
	{
		if (UE::Mass::Tweakables::bFullyParallel)
		{
			Phase.ConfigureForParallelMode();
		}
		else
		{
			Phase.ConfigureForSingleThreadMode();
		}
	}
}

FString FMassProcessingPhaseManager::GetName() const
{
	return GetNameSafe(Owner.Get()) + TEXT("_MassProcessingPhaseManager");
}

void FMassProcessingPhaseManager::RegisterDynamicProcessor(UMassProcessor& Processor)
{
	DynamicProcessors.Add(&Processor);
	ProcessingGraphBuildStates[uint32(Processor.GetProcessingPhase())].bProcessorsNeedRebuild = true;
}

void FMassProcessingPhaseManager::UnregisterDynamicProcessor(UMassProcessor& Processor)
{
	int32 Index;
	if (DynamicProcessors.Find(&Processor, Index))
	{
		DynamicProcessors.RemoveAt(Index);
		ProcessingGraphBuildStates[uint32(Processor.GetProcessingPhase())].bProcessorsNeedRebuild = true;
	}
	else
	{
		checkf(false, TEXT("Unable to remove Processor '%s', as it was never added or already removed."), *Processor.GetName());
	}
}

void FMassProcessingPhaseManager::OnNewArchetype(const FMassArchetypeHandle& NewArchetype)
{
	for (FPhaseGraphBuildState& GraphBuildState : ProcessingGraphBuildStates)
	{
		GraphBuildState.bNewArchetypes = true;
	}
}

#undef LOCTEXT_NAMESPACE
