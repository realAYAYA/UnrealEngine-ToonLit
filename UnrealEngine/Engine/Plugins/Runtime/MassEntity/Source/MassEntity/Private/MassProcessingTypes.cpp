// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "Misc/OutputDevice.h"
#include "MassEntityUtils.h"
#include "VisualLogger/VisualLogger.h"
#include "MassDebugger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassProcessingTypes)

DEFINE_LOG_CATEGORY(LogMass);

//----------------------------------------------------------------------//
//  FMassProcessingContext
//----------------------------------------------------------------------//
FMassProcessingContext::FMassProcessingContext(FMassEntityManager& InEntityManager, const float InDeltaSeconds)
	: EntityManager(InEntityManager.AsShared()), DeltaSeconds(InDeltaSeconds)
{

}

FMassProcessingContext::FMassProcessingContext(TSharedPtr<FMassEntityManager>& InEntityManager, const float InDeltaSeconds)
	: EntityManager(InEntityManager), DeltaSeconds(InDeltaSeconds)
{

}

FMassProcessingContext::~FMassProcessingContext()
{
	if (CommandBuffer && CommandBuffer.IsUnique() && CommandBuffer->HasPendingCommands())
	{
		UE_CLOG(!EntityManager, LogMass, Error, TEXT("Unable to auto-flush FMassProcessingContext\'s commands due to missing EntityManager"));
		if (ensure(EntityManager))
		{
			UE_VLOG(EntityManager->GetOwner(), LogMass, Log, TEXT("Auto-flushing command buffer as part of FMassProcessingContext destruction"));
			checkf(CommandBuffer->IsFlushing() == false, TEXT("A totally unexpected scenario."));
			EntityManager->FlushCommands(CommandBuffer);
		}
	}
}

//----------------------------------------------------------------------//
//  FMassRuntimePipeline
//----------------------------------------------------------------------//
void FMassRuntimePipeline::Reset()
{
	Processors.Reset();
}

void FMassRuntimePipeline::Initialize(UObject& Owner)
{
	// having nulls in Processors should be rare so we run the "remove all nulls" operation below only if we know 
	// for sure that there are any nulls to be removed
	bool bNullsFound = false;

	for (UMassProcessor* Proc : Processors)
	{
		if (Proc)
		{
			if (Proc->IsInitialized() == false)
			{
				REDIRECT_OBJECT_TO_VLOG(Proc, &Owner);
				Proc->Initialize(Owner);
				ensureMsgf(Proc->IsInitialized(), TEXT("Missing Super::Initialize call for %s"), *Proc->GetFullName());
			}
		}
		else
		{
			bNullsFound = true;
		}
	}

	if (bNullsFound)
	{
		Processors.RemoveAll([](const UMassProcessor* Proc) { return Proc == nullptr; });
	}
}

void FMassRuntimePipeline::SetProcessors(TArray<UMassProcessor*>&& InProcessors)
{
	Reset();
	Processors = InProcessors;
}

void FMassRuntimePipeline::CreateFromArray(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner)
{
	Reset();
	AppendOrOverrideRuntimeProcessorCopies(InProcessors, InOwner);
}

void FMassRuntimePipeline::InitializeFromArray(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner)
{
	CreateFromArray(InProcessors, InOwner);
	Initialize(InOwner);
}

void FMassRuntimePipeline::InitializeFromClassArray(TConstArrayView<TSubclassOf<UMassProcessor>> InProcessorClasses, UObject& InOwner)
{
	Reset();

	const UWorld* World = InOwner.GetWorld();
	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(World, ExecutionFlags);

	for (const TSubclassOf<UMassProcessor>& ProcessorClass : InProcessorClasses)
	{
		if (ProcessorClass)
		{
			UMassProcessor* CDO = ProcessorClass.GetDefaultObject();
			if (CDO && CDO->ShouldExecute(WorldExecutionFlags))
			{
				UMassProcessor* ProcInstance = NewObject<UMassProcessor>(&InOwner, ProcessorClass);
				Processors.Add(ProcInstance);
			}
			else
			{
				UE_CVLOG(CDO, &InOwner, LogMass, Log, TEXT("Skipping %s due to ExecutionFlags"), *CDO->GetName());
			}
		}
	}

	Initialize(InOwner);
}

bool FMassRuntimePipeline::HasProcessorOfExactClass(TSubclassOf<UMassProcessor> InClass) const
{
	UClass* TestClass = InClass.Get();
	return Processors.FindByPredicate([TestClass](const UMassProcessor* Proc){ return Proc != nullptr && Proc->GetClass() == TestClass; }) != nullptr;
}

void FMassRuntimePipeline::AppendUniqueRuntimeProcessorCopies(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner)
{
	const UWorld* World = InOwner.GetWorld();
	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(World, ExecutionFlags);
	const int32 StartingCount = Processors.Num();
		
	for (const UMassProcessor* Proc : InProcessors)
	{
		if (Proc && Proc->ShouldExecute(WorldExecutionFlags)
			&& (Proc->ShouldAllowMultipleInstances() || (HasProcessorOfExactClass(Proc->GetClass()) == false)))
		{
			// unfortunately the const cast is required since NewObject doesn't support const Template object
			UMassProcessor* ProcCopy = NewObject<UMassProcessor>(&InOwner, Proc->GetClass(), FName(), RF_NoFlags, const_cast<UMassProcessor*>(Proc));
			Processors.Add(ProcCopy);
		}
#if WITH_MASSENTITY_DEBUG
		else if (Proc)
		{
			if (Proc->ShouldExecute(WorldExecutionFlags) == false)
			{
				UE_VLOG(&InOwner, LogMass, Log, TEXT("Skipping %s due to ExecutionFlags"), *Proc->GetName());
			}
			else if (Proc->ShouldAllowMultipleInstances() == false)
			{
				UE_VLOG(&InOwner, LogMass, Log, TEXT("Skipping %s due to it being a duplicate"), *Proc->GetName());
			}
		}
#endif // WITH_MASSENTITY_DEBUG
	}

	for (int32 NewProcIndex = StartingCount; NewProcIndex < Processors.Num(); ++NewProcIndex)
	{
		UMassProcessor* Proc = Processors[NewProcIndex];
		check(Proc);
		
		if (Proc->IsInitialized() == false)
		{
			REDIRECT_OBJECT_TO_VLOG(Proc, &InOwner);
			Proc->Initialize(InOwner);
			ensureMsgf(Proc->IsInitialized(), TEXT("Missing Super::Initialize call for %s"), *Proc->GetFullName());
		}
	}
}

void FMassRuntimePipeline::AppendOrOverrideRuntimeProcessorCopies(TConstArrayView<const UMassProcessor*> InProcessors, UObject& InOwner)
{
	const UWorld* World = InOwner.GetWorld();
	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(World, ExecutionFlags);

	for (const UMassProcessor* Proc : InProcessors)
	{
		if (Proc && Proc->ShouldExecute(WorldExecutionFlags))
		{
			// unfortunately the const cast is required since NewObject doesn't support const Template object
			UMassProcessor* ProcCopy = NewObject<UMassProcessor>(&InOwner, Proc->GetClass(), FName(), RF_NoFlags, const_cast<UMassProcessor*>(Proc));
			check(ProcCopy);

			if (ProcCopy->ShouldAllowMultipleInstances())
			{
				// we don't care if there are instances of this class in Processors already
				Processors.Add(ProcCopy);
			}
			else 
			{
				const UClass* TestClass = Proc->GetClass();
				TObjectPtr<UMassProcessor>* PrevProcessor = Processors.FindByPredicate([TestClass, ProcCopy](const UMassProcessor* Proc) {
					return Proc != nullptr && Proc->GetClass() == TestClass;
				});

				if (PrevProcessor)
				{
					*PrevProcessor = ProcCopy;
				}
				else
				{
					Processors.Add(ProcCopy);
				}
			}
		}
		else
		{
			UE_CVLOG(Proc, &InOwner, LogMass, Log, TEXT("Skipping %s due to ExecutionFlags"), *Proc->GetName());
		}
	}
}

void FMassRuntimePipeline::AppendProcessor(UMassProcessor& Processor)
{
	Processors.Add(&Processor);
}

void FMassRuntimePipeline::AppendProcessor(TSubclassOf<UMassProcessor> ProcessorClass, UObject& InOwner)
{
	check(ProcessorClass);
	UMassProcessor* ProcInstance = NewObject<UMassProcessor>(&InOwner, ProcessorClass);
	AppendProcessor(*ProcInstance);
}

void FMassRuntimePipeline::RemoveProcessor(UMassProcessor& InProcessor)
{
	Processors.Remove(&InProcessor);
}

UMassCompositeProcessor* FMassRuntimePipeline::FindTopLevelGroupByName(FName GroupName)
{
	for (UMassProcessor* Processor : Processors)
	{
		UMassCompositeProcessor* CompositeProcessor = Cast<UMassCompositeProcessor>(Processor);
		if (CompositeProcessor && CompositeProcessor->GetGroupName() == GroupName)
		{
			return CompositeProcessor;
		}
	}
	return nullptr;
}

uint32 GetTypeHash(const FMassRuntimePipeline& Instance)
{ 
	uint32 Hash = 0;
	for (const UMassProcessor* Proc : Instance.Processors)
	{
		Hash = HashCombine(Hash, PointerHash(Proc));
	}
	return Hash;
}

