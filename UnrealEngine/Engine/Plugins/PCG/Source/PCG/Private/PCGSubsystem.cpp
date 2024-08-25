// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubsystem.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGWorldActor.h"
#include "Graph/PCGGraphCache.h"
#include "Graph/PCGGraphCompiler.h"
#include "Graph/PCGGraphExecutor.h"
#include "Graph/PCGStackContext.h"
#include "Grid/PCGLandscapeCache.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "RuntimeGen/PCGGenSourceManager.h"
#include "RuntimeGen/PCGRuntimeGenScheduler.h"

#include "Engine/Level.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSubsystem)

#if WITH_EDITOR
#include "Editor.h"
#include "PackageSourceControlHelper.h"
#include "ObjectTools.h"
#else
#include "Engine/Engine.h"
#include "Engine/World.h"
#endif

namespace PCGSubsystemConsole
{
	static FAutoConsoleCommand CommandFlushCache(
		TEXT("pcg.FlushCache"),
		TEXT("Clears the PCG results cache and compiled graph cache."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
				{
					PCGSubsystem->FlushCache();
				}
			}));

	TAutoConsoleVariable<int32> CVarPCGQuality(
		TEXT("pcg.Quality"), 2,
		TEXT("Selects the quality permutation of PCG which impacts Runtime Quality Branch/Select nodes.\n")
		TEXT(" 0: Low\n")
		TEXT(" 1: Medium\n")
		TEXT(" 2: High\n")
		TEXT(" 3: Epic\n")
		TEXT(" 4: Cinematic\n"),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* CVar)
		{
			if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
			{
				Subsystem->OnPCGQualityLevelChanged();
			}
		}),
		ECVF_Scalability);

#if WITH_EDITOR
	static FAutoConsoleCommand CommandBuildLandscapeCache(
		TEXT("pcg.BuildLandscapeCache"),
		TEXT("Builds the landscape cache in the current world."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
				{
					PCGSubsystem->BuildLandscapeCache();
				}
			}));

	static FAutoConsoleCommand CommandClearLandscapeCache(
		TEXT("pcg.ClearLandscapeCache"),
		TEXT("Clear the landscape cache in the current world."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
				{
					PCGSubsystem->ClearLandscapeCache();
				}
			}));

	static TAutoConsoleVariable<bool> CVarRebuildLandscapeOnPIE(
		TEXT("pcg.PIE.RebuildLandscapeOnPIE"),
		true,
		TEXT("Controls whether the landscape cache will be rebuilt on PIE"));

	static FAutoConsoleCommand CommandDeleteCurrentPCGWorldActor(
		TEXT("pcg.DeleteCurrentPCGWorldActor"),
		TEXT("Deletes the PCG World Actor currently registered to the PCG Subsystem."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if(UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
			{
				PCGSubsystem->DestroyCurrentPCGWorldActor();
			}
		}));

	static FAutoConsoleCommand CommandDeleteAllPCGWorldActors(
		TEXT("pcg.DeleteAllPCGWorldActors"),
		TEXT("Deletes all PCG World Actors in current World.."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetSubsystemForCurrentWorld())
			{
				PCGSubsystem->DestroyAllPCGWorldActors();
			}
		}));
#endif
}

UPCGSubsystem::UPCGSubsystem()
	: Super()
	, ActorAndComponentMapping(this)
{
}

UPCGSubsystem* UPCGSubsystem::GetSubsystemForCurrentWorld()
{
	UWorld* World = nullptr;

#if WITH_EDITOR
	if (GEditor)
	{
		if (GEditor->PlayWorld)
		{
			World = GEditor->PlayWorld;
		}
		else
		{
			World = GEditor->GetEditorWorldContext().World();
		}
	}
	else
#endif
	if (GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	return UPCGSubsystem::GetInstance(World);
}

void UPCGSubsystem::Deinitialize()
{
	// Cancel all tasks
	// TODO
	delete GraphExecutor;
	GraphExecutor = nullptr;

	delete RuntimeGenScheduler;
	RuntimeGenScheduler = nullptr;

	PCGWorldActor = nullptr;
	bHasTickedOnce = false;

	ActorAndComponentMapping.Deinitialize();

	Super::Deinitialize();
}

void UPCGSubsystem::PostInitialize()
{
	Super::PostInitialize();

	ActorAndComponentMapping.Initialize(GetWorld());

	// Initialize graph executor
	check(!GraphExecutor);
	GraphExecutor = new FPCGGraphExecutor();

	// Initialize runtime generation scheduler
	check(!RuntimeGenScheduler);
	RuntimeGenScheduler = new FPCGRuntimeGenScheduler(GetWorld(), &ActorAndComponentMapping);

	// Gather world pcg actor if it exists
	if (!PCGWorldActor)
	{
		if (UWorld* World = GetWorld())
		{
			UPCGActorHelpers::ForEachActorInWorld<APCGWorldActor>(World, [this](AActor* InActor)
			{
				PCGWorldActor = Cast<APCGWorldActor>(InActor);
				return PCGWorldActor == nullptr;
			});
		}
	}
}

UPCGSubsystem* UPCGSubsystem::GetInstance(UWorld* World)
{
	if (World)
	{
		UPCGSubsystem* Subsystem = World->GetSubsystem<UPCGSubsystem>();
		return (Subsystem && Subsystem->IsInitialized()) ? Subsystem : nullptr;
	}
	else
	{
		return nullptr;
	}
}

#if WITH_EDITOR
UPCGSubsystem* UPCGSubsystem::GetActiveEditorInstance()
{
	if (GEditor)
	{
		return GEditor->PlayWorld ? UPCGSubsystem::GetInstance(GEditor->PlayWorld.Get()) : UPCGSubsystem::GetInstance(GEditor->GetEditorWorldContext().World());
	}

	return nullptr;
}

void UPCGSubsystem::SetConstructionScriptSourceComponent(UPCGComponent* InComponent)
{
	if (InComponent)
	{
		if (AActor* Owner = InComponent->GetOwner())
		{
			PerActorConstructionScriptSourceComponents.FindOrAdd(Owner).Add(InComponent->GetFName(), InComponent);
		}
	}
}

bool UPCGSubsystem::RemoveAndCopyConstructionScriptSourceComponent(AActor* InComponentOwner, FName InComponentName, UPCGComponent*& OutSourceComponent)
{
	OutSourceComponent = nullptr;
	if (FConstructionScriptSourceComponents* Found = PerActorConstructionScriptSourceComponents.Find(InComponentOwner))
	{
		TObjectKey<UPCGComponent> FoundComponent;
		if (Found->RemoveAndCopyValue(InComponentName, FoundComponent))
		{
			OutSourceComponent = FoundComponent.ResolveObjectPtrEvenIfGarbage();
			if (Found->IsEmpty())
			{
				PerActorConstructionScriptSourceComponents.Remove(InComponentOwner);
			}
		}
	}

	return OutSourceComponent != nullptr;
}
#endif

void UPCGSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR
	PerActorConstructionScriptSourceComponents.Empty();
#endif

	if (!bHasTickedOnce)
	{
#if WITH_EDITOR
		if (PCGSubsystemConsole::CVarRebuildLandscapeOnPIE.GetValueOnAnyThread() && PCGHelpers::IsRuntimeOrPIE())
		{
			BuildLandscapeCache(/*bQuiet=*/true);
		}
#endif

		bHasTickedOnce = true;
	}

	// Lose references to landscape cache as needed
	// This will also initialize the cache if it isn't already so needs to happen before the GraphExecutor->Executor call
	if (PCGWorldActor && GetLandscapeCache())
	{
		GetLandscapeCache()->Tick(DeltaSeconds);
	}
	
	// If we have any tasks to execute, schedule some
	GraphExecutor->Execute();

	ActorAndComponentMapping.Tick();

	if (PCGWorldActor)
	{
		RuntimeGenScheduler->Tick(PCGWorldActor);
	}
}

APCGWorldActor* UPCGSubsystem::GetPCGWorldActor()
{
#if WITH_EDITOR
	if (!PCGWorldActor && !PCGHelpers::IsRuntimeOrPIE())
	{
		PCGWorldActorLock.Lock();
		if (!PCGWorldActor)
		{
			PCGWorldActor = APCGWorldActor::CreatePCGWorldActor(GetWorld());
		}
		PCGWorldActorLock.Unlock();
	}
#endif

	return PCGWorldActor;
}

APCGWorldActor* UPCGSubsystem::FindPCGWorldActor()
{
	return PCGWorldActor;
}

int32 UPCGSubsystem::GetPCGQualityLevel()
{
	return PCGSubsystemConsole::CVarPCGQuality.GetValueOnAnyThread();
}

void UPCGSubsystem::OnPCGQualityLevelChanged()
{
	// Trigger deep refresh of all runtime generated components.
	ActorAndComponentMapping.ForAllOriginalComponents([this](UPCGComponent* InComponent)
	{
		if (InComponent && InComponent->IsManagedByRuntimeGenSystem())
		{
#if WITH_EDITOR
			if (UPCGGraph* Graph = InComponent->GetGraph())
			{
				Graph->OnPCGQualityLevelChanged();
			}
#else
			RefreshRuntimeGenComponent(InComponent, EPCGChangeType::GenerationGrid);
#endif

		}
	});
}

#if WITH_EDITOR
void UPCGSubsystem::DestroyCurrentPCGWorldActor()
{
	if (PCGWorldActor)
	{
		PCGWorldActorLock.Lock();
		PCGWorldActor->Destroy();
		PCGWorldActor = nullptr;
		PCGWorldActorLock.Unlock();
	}
}

void UPCGSubsystem::DestroyAllPCGWorldActors()
{
	// Delete all PAs first to avoid leaving orphans behind.
	DeleteSerializedPartitionActors(/*bOnlyDeleteUnused=*/false);

	// Get rid of current PCG world actor first
	DestroyCurrentPCGWorldActor();
	
	// Pick up any strays in the current world
	TArray<APCGWorldActor*> ActorsToDestroy;
	ForEachObjectWithOuter(GetWorld(), [&ActorsToDestroy](UObject* Object)
	{
		if (APCGWorldActor* WorldActor = Cast<APCGWorldActor>(Object))
		{
			if (IsValid(WorldActor))
			{
				ActorsToDestroy.Add(WorldActor);
			}
		}
	});

	for (APCGWorldActor* ActorToDestroy : ActorsToDestroy)
	{
		ActorToDestroy->Destroy();
	}
}

void UPCGSubsystem::LogAbnormalComponentStates(bool bGroupByState) const
{
	TArray<UPCGComponent*> DeactivatedComponents;
	TArray<UPCGComponent*> NotGeneratedComponents;
	TArray<UPCGComponent*> DirtyGeneratedComponents;

	UE_LOG(LogPCG, Log, TEXT("--- Logging Abnormal PCG Component States ---"));

	UPCGActorHelpers::ForEachActorInWorld(GetWorld(), AActor::StaticClass(), [bGroupByState, &DeactivatedComponents, &NotGeneratedComponents, &DirtyGeneratedComponents](AActor* InActor)
	{
		if (!InActor || !IsValid(InActor))
		{
			return true;
		}

		TInlineComponentArray<UPCGComponent*, 1> PCGComponents;
		InActor->GetComponents(PCGComponents);

		for (UPCGComponent* PCGComponent : PCGComponents)
		{
			if (!PCGComponent->bActivated)
			{
				if (bGroupByState)
				{
					DeactivatedComponents.Add(PCGComponent);
				}
				else
				{
					UE_LOG(LogPCG, Log, TEXT("%s - %s - Deactivated Component"), *InActor->GetName(), *PCGComponent->GetName());
				}
			}
			else if (!PCGComponent->bGenerated && PCGComponent->GenerationTrigger != EPCGComponentGenerationTrigger::GenerateAtRuntime)
			{
				if (bGroupByState)
				{
					NotGeneratedComponents.Add(PCGComponent);
				}
				else
				{
					UE_LOG(LogPCG, Log, TEXT("%s - %s - Not Generated Component"), *InActor->GetName(), *PCGComponent->GetName());
				}
			}
			else if (PCGComponent->bDirtyGenerated)
			{
				if (bGroupByState)
				{
					DirtyGeneratedComponents.Add(PCGComponent);
				}
				else
				{
					UE_LOG(LogPCG, Log, TEXT("%s - %s - Dirty Generated Component "), *InActor->GetName(), *PCGComponent->GetName());
				}
			}
		}

		return true;
	});

	if (bGroupByState)
	{
		UE_LOG(LogPCG, Log, TEXT("--- Deactivated PCG Components ---"));
		for (UPCGComponent* Component : DeactivatedComponents)
		{
			check(Component && Component->GetOwner());
			UE_LOG(LogPCG, Log, TEXT("%s - %s"), *Component->GetOwner()->GetName(), *Component->GetName());
		}

		UE_LOG(LogPCG, Log, TEXT("--- Not Generated Components ---"));
		for (UPCGComponent* Component : NotGeneratedComponents)
		{
			check(Component && Component->GetOwner());
			UE_LOG(LogPCG, Log, TEXT("%s - %s"), *Component->GetOwner()->GetName(), *Component->GetName());
		}

		UE_LOG(LogPCG, Log, TEXT("--- Dirty Components ---"));
		for (UPCGComponent* Component : DirtyGeneratedComponents)
		{
			check(Component && Component->GetOwner());
			UE_LOG(LogPCG, Log, TEXT("%s - %s"), *Component->GetOwner()->GetName(), *Component->GetName());
		}
	}
}

#endif // WITH_EDITOR

void UPCGSubsystem::RegisterPCGWorldActor(APCGWorldActor* InActor)
{
	// TODO: we should support merging or multi world actor support when relevant
	if (!PCGWorldActor)
	{
		PCGWorldActor = InActor;
	}
	else if (InActor != PCGWorldActor)
	{
		PCGWorldActor->MergeFrom(InActor);
	}
}

void UPCGSubsystem::UnregisterPCGWorldActor(APCGWorldActor* InActor)
{
	if (PCGWorldActor == InActor)
	{
		PCGWorldActor = nullptr;
	}
}

void UPCGSubsystem::OnOriginalComponentRegistered(UPCGComponent* InComponent)
{
	if (RuntimeGenScheduler)
	{
		RuntimeGenScheduler->OnOriginalComponentRegistered(InComponent);
	}
}

void UPCGSubsystem::OnOriginalComponentUnregistered(UPCGComponent* InComponent)
{
	if (RuntimeGenScheduler)
	{
		RuntimeGenScheduler->OnOriginalComponentUnregistered(InComponent);
	}

#if WITH_EDITOR
	ClearExecutionMetadata(InComponent);
#endif
}

#if WITH_EDITOR
void UPCGSubsystem::OnScheduleGraph(const FPCGStackContext& StackContext)
{
	// Always clear any possibly related warnings/errors on schedule.
	for (const FPCGStack& Stack : StackContext.GetStacks())
	{
		GetNodeVisualLogsMutable().ClearLogs(Stack);
	}

	// Flush out all stacks that begin from the component / top graph as the existing dynamic
	// stacks may not be occur during the next execution.
	if (const FPCGStack* BaseStack = StackContext.GetStack(0))
	{
		if (BaseStack->IsCurrentFrameInRootGraph())
		{
			ClearExecutionMetadata(*BaseStack);
		}
	}

	// Record executed stacks.
	{
		FWriteScopeLock Lock(ExecutedStacksLock);
		for (const FPCGStack& ExecutedStack : StackContext.GetStacks())
		{
			ExecutedStacks.AddUnique(ExecutedStack);
		}
	}
}
#endif

UPCGLandscapeCache* UPCGSubsystem::GetLandscapeCache()
{
	APCGWorldActor* LandscapeCacheOwner = GetPCGWorldActor();
	return LandscapeCacheOwner ? LandscapeCacheOwner->LandscapeCacheObject.Get() : nullptr;
}

FPCGTaskId UPCGSubsystem::ScheduleComponent(UPCGComponent* PCGComponent, EPCGHiGenGrid Grid, bool bForce, const TArray<FPCGTaskId>& InDependencies)
{
	check(GraphExecutor);

	if (!PCGComponent)
	{
		return InvalidPCGTaskId;
	}

	bool bHasUnbounded = false;
	PCGHiGenGrid::FSizeArray GridSizes;
	ensure(PCGHelpers::GetGenerationGridSizes(PCGComponent->GetGraph(), GetPCGWorldActor(), GridSizes, bHasUnbounded));

	// Create the PartitionActors if necessary. Skip if this is a runtime managed component, PAs are handled manually by the RuntimeGenScheduler.
	if (PCGComponent->IsPartitioned() && !PCGComponent->IsManagedByRuntimeGenSystem())
	{
		if (!GridSizes.IsEmpty())
		{
			// In this case create the PA and update the mapping
			// Note: This is an immediate operation, as we need the PA for the generation.
			auto ScheduleTask = [](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds) { return InvalidPCGTaskId; };

			ForAllOverlappingCells(PCGComponent->GetGridBounds(), GridSizes, /*bCanCreateActor=*/true, /*Dependencies=*/{}, ScheduleTask);
		}

		ActorAndComponentMapping.UpdateMappingPCGComponentPartitionActor(PCGComponent);
	}

	// Execution dependencies require a task to finish executing before the dependent task.
	TArray<FPCGTaskId> ExecutionDependencyTasks;

	// Data dependencies act as execution dependencies, but will also have their output consumed by the waiting task. For a component, this means
	// it will store the output data into its managed resources, which, for an original component, should not include the local component generation tasks,
	// since those resources should be managed locally.
	TArray<FPCGTaskId> DataDependencyTasks;

	// Schedule generation of original component if is is non-partitioned, or if it has nodes that will execute at the Unbounded level.
	FPCGTaskId OriginalComponentTask = InvalidPCGTaskId;

	bool bGeneratePCGComponent = false;
	if (!PCGComponent->IsPartitioned())
	{
		// This component is either an unpartitioned original component or a local component. Generate if grid size matches preference (if provided).
		bGeneratePCGComponent = (Grid == EPCGHiGenGrid::Uninitialized) || !!(Grid & PCGComponent->GetGenerationGrid());
	}
	else
	{
		// This component is a partitioned original component. Generate if the graph has unbounded nodes and if this grid matches preference (if provided).
		bGeneratePCGComponent = bHasUnbounded && (!!(Grid & EPCGHiGenGrid::Unbounded) || Grid == EPCGHiGenGrid::Uninitialized);
	}

	if (bGeneratePCGComponent)
	{
		OriginalComponentTask = PCGComponent->CreateGenerateTask(bForce, InDependencies);
		if (OriginalComponentTask != InvalidPCGTaskId)
		{
			DataDependencyTasks.Add(OriginalComponentTask);
		}
	}

	// If the component is partitioned, we will forward the calls to its registered PCG Partition actors
	if (PCGComponent->IsPartitioned() && PCGHiGenGrid::IsValidGridOrUninitialized(Grid))
	{
		// Local components depend on the original component (to ensure any data is available).
		TArray<FPCGTaskId> Dependencies = InDependencies;
		if (OriginalComponentTask != InvalidPCGTaskId)
		{
			Dependencies.Add(OriginalComponentTask);
		}

		auto LocalGenerateTask = [OriginalComponent = PCGComponent, Grid, &Dependencies, bForce, &GridSizes](UPCGComponent* LocalComponent)
		{
			if (!GridSizes.Contains(LocalComponent->GetGenerationGridSize()))
			{
				// Local component with invalid grid size. Grid sizes may have changed in graph.
				return LocalComponent->CleanupInternal(/*bRemoveComponents=*/true, Dependencies);
			}
			else if (Grid != EPCGHiGenGrid::Uninitialized && !(Grid & LocalComponent->GetGenerationGrid()))
			{
				// Grid size does not match the given target grid, so skip.
				return InvalidPCGTaskId;
			}

			// If the local component is currently generating, it's probably because it was requested by a refresh.
			// Wait after this one instead
			if (LocalComponent->IsGenerating())
			{
				return LocalComponent->CurrentGenerationTask;
			}

			// Ensure that the PCG actor match our original
			LocalComponent->SetPropertiesFromOriginal(OriginalComponent);

			if (LocalComponent->bGenerated && !OriginalComponent->bGenerated)
			{
				// Detected a mismatch between the original component and the local component.
				// Request a cleanup first
				LocalComponent->CleanupLocalImmediate(true);
			}

			return LocalComponent->GenerateInternal(bForce, LocalComponent->GetGenerationGrid(), EPCGComponentGenerationTrigger::GenerateOnDemand, Dependencies);
		};

		ExecutionDependencyTasks.Append(ActorAndComponentMapping.DispatchToRegisteredLocalComponents(PCGComponent, LocalGenerateTask));
	}

	if (!ExecutionDependencyTasks.IsEmpty() || !DataDependencyTasks.IsEmpty())
	{
		TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);

		return GraphExecutor->ScheduleGenericWithContext([ComponentPtr](FPCGContext* Context)
		{
			if (UPCGComponent* Component = ComponentPtr.Get())
			{
				// If the component is not valid anymore, just early out.
				if (!IsValid(Component))
				{
					return true;
				}

				const FBox NewBounds = Component->GetGridBounds();
				Component->PostProcessGraph(NewBounds, /*bGenerate=*/true, Context);
			}

			return true;
		}, PCGComponent, ExecutionDependencyTasks, DataDependencyTasks);
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("[ScheduleComponent] Didn't schedule any task."));
		if (PCGComponent)
		{
			PCGComponent->OnProcessGraphAborted();
		}
		return InvalidPCGTaskId;
	}
}

FPCGTaskId UPCGSubsystem::ScheduleCleanup(UPCGComponent* PCGComponent, bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies)
{
	if (!PCGComponent)
	{
		return InvalidPCGTaskId;
	}

	bool bHasUnbounded = false;
	PCGHiGenGrid::FSizeArray GridSizes;
	ensure(PCGHelpers::GetGenerationGridSizes(PCGComponent->GetGraph(), GetPCGWorldActor(), GridSizes, bHasUnbounded));

	TArray<FPCGTaskId> AllTasks;

	// Schedule cleanup of original component if is is non-partitioned, or if it has nodes that will execute at the Unbounded level.
	if (!PCGComponent->IsPartitioned() || bHasUnbounded)
	{
		FPCGTaskId TaskId = PCGComponent->CreateCleanupTask(bRemoveComponents, Dependencies);
		if (TaskId != InvalidPCGTaskId)
		{
			AllTasks.Add(TaskId);
		}
	}

	// If the component is partitioned, we will forward the calls to its registered PCG Partition actors
	if (PCGComponent->IsPartitioned())
	{
		auto LocalCleanupTask = [bRemoveComponents, &Dependencies](UPCGComponent* LocalComponent)
		{
			// If the local component is currently cleaning up, it's probably because it was requested by a refresh.
			// Wait after this one instead
			if (LocalComponent->IsCleaningUp())
			{
				return LocalComponent->CurrentCleanupTask;
			}

			// Always executes regardless of local component grid size - clean up as much as possible.
			return LocalComponent->CleanupInternal(bRemoveComponents, Dependencies);
		};

		AllTasks.Append(ActorAndComponentMapping.DispatchToRegisteredLocalComponents(PCGComponent, LocalCleanupTask));
	}

	TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);
	auto PostCleanupTask = [this, bRemoveComponents, ComponentPtr]()
	{
		if (UPCGComponent* Component = ComponentPtr.Get())
		{
			// If the component is not valid anymore, just early out
			if (!IsValid(Component))
			{
				return true;
			}

			Component->PostCleanupGraph();

			// Remove the local component mappings if requested and the component is partitioned. If 'bRemoveComponents' is false, that indicates we are doing a refresh, so destroying
			// the component mappings is counterproductive.
			if (bRemoveComponents && Component->IsPartitioned())
			{
				ActorAndComponentMapping.DeleteMappingPCGComponentPartitionActor(Component);
			}
		}

		return true;
	};

	FPCGTaskId PostCleanupTaskId = InvalidPCGTaskId;

	// If we have no tasks to do, just call PostCleanup immediately
	// otherwise wait for all the tasks to be done to call PostCleanup.
	if (AllTasks.IsEmpty())
	{
		PostCleanupTask();
	}
	else
	{
		PostCleanupTaskId = GraphExecutor->ScheduleGeneric(PostCleanupTask, PCGComponent, AllTasks);
	}

	return PostCleanupTaskId;
}

FPCGTaskId UPCGSubsystem::ScheduleGraph(UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& Dependencies)
{
	if (SourceComponent)
	{
		return GraphExecutor->Schedule(SourceComponent, Dependencies);
	}
	else
	{
		return InvalidPCGTaskId;
	}
}

FPCGTaskId UPCGSubsystem::ScheduleGraph(
	UPCGGraph* Graph,
	UPCGComponent* SourceComponent,
	FPCGElementPtr PreGraphElement, 
	FPCGElementPtr InputElement, 
	const TArray<FPCGTaskId>& Dependencies, 
	const FPCGStack* InFromStack, 
	bool bAllowHierarchicalGeneration)
{
	if (SourceComponent)
	{
		return GraphExecutor->Schedule(Graph, SourceComponent, PreGraphElement, InputElement, Dependencies, InFromStack, bAllowHierarchicalGeneration);
	}
	else
	{
		return InvalidPCGTaskId;
	}
}

ETickableTickType UPCGSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always;
}

TStatId UPCGSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPCGSubsystem, STATGROUP_Tickables);
}

FPCGTaskId UPCGSubsystem::ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGeneric(InOperation, SourceComponent, TaskExecutionDependencies);
}

FPCGTaskId UPCGSubsystem::ScheduleGeneric(TFunction<bool()> InOperation, TFunction<void()> InAbortOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGeneric(InOperation, InAbortOperation, SourceComponent, TaskExecutionDependencies);
}

FPCGTaskId UPCGSubsystem::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGenericWithContext(InOperation, SourceComponent, TaskExecutionDependencies, TaskDataDependencies);
}

FPCGTaskId UPCGSubsystem::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, TFunction<void(FPCGContext*)> InAbortOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGenericWithContext(InOperation, InAbortOperation, SourceComponent, TaskExecutionDependencies, TaskDataDependencies);
}

void UPCGSubsystem::CancelGeneration(UPCGComponent* Component)
{
	check(GraphExecutor && IsInGameThread());
	if (!Component || !Component->IsGenerating())
	{
		return;
	}

	if (Component->IsPartitioned())
	{
		auto LocalCancel = [this](UPCGComponent* LocalComponent)
		{
			if (LocalComponent->IsGenerating())
			{
				CancelGeneration(LocalComponent);
			}

			return InvalidPCGTaskId;
		};

		ActorAndComponentMapping.DispatchToRegisteredLocalComponents(Component, LocalCancel);
	}

	TArray<UPCGComponent*> CancelledComponents = GraphExecutor->Cancel(Component);
	for (UPCGComponent* CancelledComponent : CancelledComponents)
	{
		if (CancelledComponent)
		{
			CancelledComponent->OnProcessGraphAborted(/*bQuiet=*/true);
		}
	}	
}

void UPCGSubsystem::CancelGeneration(UPCGGraph* Graph)
{
	check(GraphExecutor);

	if (!Graph)
	{
		return;
	}

	TArray<UPCGComponent*> CancelledComponents = GraphExecutor->Cancel(Graph);
	for (UPCGComponent* CancelledComponent : CancelledComponents)
	{
		if (ensure(CancelledComponent))
		{
			CancelledComponent->OnProcessGraphAborted(/*bQuiet=*/true);
		}
	}
}

void UPCGSubsystem::CancelAllGeneration()
{
	check(GraphExecutor);

	TArray<UPCGComponent*> CancelledComponents = GraphExecutor->CancelAll();
	for (UPCGComponent* CancelledComponent : CancelledComponents)
	{
		if (ensure(CancelledComponent))
		{
			CancelledComponent->OnProcessGraphAborted(/*bQuiet=*/true);
		}
	}
}

void UPCGSubsystem::RefreshRuntimeGenComponent(UPCGComponent* RuntimeComponent, EPCGChangeType ChangeType)
{
	if (!ensure(RuntimeComponent && RuntimeComponent->IsManagedByRuntimeGenSystem()))
	{
		return;
	}

	if (ensure(RuntimeGenScheduler))
	{
		// Only need to remove PAs if the grid sizes have changed.
		const bool bRemovePartitionActors = !!(ChangeType & EPCGChangeType::GenerationGrid);
		RuntimeGenScheduler->RefreshComponent(RuntimeComponent, bRemovePartitionActors);
	}
}

bool UPCGSubsystem::IsGraphCurrentlyExecuting(UPCGGraph* Graph)
{
	check(GraphExecutor);

	if (!Graph)
	{
		return false;
	}

	return GraphExecutor->IsGraphCurrentlyExecuting(Graph);
}

bool UPCGSubsystem::IsAnyGraphCurrentlyExecuting() const
{
	return GraphExecutor && GraphExecutor->IsAnyGraphCurrentlyExecuting();
}

void UPCGSubsystem::ForAllRegisteredLocalComponents(UPCGComponent* InOriginalComponent, const TFunctionRef<void(UPCGComponent*)>& InFunc) const
{
	auto WrapperFunc = [&InFunc](UPCGComponent* Component) -> FPCGTaskId
	{
		InFunc(Component);
		return InvalidPCGTaskId;
	};

	ActorAndComponentMapping.DispatchToRegisteredLocalComponents(InOriginalComponent, WrapperFunc);
}

void UPCGSubsystem::ForAllRegisteredIntersectingLocalComponents(UPCGComponent* InOriginalComponent, const FBoxCenterAndExtent& InBounds, const TFunctionRef<void(UPCGComponent*)>& InFunc) const
{
	check(InOriginalComponent);
	const FBox Overlap = InOriginalComponent->GetGridBounds().Overlap(InBounds.GetBox());

	// We reject overlaps with zero volume instead of simply checking Intersect(...) to avoid bounds which touch but do not overlap.
	if (Overlap.GetVolume() <= 0)
	{
		return;
	}

	ActorAndComponentMapping.ForAllIntersectingPartitionActors(Overlap, [InOriginalComponent, &InFunc](APCGPartitionActor* Actor)
	{
		if (UPCGComponent* LocalComponent = Actor->GetLocalComponent(InOriginalComponent))
		{
			InFunc(LocalComponent);
		}
	});
}

void UPCGSubsystem::ForAllOverlappingComponentsInHierarchy(UPCGComponent* InComponent, const TFunctionRef<void(UPCGComponent*)>& InFunc) const
{
	UPCGComponent* OriginalComponent = InComponent->GetOriginalComponent();

	ForAllRegisteredLocalComponents(OriginalComponent, [InComponent, &InFunc](UPCGComponent* InLocalComponent)
	{
		const FBox OtherBounds = InLocalComponent->GetGridBounds();
		const FBox ThisBounds = InComponent->GetGridBounds();
		const FBox Overlap = OtherBounds.Overlap(ThisBounds);
		if (Overlap.GetVolume() > 0)
		{
			InFunc(InLocalComponent);
		}
	});
}

FPCGTaskId UPCGSubsystem::ForAllOverlappingCells(const FBox& InBounds, const PCGHiGenGrid::FSizeArray& InGridSizes, bool bCanCreateActor, const TArray<FPCGTaskId>& Dependencies, TFunctionRef<FPCGTaskId(APCGPartitionActor*, const FBox&)> InFunc) const
{
	if (!GraphExecutor || !PCGWorldActor)
	{
		UE_LOG(LogPCG, Error, TEXT("[ForAllOverlappingCells] GraphExecutor or PCGWorldActor is null."));
		return InvalidPCGTaskId;
	}

	PCGHiGenGrid::FSizeArray GridSizes = InGridSizes;

	// We have no use for unbounded grids as this is a grid-centric function. Also discard invalid grid sizes.
	GridSizes.Remove(static_cast<uint32>(EPCGHiGenGrid::Unbounded));
	GridSizes.RemoveAll([](uint32 GridSize) { return !ensure(PCGHiGenGrid::IsValidGridSize(GridSize)); });

	if (GridSizes.IsEmpty())
	{
		ensureMsgf(false, TEXT("Must have grid sizes."));
		return InvalidPCGTaskId;
	}

	PCGWorldActor->CreateGridGuidsIfNecessary(GridSizes, /*bAreGridsSerialized=*/true);

	PCGHiGenGrid::FSizeToGuidMap GridSizeToGuid;
	PCGWorldActor->GetSerializedGridGuids(GridSizeToGuid);
	
	TArray<FPCGTaskId> CellTasks;
	for (uint32 GridSize : GridSizes)
	{
		const FGuid* GuidPtr = GridSizeToGuid.Find(GridSize);
		if (!ensure(GuidPtr))
		{
			continue;
		}

		const FGuid GridGuid = *GuidPtr;

		// In case of 2D grid, we are clamping our bounds in Z to be within 0 and GridSize to create a 2D grid instead of 3D.
		FBox ModifiedInBounds = InBounds;
		if (PCGWorldActor->bUse2DGrid)
		{
			FVector MinBounds = InBounds.Min;
			FVector MaxBounds = InBounds.Max;

			MinBounds.Z = 0;
			MaxBounds.Z = GridSize;
			ModifiedInBounds = FBox(MinBounds, MaxBounds);
		}

		// Helper lambda to apply 'InFunc' to the specified grid cell. Only applies if the cell overlaps the modified bounds, and the actor can be found or created and is not unloaded.
		auto ApplyOnCell = [this, &CellTasks, ModifiedInBounds, bCanCreateActor, &GridGuid, GridSize, &InFunc](const FIntVector& CellCoord, const FBox& CellBounds)
		{
			const FBox IntersectedBounds = ModifiedInBounds.Overlap(CellBounds);

			if (IntersectedBounds.IsValid)
			{
				if (APCGPartitionActor* Actor = FindOrCreatePCGPartitionActor(GridGuid, GridSize, CellCoord, /*bRuntimeGenerated=*/false, bCanCreateActor))
				{
					FPCGTaskId ExecuteTaskId = InFunc(Actor, IntersectedBounds);

					if (ExecuteTaskId != InvalidPCGTaskId)
					{
						CellTasks.Add(ExecuteTaskId);
					}
				}
			}
		};

		FIntVector MinCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Min, GridSize, PCGWorldActor->bUse2DGrid);
		FIntVector MaxCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Max, GridSize, PCGWorldActor->bUse2DGrid);

		// Apply 'InFunc' to all cells in the provided bounds.
		for (int32 Z = MinCellCoords.Z; Z <= MaxCellCoords.Z; ++Z)
		{
			for (int32 Y = MinCellCoords.Y; Y <= MaxCellCoords.Y; ++Y)
			{
				for (int32 X = MinCellCoords.X; X <= MaxCellCoords.X; ++X)
				{
					FIntVector CellCoords(X, Y, Z);

					const FVector Min = FVector(CellCoords) * GridSize;
					const FVector Max = Min + FVector(GridSize);
					FBox CellBounds(Min, Max);

					ApplyOnCell(MoveTemp(CellCoords), MoveTemp(CellBounds));
				}
			}
		}
	}

	// Create a dummy task to wait on dependencies, which creates a dummy task to wait on all cells.
	if (!CellTasks.IsEmpty())
	{
		const FPCGTaskId DependenciesTaskId = GraphExecutor->ScheduleGeneric([]() { return true; }, /*InSourceComponent=*/nullptr, Dependencies);
		return GraphExecutor->ScheduleGeneric([]() { return true; }, /*InSourceComponent=*/nullptr, { DependenciesTaskId });
	}
	else
	{
		return InvalidPCGTaskId;
	}
}

void UPCGSubsystem::CleanupLocalComponentsImmediate(UPCGComponent* InOriginalComponent, bool bRemoveComponents)
{
	if (!InOriginalComponent)
	{
		return;
	}

	auto LocalCleanupTask = [bRemoveComponents](UPCGComponent* LocalComponent)
	{
		if (ensure(LocalComponent) && !LocalComponent->IsCleaningUp())
		{
			LocalComponent->CleanupLocalImmediate(bRemoveComponents);
		}

		return InvalidPCGTaskId;
	};

	ActorAndComponentMapping.DispatchToRegisteredLocalComponents(InOriginalComponent, LocalCleanupTask);

	// Remove the local component mappings if requested and the component is partitioned. If 'bRemoveComponents' is false, that indicates we are doing a refresh, so destroying
	// the component mappings is counterproductive.
	if (bRemoveComponents && InOriginalComponent->IsPartitioned())
	{
		ActorAndComponentMapping.DeleteMappingPCGComponentPartitionActor(InOriginalComponent);
	}
}

UPCGComponent* UPCGSubsystem::GetLocalComponent(uint32 GridSize, const FIntVector& CellCoords, const UPCGComponent* InOriginalComponent, bool bTransient) const
{
	return ActorAndComponentMapping.GetLocalComponent(GridSize, CellCoords, InOriginalComponent, bTransient);
}

APCGPartitionActor* UPCGSubsystem::GetRegisteredPCGPartitionActor(uint32 GridSize, const FIntVector& GridCoords, bool bRuntimeGenerated) const
{
	return ActorAndComponentMapping.GetPartitionActor(GridSize, GridCoords, bRuntimeGenerated);
}

APCGPartitionActor* UPCGSubsystem::FindOrCreatePCGPartitionActor(const FGuid& Guid, uint32 GridSize, const FIntVector& GridCoords, bool bRuntimeGenerated, bool bCanCreateActor) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorHelpers::FindOrCreatePCGPartitionActor);

	UWorld* World = GetWorld();

	if (!World)
	{
		UE_LOG(LogPCG, Error, TEXT("[FindOrCreatePCGPartitionActor] World is null."));
		return nullptr;
	}

	if (!PCGWorldActor)
	{
		UE_LOG(LogPCG, Error, TEXT("[FindOrCreatePCGPartitionActor] PCGWorldActor is null."));
		return nullptr;
	}

	// Attempt to find an existing PA.
	if (APCGPartitionActor* ExistingActor = GetRegisteredPCGPartitionActor(GridSize, GridCoords, bRuntimeGenerated))
	{
		return ExistingActor;
	}
	else if (!bRuntimeGenerated)
	{
		// Check if there is already an unloaded actor for this cell. RuntimeGenerated PAs are never unloaded, so we ignore them.
		if (PCGWorldActor->DoesSerializedPartitionActorExist(Guid, GridSize, GridCoords))
		{
			return nullptr;
		}
	}

	if (!bCanCreateActor)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
#if WITH_EDITOR
	SpawnParams.Name = *APCGPartitionActor::GetPCGPartitionActorName(GridSize, GridCoords, bRuntimeGenerated);
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
#endif
	if (bRuntimeGenerated)
	{
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.ObjectFlags &= ~RF_Transactional;
	}

	const FVector CellCenter(FVector(GridCoords.X + 0.5, GridCoords.Y + 0.5, GridCoords.Z + 0.5) * GridSize);
	APCGPartitionActor* NewActor = CastChecked<APCGPartitionActor>(World->SpawnActor(APCGPartitionActor::StaticClass(), &CellCenter, nullptr, SpawnParams));

	if (bRuntimeGenerated)
	{
		NewActor->SetToRuntimeGenerated();
	}

#if WITH_EDITOR
	NewActor->SetLockLocation(true);
	NewActor->SetActorLabel(SpawnParams.Name.ToString());
	NewActor->SetGridSize(GridSize);
#endif

	// Empty GUID if runtime generated, since transient PAs don't need one.
	NewActor->PostCreation(bRuntimeGenerated ? FGuid() : Guid, GridSize);

	return NewActor;
}

bool UPCGSubsystem::IsGraphCacheDebuggingEnabled() const
{
	return GraphExecutor && GraphExecutor->IsGraphCacheDebuggingEnabled();
}

FPCGGenSourceManager* UPCGSubsystem::GetGenSourceManager() const
{
	return RuntimeGenScheduler ? RuntimeGenScheduler->GenSourceManager : nullptr;
}

bool UPCGSubsystem::GetOutputData(FPCGTaskId TaskId, FPCGDataCollection& OutData)
{
	check(GraphExecutor);
	return GraphExecutor->GetOutputData(TaskId, OutData);
}

#if WITH_EDITOR

FPCGTaskId UPCGSubsystem::ScheduleRefresh(UPCGComponent* Component, bool bForceRegen)
{
	check(Component && !Component->IsManagedByRuntimeGenSystem());

	TWeakObjectPtr<UPCGComponent> ComponentPtr(Component);

	auto RefreshTask = [ComponentPtr, bForceRegen]() {
		if (UPCGComponent* Component = ComponentPtr.Get())
		{
			Component->OnRefresh(bForceRegen);
		}
		return true;
	};

	return GraphExecutor->ScheduleGeneric(RefreshTask, Component, {});
}

void UPCGSubsystem::DirtyGraph(UPCGComponent* Component, const FBox& InBounds, EPCGComponentDirtyFlag DirtyFlag)
{
	check(Component);

	// Immediate operation
	auto DirtyTask = [DirtyFlag](UPCGComponent* LocalComponent)
	{
		LocalComponent->DirtyGenerated(DirtyFlag);
		return InvalidPCGTaskId;
	};

	ActorAndComponentMapping.DispatchToRegisteredLocalComponents(Component, DirtyTask);
}

void UPCGSubsystem::ClearPCGLink(UPCGComponent* InComponent, const FBox& InBounds, AActor* InNewActor)
{
	TWeakObjectPtr<AActor> NewActorPtr(InNewActor);
	TWeakObjectPtr<UPCGComponent> ComponentPtr(InComponent);

	auto ScheduleTask = [this, NewActorPtr, ComponentPtr](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds)
	{
		auto MoveTask = [this, NewActorPtr, ComponentPtr, PCGActor]()
		{
			check(NewActorPtr.IsValid() && ComponentPtr.IsValid() && PCGActor != nullptr);

			if (TObjectPtr<UPCGComponent> LocalComponent = PCGActor->GetLocalComponent(ComponentPtr.Get()))
			{
				LocalComponent->MoveResourcesToNewActor(NewActorPtr.Get(), /*bCreateChild=*/true);
			}

			return true;
		};

		return GraphExecutor->ScheduleGeneric(MoveTask, ComponentPtr.Get(), /*TaskExecutionDependencies=*/{});
	};

	bool bHasUnbounded = false;
	PCGHiGenGrid::FSizeArray GridSizes;
	ensure(PCGHelpers::GetGenerationGridSizes(InComponent->GetGraph(), GetPCGWorldActor(), GridSizes, bHasUnbounded));

	FPCGTaskId TaskId = InvalidPCGTaskId;
	if (!GridSizes.IsEmpty())
	{
		TaskId = ForAllOverlappingCells(InBounds, GridSizes, /*bCanCreateActor=*/false, /*Dependencies=*/{}, ScheduleTask);
	}

	// Verify if the NewActor has some components attached to its root or attached actors. If not, destroy it.
	// Return false if the new actor is not valid or destroyed.
	auto VerifyAndDestroyNewActor = [this, NewActorPtr]()
	{
		check(NewActorPtr.IsValid());

		USceneComponent* RootComponent = NewActorPtr->GetRootComponent();
		check(RootComponent);

		AActor* NewActor = NewActorPtr.Get();

		TArray<AActor*> AttachedActors;
		NewActor->GetAttachedActors(AttachedActors);

		if (RootComponent->GetNumChildrenComponents() == 0 && AttachedActors.IsEmpty())
		{
			GetWorld()->DestroyActor(NewActor);
			return false;
		}

		return true;
	};

	if (TaskId != InvalidPCGTaskId)
	{
		auto CleanupTask = [this, ComponentPtr, VerifyAndDestroyNewActor]()
		{

			// If the new actor is valid, clean up the original component.
			if (VerifyAndDestroyNewActor())
			{
				check(ComponentPtr.IsValid());
				ComponentPtr->Cleanup(/*bRemoveComponents=*/true);
			}

			return true;
		};

		GraphExecutor->ScheduleGeneric(CleanupTask, InComponent, { TaskId });
	}
	else
	{
		VerifyAndDestroyNewActor();
	}
}

void UPCGSubsystem::DeleteSerializedPartitionActors(bool bOnlyDeleteUnused, bool bOnlyChildren)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DeleteSerializedPartitionActors);

	TSet<UPackage*> PackagesToCleanup;
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	auto GatherAndDestroyActors = [this, &PackagesToCleanup, World, bOnlyDeleteUnused, bOnlyChildren](AActor* Actor) -> bool
	{
		TObjectPtr<APCGPartitionActor> PartitionActor = CastChecked<APCGPartitionActor>(Actor);

		// Do not delete RuntimeGen PAs or PAs with graph instances if we are only deleting unused PAs.
		if (!PartitionActor->IsRuntimeGenerated() && (!bOnlyDeleteUnused || !PartitionActor->HasGraphInstances()))
		{
			// Gather all child actors
			TArray<AActor*> AttachedActors;
			PartitionActor->GetAttachedActors(AttachedActors, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/ true);

			if (!bOnlyChildren)
			{
				AttachedActors.Add(PartitionActor);

				PCGWorldActor->RemoveSerializedPartitionActorRecord({ PartitionActor->PCGGuid, PartitionActor->GetPCGGridSize(), PartitionActor->GetGridCoord() });
			}

			for (AActor* ActorToDelete : AttachedActors)
			{
				if (UPackage* ExternalPackage = ActorToDelete->GetExternalPackage())
				{
					PackagesToCleanup.Add(ExternalPackage);
				}

				World->DestroyActor(ActorToDelete);
			}
		}

		return true;
	};

	// First, clear selection otherwise it might crash
	if (GEditor)
	{
		GEditor->SelectNone(true, true, false);
	}

	// Attempt destroy on all PAs in the level.
	if (ULevel* Level = World->GetCurrentLevel())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DeleteSerializedPartitionActors::ForEachActorInLevel);
		UPCGActorHelpers::ForEachActorInLevel<APCGPartitionActor>(Level, GatherAndDestroyActors);
	}

	if (PackagesToCleanup.Num() > 0)
	{
		ObjectTools::CleanupAfterSuccessfulDelete(PackagesToCleanup.Array(), /*bPerformanceReferenceCheck=*/true);
	}
}

void UPCGSubsystem::NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType)
{
	if (!!(ChangeType & (EPCGChangeType::Structural | EPCGChangeType::GenerationGrid)))
	{
		// If change was deep enough, clear out all executed stacks, and let them refresh upon next generate. Fixes
		// cases where stale stacks never got flushed.
		ClearExecutedStacks(InGraph);
	}

	if (GraphExecutor)
	{
		GraphExecutor->NotifyGraphChanged(InGraph, ChangeType);
	}
}

void UPCGSubsystem::PropagateEditingModeToLocalComponents(UPCGComponent* InComponent, EPCGEditorDirtyMode EditingMode)
{
	if (ensure(InComponent && InComponent->IsPartitioned()))
	{
		FBox Bounds = ActorAndComponentMapping.PartitionedOctree.GetBounds(InComponent);
		if (!Bounds.IsValid)
		{
			return;
		}

		ActorAndComponentMapping.ForAllIntersectingPartitionActors(Bounds, [InComponent, EditingMode](APCGPartitionActor* Actor)
		{
			Actor->ChangeTransientState(InComponent, EditingMode);
		});
	}
}

void UPCGSubsystem::CleanFromCache(const IPCGElement* InElement, const UPCGSettings* InSettings/*= nullptr*/)
{
	if (GraphExecutor)
	{
		GraphExecutor->GetCache().CleanFromCache(InElement, InSettings);
	}
}

void UPCGSubsystem::BuildLandscapeCache(bool bQuiet)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::BuildLandscapeCache);
	if (UPCGLandscapeCache* LandscapeCache = GetLandscapeCache())
	{
		LandscapeCache->PrimeCache();
	}
	else if(!bQuiet)
	{
		UE_LOG(LogPCG, Error, TEXT("Unable to build landscape cache because either the world is null or there is no PCG world actor"));
	}
}

void UPCGSubsystem::ClearLandscapeCache()
{
	if (UPCGLandscapeCache* LandscapeCache = GetLandscapeCache())
	{
		LandscapeCache->ClearCache();
	}
}

FPCGGraphCompiler* UPCGSubsystem::GetGraphCompiler()
{
	if (GraphExecutor)
	{
		return &(GraphExecutor->GetCompiler());
	}

	return nullptr;
}

bool UPCGSubsystem::GetStackContext(const UPCGComponent* InComponent, FPCGStackContext& OutStackContext)
{
	UPCGGraph* Graph = InComponent ? InComponent->GetGraph() : nullptr;
	if (!InComponent || !Graph)
	{
		return false;
	}

	// A non-partitioned component generally executes (original component or local component).
	bool bDoesComponentExecute = !InComponent->IsPartitioned();

	// A partitioned higen original component will execute if the graph has UB grid level.
	if (!bDoesComponentExecute && Graph->IsHierarchicalGenerationEnabled())
	{
		PCGHiGenGrid::FSizeArray GridSizes;
		Graph->GetGridSizes(GridSizes, /*bOutHasUnbounded=*/bDoesComponentExecute);
	}

	if (bDoesComponentExecute)
	{
		GetGraphCompiler()->GetCompiledTasks(Graph, InComponent->GetGenerationGridSize(), OutStackContext);
		return true;
	}
	else
	{
		OutStackContext = FPCGStackContext();
		return false;
	}
}

uint32 UPCGSubsystem::GetGraphCacheEntryCount(IPCGElement* InElement) const
{
	return GraphExecutor ? GraphExecutor->GetGraphCacheEntryCount(InElement) : 0;
}

void UPCGSubsystem::NotifyLandscapeEditModeExited()
{
	ActorAndComponentMapping.NotifyLandscapeEditModeExited();
}

void UPCGSubsystem::ClearExecutionMetadata(const FPCGStack& BaseStack)
{
	ClearExecutedStacks(BaseStack);

	if (UPCGComponent* Component = const_cast<UPCGComponent*>(BaseStack.GetRootComponent()))
	{
		Component->ClearInspectionData();

		NodeVisualLogs.ClearLogs(Component);
	}
}

void UPCGSubsystem::ClearExecutionMetadata(UPCGComponent* InComponent)
{
	FPCGStack Stack;
	Stack.PushFrame(InComponent);
	ClearExecutionMetadata(Stack);
}

TArray<FPCGStack> UPCGSubsystem::GetExecutedStacks(const UPCGComponent* InComponent, const UPCGGraph* InSubgraph)
{
	FReadScopeLock Lock(ExecutedStacksLock);

	TArray<FPCGStack> MatchingStacks;

	for (const FPCGStack& Stack : ExecutedStacks)
	{
		if (Stack.GetRootComponent() == InComponent && Stack.GetGraphForCurrentFrame() == InSubgraph)
		{
			MatchingStacks.Add(Stack);
		}
	}

	return MatchingStacks;
}

void UPCGSubsystem::ClearExecutedStacks(FPCGStack BeginningWithStack)
{
	FWriteScopeLock Lock(ExecutedStacksLock);

	for (int StackIndex = ExecutedStacks.Num() - 1; StackIndex >= 0; --StackIndex)
	{
		// Clear any dynamic stack that starts with the given base stack, we don't know what stacks will
		// execute so clean out everything.
		if (ExecutedStacks[StackIndex].BeginsWith(BeginningWithStack))
		{
			ExecutedStacks.RemoveAtSwap(StackIndex);
		}
	}
}

void UPCGSubsystem::ClearExecutedStacks(const UPCGGraph* InContainingGraph)
{
	FWriteScopeLock Lock(ExecutedStacksLock);

	for (int StackIndex = ExecutedStacks.Num() - 1; StackIndex >= 0; --StackIndex)
	{
		// Clear any dynamic stack that starts with the given base stack, we don't know what stacks will
		// execute so clean out everything.
		if (ExecutedStacks[StackIndex].HasObject(InContainingGraph))
		{
			ExecutedStacks.RemoveAtSwap(StackIndex);
		}
	}
}

#endif // WITH_EDITOR

void UPCGSubsystem::FlushCache()
{
	if (GraphExecutor)
	{
		GraphExecutor->GetCache().ClearCache();
		GraphExecutor->GetCompiler().ClearCache();
	}

#if WITH_EDITOR
	// Garbage collection is very seldom run in the editor, but we currently can consume a lot of memory in the cache.
	if (!PCGHelpers::IsRuntimeOrPIE())
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	}
#endif
}
