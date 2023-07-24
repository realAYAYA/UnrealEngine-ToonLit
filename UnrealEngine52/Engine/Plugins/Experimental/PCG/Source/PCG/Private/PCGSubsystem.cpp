// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubsystem.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGWorldActor.h"
#include "Graph/PCGGraphCache.h"
#include "Graph/PCGGraphExecutor.h"
#include "Grid/PCGLandscapeCache.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Engine/Level.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSubsystem)

#if WITH_EDITOR
#include "Editor.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "PackageSourceControlHelper.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "ObjectTools.h"
#else
#include "Engine/Engine.h"
#include "Engine/World.h"
#endif

namespace PCGSubsystemConsole
{
	static FAutoConsoleCommand CommandFlushCache(
		TEXT("pcg.FlushCache"),
		TEXT("Clears the PCG results cache."),
		FConsoleCommandDelegate::CreateLambda([]()
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

				if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(World))
				{
					PCGSubsystem->FlushCache();
				}
			}));

#if WITH_EDITOR
	static FAutoConsoleCommand CommandBuildLandscapeCache(
		TEXT("pcg.BuildLandscapeCache"),
		TEXT("Builds the landscape cache in the current world."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				UWorld* World = (GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : (GEngine ? GEngine->GetCurrentPlayWorld() : nullptr));
				if (UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(World))
				{
					PCGSubsystem->BuildLandscapeCache();
				}
			}));

	static TAutoConsoleVariable<bool> CVarRebuildLandscapeOnPIE(
		TEXT("pcg.PIE.RebuildLandscapeOnPIE"),
		true,
		TEXT("Controls whether the landscape cache will be rebuilt on PIE"));
#endif
}

#if WITH_EDITOR
namespace PCGSubsystem
{
	FPCGTaskId ForEachIntersectingCell(FPCGGraphExecutor* GraphExecutor, UWorld* World, const FBox& InBounds, bool bCreateActor, bool bLoadCell, bool bSaveActors, TFunctionRef<FPCGTaskId(APCGPartitionActor*, const FBox&, const TArray<FPCGTaskId>&)> InOperation);
}
#endif

UPCGSubsystem::UPCGSubsystem()
	: Super()
	, ActorAndComponentMapping(this)
{
#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// TODO: Uncomment when tracking is functionnal
		// ActorAndComponentMapping.RegisterTrackingCallbacks();
	}
#endif // WITH_EDITOR
}

void UPCGSubsystem::Deinitialize()
{
	// Cancel all tasks
	// TODO
	delete GraphExecutor;
	GraphExecutor = nullptr;

	PCGWorldActor = nullptr;
	bHasTickedOnce = false;

#if WITH_EDITOR
	// TODO: Uncomment when tracking is functionnal
	// ActorAndComponentMapping.TeardownTrackingCallbacks();
#endif // WITH_EDITOR

	Super::Deinitialize();
}

void UPCGSubsystem::PostInitialize()
{
	Super::PostInitialize();

	// Initialize graph executor
	check(!GraphExecutor);
	GraphExecutor = new FPCGGraphExecutor(this);
	
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
#endif

void UPCGSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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

	// If we have any tasks to execute, schedule some
	GraphExecutor->Execute();

	// Lose references to landscape cache as needed
	if(PCGWorldActor && GetLandscapeCache())
	{
		GetLandscapeCache()->Tick(DeltaSeconds);
	}

	ActorAndComponentMapping.Tick();
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

#if WITH_EDITOR
void UPCGSubsystem::DestroyPCGWorldActor()
{
	if (PCGWorldActor)
	{
		PCGWorldActorLock.Lock();
		PCGWorldActor->Destroy();
		PCGWorldActor = nullptr;
		PCGWorldActorLock.Unlock();
	}
}
#endif

void UPCGSubsystem::RegisterPCGWorldActor(APCGWorldActor* InActor)
{
	// TODO: we should support merging or multi world actor support when relevant
	if (!PCGWorldActor)
	{
		PCGWorldActor = InActor;
	}
}

void UPCGSubsystem::UnregisterPCGWorldActor(APCGWorldActor* InActor)
{
	if (PCGWorldActor == InActor)
	{
		PCGWorldActor = nullptr;
	}
}

UPCGLandscapeCache* UPCGSubsystem::GetLandscapeCache()
{
	APCGWorldActor* LandscapeCacheOwner = GetPCGWorldActor();
	return LandscapeCacheOwner ? LandscapeCacheOwner->LandscapeCacheObject.Get() : nullptr;
}

FPCGTaskId UPCGSubsystem::ScheduleComponent(UPCGComponent* PCGComponent, bool bSave, const TArray<FPCGTaskId>& Dependencies)
{
	check(GraphExecutor);

	if (!PCGComponent)
	{
		return InvalidPCGTaskId;
	}

#if WITH_EDITOR
	if (PCGComponent->IsPartitioned() && !PCGHelpers::IsRuntimeOrPIE())
	{
		// In this case create the PA and update the mapping
		// Note: This is an immediate operation, as we need the PA for the generation.
		auto ScheduleTask = [](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) { return InvalidPCGTaskId; };

		PCGSubsystem::ForEachIntersectingCell(GraphExecutor, PCGComponent->GetWorld(), PCGComponent->GetGridBounds(), /*bCreateActor=*/true, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);

		ActorAndComponentMapping.UpdateMappingPCGComponentPartitionActor(PCGComponent);
	}
#endif // WITH_EDITOR

	TArray<FPCGTaskId> AllTasks;

	// If the component is partitioned, we will forward the calls to its registered PCG Partition actors
	if (PCGComponent->IsPartitioned())
	{
		auto LocalGenerateTask = [OriginalComponent = PCGComponent, &Dependencies](UPCGComponent* LocalComponent)
		{
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

			return LocalComponent->GenerateInternal(/*bForce=*/ false, EPCGComponentGenerationTrigger::GenerateOnDemand, Dependencies);
		};

		AllTasks = ActorAndComponentMapping.DispatchToRegisteredLocalComponents(PCGComponent, LocalGenerateTask);
	}
	else
	{
		FPCGTaskId TaskId = PCGComponent->CreateGenerateTask(/*bForce=*/bSave, Dependencies);
		if (TaskId != InvalidPCGTaskId)
		{
			AllTasks.Add(TaskId);
		}
	}

	if (!AllTasks.IsEmpty())
	{
		TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);

		return GraphExecutor->ScheduleGenericWithContext([ComponentPtr](FPCGContext* Context) {
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
			}, PCGComponent, AllTasks);
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

FPCGTaskId UPCGSubsystem::ScheduleCleanup(UPCGComponent* PCGComponent, bool bRemoveComponents, bool bSave, const TArray<FPCGTaskId>& Dependencies)
{
	if (!PCGComponent)
	{
		return InvalidPCGTaskId;
	}

	TArray<FPCGTaskId> AllTasks;

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

			return LocalComponent->CleanupInternal(bRemoveComponents, /*bSave=*/ false, Dependencies);
		};

		AllTasks = ActorAndComponentMapping.DispatchToRegisteredLocalComponents(PCGComponent, LocalCleanupTask);
	}
	else
	{
		FPCGTaskId TaskId = PCGComponent->CreateCleanupTask(bRemoveComponents, Dependencies);
		if (TaskId != InvalidPCGTaskId)
		{
			AllTasks.Add(TaskId);
		}
	}

	TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);
	auto PostCleanupTask = [this, ComponentPtr]()
	{
		if (UPCGComponent* Component = ComponentPtr.Get())
		{
			// If the component is not valid anymore, just early out
			if (!IsValid(Component))
			{
				return true;
			}

			Component->PostCleanupGraph();

#if WITH_EDITOR
			// If we are in Editor and partitioned, delete the mapping.
			if (Component->IsPartitioned() && !PCGHelpers::IsRuntimeOrPIE())
			{
				ActorAndComponentMapping.DeleteMappingPCGComponentPartitionActor(Component);
			}
#endif // WITH_EDITOR
		}

		return true;
	};

	FPCGTaskId PostCleanupTaskId = InvalidPCGTaskId;

	// If we have no tasks to do, just call PostCleanup immediatly
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

FPCGTaskId UPCGSubsystem::ScheduleGraph(UPCGGraph* Graph, UPCGComponent* SourceComponent, FPCGElementPtr InputElement, const TArray<FPCGTaskId>& Dependencies)
{
	if (SourceComponent)
	{
		return GraphExecutor->Schedule(Graph, SourceComponent, InputElement, Dependencies);
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

FPCGTaskId UPCGSubsystem::ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* Component, const TArray<FPCGTaskId>& TaskDependencies)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGeneric(InOperation, Component, TaskDependencies);
}

FPCGTaskId UPCGSubsystem::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* Component, const TArray<FPCGTaskId>& TaskDependencies, bool bConsumeInputData)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGenericWithContext(InOperation, Component, TaskDependencies, bConsumeInputData);
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
		CancelledComponent->OnProcessGraphAborted(/*bQuiet=*/true);
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
		CancelledComponent->OnProcessGraphAborted(/*bQuiet=*/true);
	}
}

void UPCGSubsystem::CancelAllGeneration()
{
	check(GraphExecutor);

	TArray<UPCGComponent*> CancelledComponents = GraphExecutor->CancelAll();
	for (UPCGComponent* Component : CancelledComponents)
	{
		Component->OnProcessGraphAborted(/*bQuiet=*/true);
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

void UPCGSubsystem::ForAllRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunction<void(UPCGComponent*)>& InFunc) const
{
	auto WrapperFunc = [&InFunc](UPCGComponent* Component) -> FPCGTaskId
	{
		InFunc(Component);
		return InvalidPCGTaskId;
	};

	ActorAndComponentMapping.DispatchToRegisteredLocalComponents(OriginalComponent, WrapperFunc);
}

bool UPCGSubsystem::IsGraphCacheDebuggingEnabled() const
{
	return GraphExecutor && GraphExecutor->IsGraphCacheDebuggingEnabled();
}

bool UPCGSubsystem::GetOutputData(FPCGTaskId TaskId, FPCGDataCollection& OutData)
{
	check(GraphExecutor);
	return GraphExecutor->GetOutputData(TaskId, OutData);
}

#if WITH_EDITOR

namespace PCGSubsystem
{
	FPCGTaskId ForEachIntersectingCell(FPCGGraphExecutor* GraphExecutor, UWorld* World, const FBox& InBounds, bool bCreateActor, bool bLoadCell, bool bSaveActors, TFunctionRef<FPCGTaskId(APCGPartitionActor*, const FBox&, const TArray<FPCGTaskId>&)> InOperation)
	{
		if (!GraphExecutor || !World)
		{
			UE_LOG(LogPCG, Error, TEXT("[ForEachIntersectingCell] GraphExecutor or World is null"));
			return InvalidPCGTaskId;
		}

		APCGWorldActor* PCGWorldActor = PCGHelpers::GetPCGWorldActor(World);

		check(PCGWorldActor);

		// In case of 2D grid, we are clamping our bounds in Z to be within 0 and PartitionGridSize
		// By doing so, WP will tie all the partition actors to [0, PartitionGridSize[ interval, generating a "2D grid" instead of 3D.
		FBox ModifiedInBounds = InBounds;
		if (PCGWorldActor->bUse2DGrid)
		{
			FVector MinBounds = InBounds.Min;
			FVector MaxBounds = InBounds.Max;

			MinBounds.Z = 0;
			MaxBounds.Z = FMath::Max<FVector::FReal>(0.0, PCGWorldActor->PartitionGridSize - 1.0); // -1 to be just below the PartitionGridSize
			ModifiedInBounds = FBox(MinBounds, MaxBounds);
		}

		TArray<FPCGTaskId> CellTasks;

		auto CellLambda = [&CellTasks, GraphExecutor, World, bCreateActor, bLoadCell, bSaveActors, ModifiedInBounds, &InOperation](const UActorPartitionSubsystem::FCellCoord& CellCoord, const FBox& CellBounds)
		{
			UActorPartitionSubsystem* PartitionSubsystem = UWorld::GetSubsystem<UActorPartitionSubsystem>(World);
			FBox IntersectedBounds = ModifiedInBounds.Overlap(CellBounds);

			if (IntersectedBounds.IsValid)
			{
				TSharedPtr<TSet<FWorldPartitionReference>> ActorReferences = MakeShared<TSet<FWorldPartitionReference>>();

				auto PostCreation = [](APartitionActor* Actor) { CastChecked<APCGPartitionActor>(Actor)->PostCreation(); };

				const bool bInBoundsSearch = true;
				const FGuid DefaultGridGuid;
				const uint32 DefaultGridSize = 0;

				APCGPartitionActor* PCGActor = Cast<APCGPartitionActor>(PartitionSubsystem->GetActor(APCGPartitionActor::StaticClass(), CellCoord, bCreateActor, DefaultGridGuid, DefaultGridSize, bInBoundsSearch, PostCreation));

				// At this point, if bCreateActor was true, then it exists, but it is not currently loaded; make sure it is loaded
				// Otherwise, we still need to load it if it exists
				// TODO: Revisit after API review on the WP side, we shouldn't have to load here or get the actor desc directly
				if (!PCGActor && bSaveActors)
				{
					const FWorldPartitionActorDesc* PCGActorDesc = nullptr;
					auto FindFirst = [&CellCoord, &PCGActorDesc](const FWorldPartitionActorDesc* ActorDesc) {
						FPartitionActorDesc* PartitionActorDesc = (FPartitionActorDesc*)ActorDesc;

						if (PartitionActorDesc &&
							PartitionActorDesc->GridIndexX == CellCoord.X &&
							PartitionActorDesc->GridIndexY == CellCoord.Y &&
							PartitionActorDesc->GridIndexZ == CellCoord.Z)
						{
							PCGActorDesc = ActorDesc;
							return false;
						}
						else
						{
							return true;
						}
					};

					FWorldPartitionHelpers::ForEachIntersectingActorDesc<APCGPartitionActor>(World->GetWorldPartition(), CellBounds, FindFirst);

					check(!bCreateActor || PCGActorDesc);
					if (PCGActorDesc)
					{
						ActorReferences->Add(FWorldPartitionReference(World->GetWorldPartition(), PCGActorDesc->GetGuid()));
						PCGActor = Cast<APCGPartitionActor>(PCGActorDesc->GetActor());
					}
				}
				else if(PCGActor)
				{
					// We still need to keep a reference on the PCG actor - note that newly created PCG actors will not have a reference here, but won't be unloaded
					ActorReferences->Add(FWorldPartitionReference(World->GetWorldPartition(), PCGActor->GetActorGuid()));
				}

				if (!PCGActor)
				{
					return true;
				}

				TArray<FPCGTaskId> PreviousTasks;

				auto SetPreviousTaskIfValid = [&PreviousTasks](FPCGTaskId TaskId){
					if (TaskId != InvalidPCGTaskId)
					{
						PreviousTasks.Reset();
						PreviousTasks.Add(TaskId);
					}
				};

				// We'll need to make sure actors in the bounds are loaded only if we need them.
				if (bLoadCell)
				{
					auto WorldPartitionLoadActorsInBounds = [World, ActorReferences](const FWorldPartitionActorDesc* ActorDesc) {
						check(ActorDesc);
						ActorReferences->Add(FWorldPartitionReference(World->GetWorldPartition(), ActorDesc->GetGuid()));
						// Load actor if not already loaded
						ActorDesc->GetActor();
						return true;
					};

					auto LoadActorsTask = [World, IntersectedBounds, WorldPartitionLoadActorsInBounds]() {
						FWorldPartitionHelpers::ForEachIntersectingActorDesc(World->GetWorldPartition(), IntersectedBounds, WorldPartitionLoadActorsInBounds);
						return true;
					};

					FPCGTaskId LoadTaskId = GraphExecutor->ScheduleGeneric(LoadActorsTask, nullptr, {});
					SetPreviousTaskIfValid(LoadTaskId);
				}

				// Execute
				FPCGTaskId ExecuteTaskId = InOperation(PCGActor, IntersectedBounds, PreviousTasks);
				SetPreviousTaskIfValid(ExecuteTaskId);

				// Save changes; note that there's no need to save if the operation was cancelled
				if (bSaveActors && ExecuteTaskId != InvalidPCGTaskId)
				{
					auto SaveActorTask = [PCGActor, GraphExecutor]() {
						GraphExecutor->AddToDirtyActors(PCGActor);
						return true;
					};

					FPCGTaskId SaveTaskId = GraphExecutor->ScheduleGeneric(SaveActorTask, nullptr, PreviousTasks);
					SetPreviousTaskIfValid(SaveTaskId);
				}

				if (bLoadCell)
				{
					// Unload actors from cell (or the pcg actor refered here)
					auto UnloadActorsTask = [GraphExecutor, ActorReferences]() {
						GraphExecutor->AddToUnusedActors(*ActorReferences);
						return true;
					};

					// Schedule after the save (if valid), then the execute so we can queue this after the load.
					FPCGTaskId UnloadTaskId = GraphExecutor->ScheduleGeneric(UnloadActorsTask, nullptr, PreviousTasks);
					SetPreviousTaskIfValid(UnloadTaskId);
				}

				// Finally, mark "last" valid task in the cell tasks.
				CellTasks.Append(PreviousTasks);
			}

			return true;
		};

		// TODO: accumulate last phases of every cell + run a GC at the end?
		//  or add some mechanism on the graph executor side to run GC every now and then
		FActorPartitionGridHelper::ForEachIntersectingCell(APCGPartitionActor::StaticClass(), ModifiedInBounds, World->PersistentLevel, CellLambda);

		// Finally, create a dummy generic task to wait on all cells
		if (!CellTasks.IsEmpty())
		{
			return GraphExecutor->ScheduleGeneric([]() { return true; }, nullptr, CellTasks);
		}
		else
		{
			return InvalidPCGTaskId;
		}
	}

} // end namepsace PCGSubsystem

void UPCGSubsystem::CreatePartitionActorsWithinBounds(const FBox& InBounds)
{
	if (!PCGHelpers::IsRuntimeOrPIE())
	{
		auto ScheduleTask = [](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) { return InvalidPCGTaskId; };

		// We can't spawn actors if we are running constructions scripts, asserting when we try to get the actor with the WP API.
		// We should never enter this if we are in a construction script. If the ensure is hit, we need to fix it.
		UWorld* World = GetWorld();
		if (ensure(World && !World->bIsRunningConstructionScript))
		{
			PCGSubsystem::ForEachIntersectingCell(GraphExecutor, World, InBounds, /*bCreateActor=*/true, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);
		}
	}
}

FPCGTaskId UPCGSubsystem::ProcessGraph(UPCGComponent* Component, const FBox& InPreviousBounds, const FBox& InNewBounds, EOperation InOperation, bool bSave)
{
	check(Component);
	TWeakObjectPtr<UPCGComponent> ComponentPtr(Component);

	// TODO: optimal implementation would find the difference between the previous bounds and the new bounds
	// and process these only. This is esp. important because of the CreateActor parameter.
	auto ScheduleTask = [this, ComponentPtr, InOperation, bSave](APCGPartitionActor* PCGActor, const FBox& InBounds, const TArray<FPCGTaskId>& TaskDependencies){
		TWeakObjectPtr<APCGPartitionActor> PCGActorPtr(PCGActor);

		auto UnpartitionTask = [ComponentPtr, PCGActorPtr]() {
			// TODO: PCG actors that become empty could be deleted, but we also need to keep track
			// of packages that would need to be deleted from SCC.
			if (APCGPartitionActor* PCGActor = PCGActorPtr.Get())
			{
				if (UPCGComponent* Component = ComponentPtr.Get())
				{
					PCGActor->RemoveGraphInstance(Component);
				}
				else
				{
#if WITH_EDITOR
					PCGActor->CleanupDeadGraphInstances();
#endif
				}
			}
			return true;
		};

		auto PartitionTask = [ComponentPtr, PCGActorPtr]() {
			if (APCGPartitionActor* PCGActor = PCGActorPtr.Get())
			{
				if (UPCGComponent* Component = ComponentPtr.Get())
				{
					PCGActor->AddGraphInstance(Component);
				}
			}

			return true;
		};

		auto ScheduleGraph = [this, PCGActorPtr, ComponentPtr, bSave, TaskDependencies]() {
			if (APCGPartitionActor* PCGActor = PCGActorPtr.Get())
			{
				if (UPCGComponent* Component = ComponentPtr.Get())
				{
					// Ensure that the PCG actor has a matching local component.
					// This is done immediately, but technically we could add it as a task
					PCGActor->AddGraphInstance(Component);

					UPCGComponent* LocalComponent = PCGActor->GetLocalComponent(Component);

					if (!LocalComponent)
					{
						return InvalidPCGTaskId;
					}

					return LocalComponent->GenerateInternal(bSave, EPCGComponentGenerationTrigger::GenerateOnDemand, TaskDependencies);
				}
				else
				{
					UE_LOG(LogPCG, Error, TEXT("[ProcessGraph] PCG Component on PCG Actor is null"));
					return InvalidPCGTaskId;
				}
			}
			
			return InvalidPCGTaskId;
		};

		switch (InOperation)
		{
		case EOperation::Unpartition:
			return GraphExecutor->ScheduleGeneric(UnpartitionTask, ComponentPtr.Get(), TaskDependencies);
		case EOperation::Partition:
			return GraphExecutor->ScheduleGeneric(PartitionTask, ComponentPtr.Get(), TaskDependencies);
		case EOperation::Generate:
		default:
			return ScheduleGraph();
		}
	};

	FBox UnionBounds = InPreviousBounds + InNewBounds;
	const bool bGenerate = InOperation == EOperation::Generate;
	const bool bCreateActors = InOperation != EOperation::Unpartition;
	const bool bLoadCell = (bGenerate && bSave); // TODO: review this
	const bool bSaveActors = (bSave);

	FPCGTaskId ProcessAllCellsTaskId = InvalidPCGTaskId;

	if (UnionBounds.IsValid)
	{
		ProcessAllCellsTaskId = PCGSubsystem::ForEachIntersectingCell(GraphExecutor, Component->GetWorld(), UnionBounds, bCreateActors, bLoadCell, bSaveActors, ScheduleTask);
	}

	// Finally, call PostProcessGraph if something happened
	if (ProcessAllCellsTaskId != InvalidPCGTaskId)
	{
		auto PostProcessGraphTask = [ComponentPtr, InNewBounds, bGenerate](FPCGContext* Context) {
			if (UPCGComponent* Component = ComponentPtr.Get())
			{
				Component->PostProcessGraph(InNewBounds, bGenerate, Context);
			}
			return true;
		};

		return GraphExecutor->ScheduleGenericWithContext(PostProcessGraphTask, Component, { ProcessAllCellsTaskId });
	}
	else
	{
		if(Component)
		{
			Component->OnProcessGraphAborted();
		}

		return InvalidPCGTaskId;
	}
}

FPCGTaskId UPCGSubsystem::ScheduleRefresh(UPCGComponent* Component)
{
	TWeakObjectPtr<UPCGComponent> ComponentPtr(Component);

	auto RefreshTask = [ComponentPtr]() {
		if (UPCGComponent* Component = ComponentPtr.Get())
		{
			Component->OnRefresh();
		}
		return true;
	};

	return GraphExecutor->ScheduleGeneric(RefreshTask, Component, {});
}

FPCGTaskId UPCGSubsystem::CleanupGraph(UPCGComponent* Component, const FBox& InBounds, bool bRemoveComponents, bool bSave)
{
	TWeakObjectPtr<UPCGComponent> ComponentPtr(Component);

	auto ScheduleTask = [this, ComponentPtr, bRemoveComponents, bSave](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) {
		UPCGComponent* Component = ComponentPtr.Get();
		check(Component != nullptr && PCGActor != nullptr);

		if (!PCGActor || !Component || !IsValid(Component))
		{
			return InvalidPCGTaskId;
		}

		if (UPCGComponent* LocalComponent = PCGActor->GetLocalComponent(Component))
		{
			// Ensure to avoid infinite loop
			if (ensure(!LocalComponent->IsPartitioned()))
			{
				return LocalComponent->CleanupInternal(bRemoveComponents, bSave, TaskDependencies);
			}
		}

		return InvalidPCGTaskId;
	};

	FPCGTaskId ProcessAllCellsTaskId = PCGSubsystem::ForEachIntersectingCell(GraphExecutor, Component->GetWorld(), InBounds, /*bCreateActor=*/false, /*bLoadCell=*/false, bSave, ScheduleTask);

	// Finally, call PostCleanupGraph if something happened
	if (ProcessAllCellsTaskId != InvalidPCGTaskId)
	{
		auto PostCleanupGraph = [ComponentPtr]() {
			if (UPCGComponent* Component = ComponentPtr.Get())
			{
				Component->PostCleanupGraph();
			}
			return true;
		};

		return GraphExecutor->ScheduleGeneric(PostCleanupGraph, Component, { ProcessAllCellsTaskId });
	}
	else
	{
		if (Component)
		{
			Component->PostCleanupGraph();
		}

		return InvalidPCGTaskId;
	}
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

void UPCGSubsystem::CleanupPartitionActors(const FBox& InBounds)
{
	auto ScheduleTask = [this](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) {
		auto CleanupTask = [PCGActor]() {
			check(PCGActor);
			PCGActor->CleanupDeadGraphInstances();

			return true;
		};

		return GraphExecutor->ScheduleGeneric(CleanupTask, nullptr, TaskDependencies);
	};

	PCGSubsystem::ForEachIntersectingCell(GraphExecutor, GetWorld(), InBounds, /*bCreateActor=*/false, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);
}

void UPCGSubsystem::ClearPCGLink(UPCGComponent* InComponent, const FBox& InBounds, AActor* InNewActor)
{
	TWeakObjectPtr<AActor> NewActorPtr(InNewActor);
	TWeakObjectPtr<UPCGComponent> ComponentPtr(InComponent);

	auto ScheduleTask = [this, NewActorPtr, ComponentPtr](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) {
		auto MoveTask = [this, NewActorPtr, ComponentPtr, PCGActor]() {
			check(NewActorPtr.IsValid() && ComponentPtr.IsValid() && PCGActor != nullptr);

			if (TObjectPtr<UPCGComponent> LocalComponent = PCGActor->GetLocalComponent(ComponentPtr.Get()))
			{
				LocalComponent->MoveResourcesToNewActor(NewActorPtr.Get(), /*bCreateChild=*/true);
			}

			return true;
		};

		return GraphExecutor->ScheduleGeneric(MoveTask, ComponentPtr.Get(), TaskDependencies);
	};

	FPCGTaskId TaskId = PCGSubsystem::ForEachIntersectingCell(GraphExecutor, GetWorld(), InBounds, /*bCreateActor=*/false, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);

	// Verify if the NewActor has some components attached to its root or attached actors. If not, destroy it.
	// Return false if the new actor is not valid or destroyed.
	auto VerifyAndDestroyNewActor = [this, NewActorPtr]() {
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
		auto CleanupTask = [this, ComponentPtr, VerifyAndDestroyNewActor]() {

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

void UPCGSubsystem::DeletePartitionActors(bool bOnlyDeleteUnused)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DeletePartitionActors);

	// TODO: This should be handled differently when we are able to stop a generation
	// For now, we need to be careful and not delete partition actors that are linked to components that are currently
	// generating stuff.
	// Also, we keep a set on all those actors, in case the partition actor status changes during the loop.
	TSet<TObjectPtr<APCGPartitionActor>> ActorsNotSafeToBeDeleted;

	TSet<UPackage*> PackagesToCleanup;
	TSet<FString> PackagesToDeleteFromSCC;
	UWorld* World = GetWorld();

	if (!World || !World->GetWorldPartition())
	{
		return;
	}

	auto GatherAndDestroyLoadedActors = [&PackagesToCleanup, &PackagesToDeleteFromSCC, World, &ActorsNotSafeToBeDeleted](AActor* Actor) -> bool
	{
		// Make sure that this actor was not flagged to not be deleted, or not safe for deletion
		TObjectPtr<APCGPartitionActor> PartitionActor = CastChecked<APCGPartitionActor>(Actor);
		if (!PartitionActor->IsSafeForDeletion())
		{
			ActorsNotSafeToBeDeleted.Add(PartitionActor);
		}
		else if (!ActorsNotSafeToBeDeleted.Contains(PartitionActor))
		{
			// Also reset the last generated bounds to indicate to the component to re-create its PartitionActors when it generates.
			for (TObjectPtr<UPCGComponent> PCGComponent : PartitionActor->GetAllOriginalPCGComponents())
			{
				if (PCGComponent)
				{
					PCGComponent->ResetLastGeneratedBounds();
				}
			}

			if (UPackage* ExternalPackage = PartitionActor->GetExternalPackage())
			{
				PackagesToCleanup.Add(ExternalPackage);
			}

			World->DestroyActor(PartitionActor);
		}

		return true;
	};

	auto GatherAndDestroyActors = [&PackagesToCleanup, &PackagesToDeleteFromSCC, World, &ActorsNotSafeToBeDeleted, &GatherAndDestroyLoadedActors](const FWorldPartitionActorDesc* ActorDesc) {
		if (ActorDesc->GetActor())
		{
			GatherAndDestroyLoadedActors(ActorDesc->GetActor());
		}
		else
		{
			PackagesToDeleteFromSCC.Add(ActorDesc->GetActorPackage().ToString());
			World->GetWorldPartition()->RemoveActor(ActorDesc->GetGuid());
		}

		return true;
	};

	// First, clear selection otherwise it might crash
	if (GEditor)
	{
		GEditor->SelectNone(true, true, false);
	}

	if (bOnlyDeleteUnused)
	{		
		// Need to copy to avoid deadlock
		TMap<FIntVector, TObjectPtr<APCGPartitionActor>> PartitionActorsMapCopy;
		{
			FReadScopeLock ReadLock(ActorAndComponentMapping.PartitionActorsMapLock);
			PartitionActorsMapCopy = ActorAndComponentMapping.PartitionActorsMap;
		}

		for (const TPair<FIntVector, TObjectPtr<APCGPartitionActor>>& Item : PartitionActorsMapCopy)
		{
			if (APCGPartitionActor* PartitionActor = Item.Value)
			{
				bool bIntersectWithOneComponent = false;

				auto FoundComponents = [&bIntersectWithOneComponent](UPCGComponent*) { bIntersectWithOneComponent = true; };

				ActorAndComponentMapping.ForAllIntersectingComponents(PartitionActor->GetFixedBounds(), FoundComponents);

				if (!bIntersectWithOneComponent)
				{
					GatherAndDestroyLoadedActors(PartitionActor);
				}
			}
		}
	}
	else
	{
		FWorldPartitionHelpers::ForEachActorDesc<APCGPartitionActor>(World->GetWorldPartition(), GatherAndDestroyActors);

		// Also cleanup the remaining actors that don't have descriptors, if we have a loaded level
		if (ULevel* Level = World->GetCurrentLevel())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DeletePartitionActors::ForEachActorInLevel);
			UPCGActorHelpers::ForEachActorInLevel<APCGPartitionActor>(Level, GatherAndDestroyLoadedActors);
		}
	}

	if (!ActorsNotSafeToBeDeleted.IsEmpty())
	{
		// FIXME: see at the top for this function.
		UE_LOG(LogPCG, Error, TEXT("Tried to delete PCGPartitionActors while their PCGComponent were refreshing. All PCGPartitionActors that are linked to those PCGComponents won't be deleted. You should retry deleting them when the refresh is done."));
	}

	if (PackagesToCleanup.Num() > 0)
	{
		ObjectTools::CleanupAfterSuccessfulDelete(PackagesToCleanup.Array(), /*bPerformanceReferenceCheck=*/true);
	}

	if (PackagesToDeleteFromSCC.Num() > 0)
	{
		FPackageSourceControlHelper PackageHelper;
		if (!PackageHelper.Delete(PackagesToDeleteFromSCC.Array()))
		{
			// Log error...
		}
	}
}

void UPCGSubsystem::NotifyGraphChanged(UPCGGraph* InGraph)
{
	if (GraphExecutor)
	{
		GraphExecutor->NotifyGraphChanged(InGraph);
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
		PCGWorldActor->Modify();
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

const FPCGGraphCompiler* UPCGSubsystem::GetGraphCompiler() const
{
	if (GraphExecutor)
	{
		return GraphExecutor->GetCompiler();
	}

	return nullptr;
}

uint32 UPCGSubsystem::GetGraphCacheEntryCount(IPCGElement* InElement) const
{
	return GraphExecutor ? GraphExecutor->GetGraphCacheEntryCount(InElement) : 0;
}

#endif // WITH_EDITOR

void UPCGSubsystem::FlushCache()
{
	if (GraphExecutor)
	{
		GraphExecutor->GetCache().ClearCache();
	}

#if WITH_EDITOR
	// Garbage collection is very seldom run in the editor, but we currently can consume a lot of memory in the cache.
	if (!PCGHelpers::IsRuntimeOrPIE())
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	}
#endif
}
