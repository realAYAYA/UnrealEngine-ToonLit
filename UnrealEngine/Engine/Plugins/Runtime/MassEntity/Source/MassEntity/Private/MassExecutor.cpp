// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassExecutor.h"
#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "MassCommandBuffer.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace UE::Mass::Executor
{
void Run(FMassRuntimePipeline& RuntimePipeline, FMassProcessingContext& ProcessingContext)
{
	if (!ensure(ProcessingContext.EntityManager) || 
		!ensure(ProcessingContext.DeltaSeconds >= 0.f) ||
		!ensure(RuntimePipeline.Processors.Find(nullptr) == INDEX_NONE))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor Run Pipeline")
	RunProcessorsView(RuntimePipeline.Processors, ProcessingContext);
}

void RunSparse(FMassRuntimePipeline& RuntimePipeline, FMassProcessingContext& ProcessingContext, FMassArchetypeHandle Archetype, TConstArrayView<FMassEntityHandle> Entities)
{
	if (!ensure(ProcessingContext.EntityManager) ||
		!ensure(RuntimePipeline.Processors.Find(nullptr) == INDEX_NONE) ||
		RuntimePipeline.Processors.Num() == 0 ||
		!ensureMsgf(Archetype.IsValid(), TEXT("The Archetype passed in to UE::Mass::Executor::RunSparse is invalid")))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor RunSparseEntities");

	const FMassArchetypeEntityCollection EntityCollection(Archetype, Entities, FMassArchetypeEntityCollection::NoDuplicates);
	RunProcessorsView(RuntimePipeline.Processors, ProcessingContext, &EntityCollection);
}

void RunSparse(FMassRuntimePipeline& RuntimePipeline, FMassProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection)
{
	if (!ensure(ProcessingContext.EntityManager) ||
		!ensure(RuntimePipeline.Processors.Find(nullptr) == INDEX_NONE) ||
		RuntimePipeline.Processors.Num() == 0 ||
		!ensureMsgf(EntityCollection.GetArchetype().IsValid(), TEXT("The Archetype of EntityCollection passed in to UE::Mass::Executor::RunSparse is invalid")))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor RunSparse");

	RunProcessorsView(RuntimePipeline.Processors, ProcessingContext, &EntityCollection);
}

void Run(UMassProcessor& Processor, FMassProcessingContext& ProcessingContext)
{
	if (!ensure(ProcessingContext.EntityManager) || !ensure(ProcessingContext.DeltaSeconds >= 0.f))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor Run")

	UMassProcessor* ProcPtr = &Processor;
	RunProcessorsView(MakeArrayView(&ProcPtr, 1), ProcessingContext);
}

void RunProcessorsView(TArrayView<UMassProcessor*> Processors, FMassProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection* EntityCollection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RunProcessorsView);

	if (!ProcessingContext.EntityManager)
	{
		UE_LOG(LogMass, Error, TEXT("%s ProcessingContext.EntityManager is null. Baling out."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
#if WITH_MASSENTITY_DEBUG
	if (Processors.Find(nullptr) != INDEX_NONE)
	{
		UE_LOG(LogMass, Error, TEXT("%s input Processors contains nullptr. Baling out."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
#endif // WITH_MASSENTITY_DEBUG

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor RunProcessorsView")

	FMassExecutionContext ExecutionContext(ProcessingContext.EntityManager, ProcessingContext.DeltaSeconds);
	if (EntityCollection)
	{
		ExecutionContext.SetEntityCollection(*EntityCollection);
	}
	
	// if ProcessingContext points at a valid CommandBuffer use that one, otherwise manually create a new command buffer 
	// to let the default one still be used by code unaware of mass processing
	TSharedPtr<FMassCommandBuffer> CommandBuffer = ProcessingContext.CommandBuffer 
		? ProcessingContext.CommandBuffer : MakeShareable(new FMassCommandBuffer());
	ExecutionContext.SetDeferredCommandBuffer(CommandBuffer);
	ExecutionContext.SetFlushDeferredCommands(false);
	ExecutionContext.SetAuxData(ProcessingContext.AuxData);
	ExecutionContext.SetExecutionType(EMassExecutionContextType::Processor);

	FMassEntityManager& EntityManager = *ProcessingContext.EntityManager.Get();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Execute Processors")
		
		FMassEntityManager::FScopedProcessing ProcessingScope = EntityManager.NewProcessingScope();

		for (UMassProcessor* Proc : Processors)
		{
			Proc->CallExecute(*ProcessingContext.EntityManager, ExecutionContext);
		}
	}
	
	if (ProcessingContext.bFlushCommandBuffer)
	{		
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Flush Deferred Commands")
		
		ExecutionContext.SetFlushDeferredCommands(true);
		// append the commands added from other, non-processor sources (like MassAgentSubsystem)
		ensure(!EntityManager.Defer().IsFlushing());
		ensure(!ExecutionContext.Defer().IsFlushing());
		ExecutionContext.Defer().MoveAppend(EntityManager.Defer());
		ExecutionContext.FlushDeferred();
	}
	// else make sure we don't just lose the commands. Append to the command buffer requested via
	// ProcessingContext.CommandBuffer or to the default EntityManager's command buffer.
	else if (CommandBuffer != ProcessingContext.CommandBuffer)
	{
		if (ProcessingContext.CommandBuffer)
		{
			ProcessingContext.CommandBuffer->MoveAppend(*CommandBuffer.Get());
		}
		else if (CommandBuffer.Get() != &EntityManager.Defer())
		{
			EntityManager.Defer().MoveAppend(*CommandBuffer.Get());
		}
	}
}

struct FMassExecutorDoneTask
{
	FMassExecutorDoneTask(const FMassExecutionContext& InExecutionContext, TFunction<void()> InOnDoneNotification, const FString& InDebugName)
		: ExecutionContext(InExecutionContext)
		, OnDoneNotification(InOnDoneNotification)
		, DebugName(InDebugName)
	{
	}
	static TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMassExecutorDoneTask, STATGROUP_TaskGraphTasks);
	}

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Flush Deferred Commands Parallel");

		FMassEntityManager& EntityManagerRef = ExecutionContext.GetEntityManagerChecked();

		if (&ExecutionContext.Defer() != &EntityManagerRef.Defer())
		{
			ExecutionContext.Defer().MoveAppend(EntityManagerRef.Defer());
		}

		UE_LOG(LogMass, Verbose, TEXT("MassExecutor %s tasks DONE"), *DebugName);
		ExecutionContext.SetFlushDeferredCommands(true);
		ExecutionContext.FlushDeferred();

		OnDoneNotification();
	}
private:
	FMassExecutionContext ExecutionContext;
	TFunction<void()> OnDoneNotification;
	FString DebugName;
};

FGraphEventRef TriggerParallelTasks(UMassProcessor& Processor, FMassProcessingContext& ProcessingContext, TFunction<void()> OnDoneNotification)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RunProcessorsView);

	if (!ProcessingContext.EntityManager)
	{
		UE_LOG(LogMass, Error, TEXT("%s ProcessingContext.EntityManager is null. Baling out."), ANSI_TO_TCHAR(__FUNCTION__));
		return FGraphEventRef();
	}

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassExecutor RunParallel")

	// not going through FMassEntityManager::CreateExecutionContext on purpose - we do need a separate command buffer
	FMassExecutionContext ExecutionContext(ProcessingContext.EntityManager, ProcessingContext.DeltaSeconds);
	TSharedPtr<FMassCommandBuffer> CommandBuffer = ProcessingContext.CommandBuffer
		? ProcessingContext.CommandBuffer : MakeShareable(new FMassCommandBuffer());
	ExecutionContext.SetDeferredCommandBuffer(CommandBuffer);
	ExecutionContext.SetFlushDeferredCommands(false);
	ExecutionContext.SetAuxData(ProcessingContext.AuxData);
	ExecutionContext.SetExecutionType(EMassExecutionContextType::Processor);

	FGraphEventRef CompletionEvent;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Dispatch Processors")
		CompletionEvent = Processor.DispatchProcessorTasks(ProcessingContext.EntityManager, ExecutionContext, {});
	}

	if (CompletionEvent.IsValid())
	{
		const FGraphEventArray Prerequisites = { CompletionEvent };
		CompletionEvent = TGraphTask<FMassExecutorDoneTask>::CreateTask(&Prerequisites)
			.ConstructAndDispatchWhenReady(ExecutionContext, OnDoneNotification, Processor.GetName());
	}

	return CompletionEvent;
}

} // namespace UE::Mass::Executor
