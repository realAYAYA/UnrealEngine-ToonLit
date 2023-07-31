// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassProcessor.h"
#include "MassEntitySettings.h"
#include "MassProcessorDependencySolver.h"
#include "VisualLogger/VisualLogger.h"
#include "Engine/World.h"
#include "MassCommandBuffer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassProcessor)

DECLARE_CYCLE_STAT(TEXT("MassProcessor Group Completed"), Mass_GroupCompletedTask, STATGROUP_TaskGraphTasks);

#if WITH_MASSENTITY_DEBUG
namespace UE::Mass::Debug
{
	bool bLogProcessingGraph = false;
	FAutoConsoleVariableRef CVarLogProcessingGraph(TEXT("mass.LogProcessingGraph"), bLogProcessingGraph
		, TEXT("When enabled will log task graph tasks created while dispatching processors to other threads, along with their dependencies"), ECVF_Cheat);
}

#define PROCESSOR_LOG(Verbosity, Fmt, ...) UE_VLOG_UELOG(this, LogMass, Verbosity, Fmt, ##__VA_ARGS__)

// change to 1 to enable more detailed processing tasks logging
#if 0
#define PROCESSOR_TASK_LOG(Fmt, ...) UE_VLOG_UELOG(this, LogMass, Verbose, Fmt, ##__VA_ARGS__)
#else
#define PROCESSOR_TASK_LOG(...) 
#endif // 0

#else 
#define PROCESSOR_LOG(...)
#define PROCESSOR_TASK_LOG(...) 
#endif // WITH_MASSENTITY_DEBUG

class FMassProcessorTask
{
public:
	FMassProcessorTask(const TSharedPtr<FMassEntityManager>& InEntityManager, const FMassExecutionContext& InExecutionContext, UMassProcessor& InProc, bool bInManageCommandBuffer = true)
		: EntityManager(InEntityManager)
		, ExecutionContext(InExecutionContext)
		, Processor(&InProc)
		, bManageCommandBuffer(bInManageCommandBuffer)
	{}

	static TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMassProcessorTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	static ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyHiPriThreadHiPriTask;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		checkf(Processor, TEXT("Expecting a valid processor to execute"));

		PROCESSOR_TASK_LOG(TEXT("+--+ Task %s started on %u"), *Processor->GetProcessorName(), FPlatformTLS::GetCurrentThreadId());

		check(EntityManager);
		FMassEntityManager& EntityManagerRef = *EntityManager.Get();
		FMassEntityManager::FScopedProcessing ProcessingScope = EntityManagerRef.NewProcessingScope();

		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Mass Processor Task");
		
		if (bManageCommandBuffer)
		{
			TSharedPtr<FMassCommandBuffer> MainSharedPtr = ExecutionContext.GetSharedDeferredCommandBuffer();
			ExecutionContext.SetDeferredCommandBuffer(MakeShareable(new FMassCommandBuffer()));
			Processor->CallExecute(EntityManagerRef, ExecutionContext);
			MainSharedPtr->MoveAppend(ExecutionContext.Defer());
		}
		else
		{
			Processor->CallExecute(EntityManagerRef, ExecutionContext);
		}
		PROCESSOR_TASK_LOG(TEXT("+--+ Task %s finished"), *Processor->GetProcessorName());
	}

private:
	TSharedPtr<FMassEntityManager> EntityManager;
	FMassExecutionContext ExecutionContext;
	UMassProcessor* Processor = nullptr;
	/** 
	 * indicates whether this task is responsible for creation of a dedicated command buffer and transferring over the 
	 * commands after processor's execution;
	 */
	bool bManageCommandBuffer = true;
};

class FMassProcessorsTask_GameThread : public FMassProcessorTask
{
public:
	FMassProcessorsTask_GameThread(const TSharedPtr<FMassEntityManager>& InEntityManager, const FMassExecutionContext& InExecutionContext, UMassProcessor& InProc)
		: FMassProcessorTask(InEntityManager, InExecutionContext, InProc)
	{}

	static ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}
};

//----------------------------------------------------------------------//
// UMassProcessor 
//----------------------------------------------------------------------//
UMassProcessor::UMassProcessor(const FObjectInitializer& ObjectInitializer)
	: UMassProcessor()
{
}

UMassProcessor::UMassProcessor()
	: ExecutionFlags((int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone))
{
}

void UMassProcessor::SetShouldAutoRegisterWithGlobalList(const bool bAutoRegister)
{	
	if (ensureMsgf(HasAnyFlags(RF_ClassDefaultObject), TEXT("Setting bAutoRegisterWithProcessingPhases for non-CDOs has no effect")))
	{
		bAutoRegisterWithProcessingPhases = bAutoRegister;
#if WITH_EDITOR
		if (UClass* Class = GetClass())
		{
			if (FProperty* AutoRegisterProperty = Class->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMassProcessor, bAutoRegisterWithProcessingPhases)))
			{
				UpdateSinglePropertyInConfigFile(AutoRegisterProperty, *GetDefaultConfigFilename());
			}
		}
#endif // WITH_EDITOR
	}
}

void UMassProcessor::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject) == false) 
	{
		ConfigureQueries();

		bool bNeedsGameThread = false;
		for (FMassEntityQuery* QueryPtr : OwnedQueries)
		{
			CA_ASSUME(QueryPtr);
			bNeedsGameThread = (bNeedsGameThread || QueryPtr->DoesRequireGameThreadExecution());
		}
		// @todo warning temporarily disabled until details cleared
		/*UE_CLOG(bRequiresGameThreadExecution != bNeedsGameThread, LogMass, Warning, TEXT("%s is marked bRequiresGameThreadExecution = %s, while the registered quries' requirement indicate the opposite")
			, *GetProcessorName(), bRequiresGameThreadExecution ? TEXT("TRUE") : TEXT("FALSE"));*/

		// better safe than sorry - if queries indicate the game thread execution is required then we marked the whole processor as such
		bRequiresGameThreadExecution = bRequiresGameThreadExecution || bNeedsGameThread;
	}
#if CPUPROFILERTRACE_ENABLED
	StatId = GetProcessorName();
#endif
}

void UMassProcessor::CallExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*StatId);
	LLM_SCOPE_BYNAME(TEXT("Mass/ExecuteProcessor"));
	// Not using a more specific scope by default (i.e. LLM_SCOPE_BYNAME(*StatId)) since LLM is more strict regarding the provided string (no spaces or '_')

#if WITH_MASSENTITY_DEBUG
	Context.DebugSetExecutionDesc(FString::Printf(TEXT("%s (%s)"), *GetProcessorName(), EntityManager.GetWorld() ? *ToString(EntityManager.GetWorld()->GetNetMode()) : TEXT("No World")));
#endif
	// CacheSubsystemRequirements will return true only if all requirements declared with ProcessorRequirements are met
	// meaning if it fails there's no point in calling Execute.
	// Note that we're not testing individual queries in OwnedQueries - processors can function just fine with some 
	// of their queries not having anything to do.
	if (Context.CacheSubsystemRequirements(GetWorld(), ProcessorRequirements))
	{
		Execute(EntityManager, Context);
	}
	else
	{
		PROCESSOR_LOG(VeryVerbose, TEXT("%s Skipping Execute due to subsystem requirements not being met"), *GetProcessorName());
	}
}

void UMassProcessor::ExportRequirements(FMassExecutionRequirements& OutRequirements) const
{
	for (FMassEntityQuery* Query : OwnedQueries)
	{
		CA_ASSUME(Query);
		Query->ExportRequirements(OutRequirements);
	}
}

void UMassProcessor::RegisterQuery(FMassEntityQuery& Query)
{
	const uintptr_t ThisStart = (uintptr_t)this;
	const uintptr_t ThisEnd = ThisStart + GetClass()->GetStructureSize();
	const uintptr_t QueryStart = (uintptr_t)&Query;
	const uintptr_t QueryEnd = QueryStart + sizeof(FMassEntityQuery);

	if (QueryStart >= ThisStart && QueryEnd <= ThisEnd)
	{
		OwnedQueries.AddUnique(&Query);
	}
	else
	{
		constexpr TCHAR MessageFormat[] = TEXT("Registering entity query for %s while the query is not given processor's member variable. Skipping.");
		checkf(false, MessageFormat, *GetProcessorName());
		UE_LOG(LogMass, Error, MessageFormat, *GetProcessorName());
	}
}

FGraphEventRef UMassProcessor::DispatchProcessorTasks(const TSharedPtr<FMassEntityManager>& EntityManager, FMassExecutionContext& ExecutionContext, const FGraphEventArray& Prerequisites)
{
	FGraphEventRef ReturnVal;
	if (bRequiresGameThreadExecution)
	{
		ReturnVal = TGraphTask<FMassProcessorsTask_GameThread>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(EntityManager, ExecutionContext, *this);
	}
	else
	{
		ReturnVal = TGraphTask<FMassProcessorTask>::CreateTask(&Prerequisites).ConstructAndDispatchWhenReady(EntityManager, ExecutionContext, *this);
	}	
	return ReturnVal;
}

void UMassProcessor::DebugOutputDescription(FOutputDevice& Ar, int32 Indent) const
{
#if WITH_MASSENTITY_DEBUG
	Ar.Logf(TEXT("%*s%s"), Indent, TEXT(""), *GetProcessorName());
#endif // WITH_MASSENTITY_DEBUG
}

//----------------------------------------------------------------------//
//  UMassCompositeProcessor
//----------------------------------------------------------------------//
UMassCompositeProcessor::UMassCompositeProcessor()
	: GroupName(TEXT("None"))
{
	// not auto-registering composite processors since the idea of the global processors list is to indicate all 
	// the processors doing the work while composite processors are just containers. Having said that subclasses 
	// can change this behavior if need be.
	bAutoRegisterWithProcessingPhases = false;
}

void UMassCompositeProcessor::SetChildProcessors(TArray<UMassProcessor*>&& InProcessors)
{
	ChildPipeline.SetProcessors(MoveTemp(InProcessors));
}

void UMassCompositeProcessor::ConfigureQueries()
{
	// nothing to do here since ConfigureQueries will get automatically called for all the processors in ChildPipeline
	// via their individual PostInitProperties call
}

FGraphEventRef UMassCompositeProcessor::DispatchProcessorTasks(const TSharedPtr<FMassEntityManager>& EntityManager, FMassExecutionContext& ExecutionContext, const FGraphEventArray& InPrerequisites)
{
	FGraphEventArray Events;
	Events.Reserve(ProcessingFlatGraph.Num());
		
	for (FDependencyNode& ProcessingNode : ProcessingFlatGraph)
	{
		FGraphEventArray Prerequisites;
		for (const int32 DependencyIndex : ProcessingNode.Dependencies)
		{
			Prerequisites.Add(Events[DependencyIndex]);
		}

		// we don't expect any group nodes at this point. If we get any there's a bug in dependencies solving
		if (ensure(ProcessingNode.Processor))
		{
			Events.Add(ProcessingNode.Processor->DispatchProcessorTasks(EntityManager, ExecutionContext, Prerequisites));
		}
	}


#if WITH_MASSENTITY_DEBUG
	if (UE::Mass::Debug::bLogProcessingGraph)
	{
		for (int i = 0; i < ProcessingFlatGraph.Num(); ++i)
		{
			FDependencyNode& ProcessingNode = ProcessingFlatGraph[i];
			FString DependenciesDesc;
			for (const int32 DependencyIndex : ProcessingNode.Dependencies)
			{
				DependenciesDesc += FString::Printf(TEXT("%s, "), *ProcessingFlatGraph[DependencyIndex].Name.ToString());
			}

			check(ProcessingNode.Processor);
			PROCESSOR_TASK_LOG(TEXT("Task %u %s%s%s"), Events[i]->GetTraceId(), *ProcessingNode.Processor->GetProcessorName()
				, DependenciesDesc.Len() > 0 ? TEXT(" depends on ") : TEXT(""), *DependenciesDesc);
		}
	}
#endif // WITH_MASSENTITY_DEBUG

	FGraphEventRef CompletionEvent = FFunctionGraphTask::CreateAndDispatchWhenReady([this](){}
		, GET_STATID(Mass_GroupCompletedTask), &Events, ENamedThreads::AnyHiPriThreadHiPriTask);

	return CompletionEvent;
}

void UMassCompositeProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	for (UMassProcessor* Proc : ChildPipeline.Processors)
	{
		check(Proc);
		Proc->CallExecute(EntityManager, Context);
	}
}

void UMassCompositeProcessor::Initialize(UObject& Owner)
{
	// remove all nulls
	ChildPipeline.Processors.RemoveAll([](const UMassProcessor* Proc) { return Proc == nullptr; });

	// from this point on we don't expect to have nulls in ChildPipeline.Processors
	for (UMassProcessor* Proc : ChildPipeline.Processors)
	{
		REDIRECT_OBJECT_TO_VLOG(Proc, this);
		Proc->Initialize(Owner);
	}
}

void UMassCompositeProcessor::CopyAndSort(const FMassProcessingPhaseConfig& PhaseConfig, const FString& DependencyGraphFileName)
{
	check(GetOuter());
	
	// create processors via a temporary pipeline. This ensures consistency with other places we use FRuntimePipelines
	FMassRuntimePipeline TmpPipeline;
	TmpPipeline.CreateFromArray(PhaseConfig.ProcessorCDOs, *GetOuter());

	SetProcessors(TmpPipeline.Processors, DependencyGraphFileName);
}

void UMassCompositeProcessor::SetProcessors(TArrayView<UMassProcessor*> InProcessorInstances, const FString& DependencyGraphFileName)
{
	// figure out dependencies
	FProcessorDependencySolver Solver(InProcessorInstances, GroupName, DependencyGraphFileName);
	TArray<FProcessorDependencySolver::FOrderInfo> SortedProcessorsAndGroups;
	Solver.ResolveDependencies(SortedProcessorsAndGroups);

	Populate(SortedProcessorsAndGroups);

	// this part is creating an ordered, flat list of processors that can be executed in sequence
	// with subsequent task only depending on the elements prior on the list
	TMap<FName, int32> NameToDependencyIndex;
	NameToDependencyIndex.Reserve(SortedProcessorsAndGroups.Num());
	TArray<int32> SuperGroupDependency;
	for (FProcessorDependencySolver::FOrderInfo& Element : SortedProcessorsAndGroups)
	{
		NameToDependencyIndex.Add(Element.Name, ProcessingFlatGraph.Num());

		// we don't expect to get any "group" nodes here. If it happens it indicates a bug in dependency solving
		checkSlow(Element.Processor);
		FDependencyNode& Node = ProcessingFlatGraph.Add_GetRef({ Element.Name, Element.Processor });
		Node.Dependencies.Reserve(Element.Dependencies.Num());
		for (FName DependencyName : Element.Dependencies)
		{
			checkSlow(DependencyName.IsNone() == false);
			Node.Dependencies.Add(NameToDependencyIndex.FindChecked(DependencyName));
		}
	}

#if WITH_MASSENTITY_DEBUG
	FScopedCategoryAndVerbosityOverride LogOverride(TEXT("LogMass"), ELogVerbosity::Log);
	UE_LOG(LogMass, Log, TEXT("%s flat processing graph:"), *GroupName.ToString());

	TArray<int32> Offset;
	Offset.Reserve(ProcessingFlatGraph.Num());

	for (int i = 0; i < ProcessingFlatGraph.Num(); ++i)
	{
		FDependencyNode& ProcessingNode = ProcessingFlatGraph[i];
		FString DependenciesDesc;
		int32 MyOffset = 0;
		for (const int32 DependencyIndex : ProcessingNode.Dependencies)
		{
			DependenciesDesc += FString::Printf(TEXT("%d, "), DependencyIndex);
			MyOffset = FMath::Max(MyOffset, Offset[DependencyIndex]);
		}
		MyOffset += 2;
		Offset.Add(MyOffset);

		if (ProcessingNode.Processor)
		{
			UE_LOG(LogMass, Log, TEXT("[%2d]%*s%s%s%s"), i, MyOffset, TEXT(""), *ProcessingNode.Processor->GetProcessorName()
				, DependenciesDesc.Len() > 0 ? TEXT(" depends on ") : TEXT(""), *DependenciesDesc);
		}
	}
#endif // WITH_MASSENTITY_DEBUG
}

void UMassCompositeProcessor::Populate(TConstArrayView<FProcessorDependencySolver::FOrderInfo> OrderedProcessors)
{
	ChildPipeline.Processors.Reset();

	const FMassProcessingPhaseConfig& PhaseConfig = GET_MASS_CONFIG_VALUE(GetProcessingPhaseConfig(ProcessingPhase));

	for (const FProcessorDependencySolver::FOrderInfo& ProcessorInfo : OrderedProcessors)
	{
		if (ensureMsgf(ProcessorInfo.NodeType == EDependencyNodeType::Processor, TEXT("Encountered unexpected EDependencyNodeType while populating %s"), *GetGroupName().ToString()))
		{
			checkSlow(ProcessorInfo.Processor);
			
			ChildPipeline.AppendProcessor(*ProcessorInfo.Processor);
		}
	}
}

void UMassCompositeProcessor::DebugOutputDescription(FOutputDevice& Ar, int32 Indent) const
{
#if WITH_MASSENTITY_DEBUG
	if (ChildPipeline.Processors.Num() == 0)
	{
		Ar.Logf(TEXT("%*sGroup %s: []"), Indent, TEXT(""), *GroupName.ToString());
	}
	else
	{
		Ar.Logf(TEXT("%*sGroup %s:"), Indent, TEXT(""), *GroupName.ToString());
		for (UMassProcessor* Proc : ChildPipeline.Processors)
		{
			check(Proc);
			Ar.Logf(TEXT("\n"));
			Proc->DebugOutputDescription(Ar, Indent + 3);
		}
	}
#endif // WITH_MASSENTITY_DEBUG
}

void UMassCompositeProcessor::SetProcessingPhase(EMassProcessingPhase Phase)
{
	Super::SetProcessingPhase(Phase);
	for (UMassProcessor* Proc : ChildPipeline.Processors)
	{
		Proc->SetProcessingPhase(Phase);
	}
}

void UMassCompositeProcessor::SetGroupName(FName NewName)
{
	GroupName = NewName;
#if CPUPROFILERTRACE_ENABLED
	StatId = GroupName.ToString();
#endif
}

void UMassCompositeProcessor::AddGroupedProcessor(FName RequestedGroupName, UMassProcessor& Processor)
{
	if (RequestedGroupName.IsNone() || RequestedGroupName == GroupName)
	{
		ChildPipeline.AppendProcessor(Processor);
	}
	else
	{
		FString RemainingGroupName;
		UMassCompositeProcessor* GroupProcessor = FindOrAddGroupProcessor(RequestedGroupName, &RemainingGroupName);
		check(GroupProcessor);
		GroupProcessor->AddGroupedProcessor(FName(*RemainingGroupName), Processor);
	}
}

UMassCompositeProcessor* UMassCompositeProcessor::FindOrAddGroupProcessor(FName RequestedGroupName, FString* OutRemainingGroupName)
{
	UMassCompositeProcessor* GroupProcessor = nullptr;
	const FString NameAsString = RequestedGroupName.ToString();
	FString TopGroupName;
	if (NameAsString.Split(TEXT("."), &TopGroupName, OutRemainingGroupName))
	{
		RequestedGroupName = FName(*TopGroupName);
	}
	GroupProcessor = ChildPipeline.FindTopLevelGroupByName(RequestedGroupName);

	if (GroupProcessor == nullptr)
	{
		check(GetOuter());
		GroupProcessor = NewObject<UMassCompositeProcessor>(GetOuter());
		GroupProcessor->SetGroupName(RequestedGroupName);
		ChildPipeline.AppendProcessor(*GroupProcessor);
	}

	return GroupProcessor;
}

