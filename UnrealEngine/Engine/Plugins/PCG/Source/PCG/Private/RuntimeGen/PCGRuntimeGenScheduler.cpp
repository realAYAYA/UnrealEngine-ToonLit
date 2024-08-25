// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/PCGRuntimeGenScheduler.h"
#include "RuntimeGen/SchedulingPolicies/PCGSchedulingPolicyBase.h"

#include "PCGActorAndComponentMapping.h"
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "RuntimeGen/GenSources/PCGGenSourceComponent.h"
#include "RuntimeGen/PCGGenSourceManager.h"

#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

namespace PCGRuntimeGenSchedulerConstants
{
	const FString PooledPartitionActorName = TEXT("PCGRuntimeGenPartitionActor_POOLED");
}

namespace PCGRuntimeGenSchedulerHelpers
{
	static TAutoConsoleVariable<bool> CVarRuntimeGenerationEnable(
		TEXT("pcg.RuntimeGeneration.Enable"),
		true,
		TEXT("Enable the RuntimeGeneration system."));

	static TAutoConsoleVariable<int32> CVarFramesBetweenGraphSchedules(
		TEXT("pcg.RuntimeGeneration.FramesBetweenGraphSchedules"),
		0,
		TEXT("Defines the minimum number of frames/ticks between any two Generation schedules in the RuntimeGenScheduler."));

	static TAutoConsoleVariable<bool> CVarRuntimeGenerationEnableDebugging(
		TEXT("pcg.RuntimeGeneration.EnableDebugging"),
		false,
		TEXT("Enable verbose debug logging for the RuntimeGeneration system."));

	static TAutoConsoleVariable<bool> CVarRuntimeGenerationEnablePooling(
		TEXT("pcg.RuntimeGeneration.EnablePooling"),
		true,
		TEXT("Enable PartitionActor pooling for the RuntimeGeneration system."));

	static TAutoConsoleVariable<int32> CVarRuntimeGenerationBasePoolSize(
		TEXT("pcg.RuntimeGeneration.BasePoolSize"),
		100,
		TEXT("Defines the base PartitionActor pool size for the RuntimeGeneration system. Cannot be less than 1."));
}

FPCGRuntimeGenScheduler::FPCGRuntimeGenScheduler(UWorld* InWorld, FPCGActorAndComponentMapping* InActorAndComponentMapping)
{
	check(InWorld && InActorAndComponentMapping);

	World = InWorld;
	Subsystem = UPCGSubsystem::GetInstance(World);
	ActorAndComponentMapping = InActorAndComponentMapping;
	GenSourceManager = new FPCGGenSourceManager(InWorld);
	bPoolingWasEnabledLastFrame = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread();
	BasePoolSizeLastFrame = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread();
}

FPCGRuntimeGenScheduler::~FPCGRuntimeGenScheduler()
{
	delete GenSourceManager;
	GenSourceManager = nullptr;
}

void FPCGRuntimeGenScheduler::Tick(APCGWorldActor* InPCGWorldActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::Tick);
	check(InPCGWorldActor && GenSourceManager);

	// 0. Preamble - check if we should be active in this world and do lazy initialization.

	if (!ShouldTick())
	{
		return;
	}

	TickCVars(InPCGWorldActor);

	TSet<IPCGGenSourceBase*> GenSources;

	if (bAnyRuntimeGenComponentsExist)
	{
		GenSourceManager->Tick();
		GenSources = GenSourceManager->GetAllGenSources(InPCGWorldActor);
	}

	// Initialize RuntimeGen PA pool if necessary. If PoolSize is 0, then we have not initialized the pool yet.
	if (!GenSources.IsEmpty() || !GeneratedComponents.IsEmpty())
	{
		if (PartitionActorPoolSize == 0 && PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
		{
			AddPartitionActorPoolCount(PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());
		}
	}

	CleanupDelayedRefreshComponents();

	// 1. Queue nearby components for generation.
	
	// Mapping of component + coordinates to priorities - needed to compute max priority over all gen sources.
	TMap<FGridGenerationKey, double> ComponentsToGenerate;
	if (!GenSources.IsEmpty())
	{
		TickQueueComponentsForGeneration(GenSources, InPCGWorldActor, ComponentsToGenerate);
	}

	// 2. Schedule cleanup on components that become out of range.

	if (!GeneratedComponents.IsEmpty())
	{
		TickCleanup(GenSources, InPCGWorldActor);
	}

	// 3. Schedule generation on components in priority order.

	if (ScheduleFrameCounter <= 0)
	{
		if (!ComponentsToGenerate.IsEmpty())
		{
			// Sort components by priority (will be generated in descending order).
			ComponentsToGenerate.ValueSort([](double PrioA, double PrioB)->bool { return PrioA > PrioB; });

			TickScheduleGeneration(ComponentsToGenerate);
		}
	}
	else
	{
		--ScheduleFrameCounter;
	}
}

bool FPCGRuntimeGenScheduler::ShouldTick()
{
	check(World && ActorAndComponentMapping);

	if (!PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnable.GetValueOnAnyThread())
	{
		return false;
	}

	// Disable tick of editor scheduling if in runtime or PIE.
	if (PCGHelpers::IsRuntimeOrPIE() && !World->IsGameWorld())
	{
		return false;
	}

#if WITH_EDITOR
	// If we're in an editor world, stop updating preview if the editor window/viewport is not active (follows
	// same behaviour as other things).
	if (!World->IsGameWorld())
	{
		bool bAnyVisible = false;
		for (FEditorViewportClient* EditorViewportClient : GEditor->GetAllViewportClients())
		{
			bAnyVisible |= EditorViewportClient->IsVisible();
		}

		if (!bAnyVisible)
		{
			return false;
		}
	}
#endif

	if (bAnyRuntimeGenComponentsExistDirty)
	{
		const bool bDidAnyRuntimeGenComponentsExist = bAnyRuntimeGenComponentsExist;
		bAnyRuntimeGenComponentsExist = ActorAndComponentMapping->AnyRuntimeGenComponentsExist();
		bAnyRuntimeGenComponentsExistDirty = false;

		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
		{
			if (bDidAnyRuntimeGenComponentsExist != bAnyRuntimeGenComponentsExist)
			{
				if (bAnyRuntimeGenComponentsExist)
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] THERE ARE NOW RUNTIME COMPONENTS IN THE LEVEL. SCHEDULER WILL BEGIN TICKING."));
				}
				else
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] THERE ARE NO MORE RUNTIME COMPONENTS. SCHEDULER WILL ONLY TICK TO CLEANUP."));
				}
			}
		}
	}

	// We can stop ticking if there are no runtime gen components alive and there are no generated components that need cleaning up.
	if (!bAnyRuntimeGenComponentsExist && GeneratedComponents.IsEmpty())
	{
		return false;
	}

	return true;
}

void FPCGRuntimeGenScheduler::TickQueueComponentsForGeneration(
	const TSet<IPCGGenSourceBase*>& InGenSources,
	APCGWorldActor* InPCGWorldActor,
	TMap<FGridGenerationKey, double>& OutComponentsToGenerate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickQueueComponentsForGeneration);

	check(ActorAndComponentMapping && InPCGWorldActor);

	// TODO: Thought - it would be possible to maintain a global maximum generation distance across all components
	// perhaps in the actor&comp mapping system, and then do a spatial query to get the components here.

	auto AddComponentToGenerate = [&OutComponentsToGenerate](FGridGenerationKey& InKey, const IPCGGenSourceBase* InGenSource, const UPCGSchedulingPolicyBase* InPolicy, const FBox& InComponentBounds, bool bInUse2DGrid)
	{
		const double PolicyPriority = InPolicy ? InPolicy->CalculatePriority(InGenSource, InComponentBounds, bInUse2DGrid) : 0.0;
		double Priority = FMath::Clamp(PolicyPriority, 0.0, 1.0);
		if (PolicyPriority != Priority)
		{
			UE_LOG(LogPCG, Warning, TEXT("Priority from runtime generation policy (%lf) outside [0.0, 1.0] range, clamped."), PolicyPriority);
		}

		// Generate largest grid to smallest (and unbounded is larger than any grid).
		const uint32 GridSize = InKey.GetGridSize();
		Priority += GridSize;

		double* ExistingPriority = OutComponentsToGenerate.Find(InKey);
		if (!ExistingPriority)
		{
			OutComponentsToGenerate.Add(InKey, Priority);
		}
		else if (Priority > *ExistingPriority)
		{
			// If this generation source prioritizes this grid cell higher, then bump the priority.
			*ExistingPriority = Priority;
		}
	};

	// Collect local components from all partitioned components.
	for (UPCGComponent* OriginalComponent : ActorAndComponentMapping->PartitionedOctree.GetAllComponents())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CollectLocalComponents);

		if (!ensure(OriginalComponent) || !OriginalComponent->GetGraph())
		{
			continue;
		}

		const UPCGSchedulingPolicyBase* Policy = OriginalComponent->GetRuntimeGenSchedulingPolicy();

		// TODO: For each execution domain (for now only GenAtRuntime/dynamic), assuming we run Preview through this scheduler, which it seems like we will.
		if (OriginalComponent->IsManagedByRuntimeGenSystem())
		{
			bool bHasUnbounded = false;
			PCGHiGenGrid::FSizeArray GridSizes;
			ensure(PCGHelpers::GetGenerationGridSizes(OriginalComponent->GetGraph(), InPCGWorldActor, GridSizes, bHasUnbounded));

			if (GridSizes.IsEmpty() && !bHasUnbounded)
			{
				continue;
			}

			const EPCGHiGenGrid MaxGrid = bHasUnbounded ? EPCGHiGenGrid::Unbounded : PCGHiGenGrid::GridSizeToGrid(GridSizes[0]);
			const double MaxGenerationRadius = OriginalComponent->GetGenerationRadiusFromGrid(MaxGrid);

			for (const IPCGGenSourceBase* GenSource : InGenSources)
			{
				const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
				if (!GenSourcePositionOptional.IsSet())
				{
					continue;
				}

				const FVector GenSourcePosition = GenSourcePositionOptional.GetValue();
				const FBox OriginalComponentBounds = OriginalComponent->GetGridBounds();

				FVector ModifiedGenSourcePosition = GenSourcePosition;
				if (InPCGWorldActor->bUse2DGrid)
				{
					ModifiedGenSourcePosition.Z = OriginalComponentBounds.Min.Z;
				}

				const double DistanceSquared = OriginalComponentBounds.ComputeSquaredDistanceToPoint(ModifiedGenSourcePosition);

				if (DistanceSquared > MaxGenerationRadius * MaxGenerationRadius)
				{
					// GenSource is not within range of the component, skip!
					continue;
				}

				if (bHasUnbounded)
				{
					// Ignore components that have already been generated or marked for generation. Unbounded grid size means not-partitioned.
					FGridGenerationKey Key(PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent);
					if (!GeneratedComponents.Contains(Key))
					{
						AddComponentToGenerate(Key, GenSource, Policy, OriginalComponentBounds, InPCGWorldActor->bUse2DGrid);
					}
				}

				// TODO: once one of the larger grid sizes is out of range, we can forego checking any smaller grid sizes. they can't possibly be closer!
				// This assumes we enforce generation radii to increase monotonically.
				for (const uint32 GridSize : GridSizes)
				{
					ensure(PCGHiGenGrid::IsValidGridSize(GridSize));

					const FIntVector GenSourceGridPosition = UPCGActorHelpers::GetCellCoord(GenSourcePosition, GridSize, InPCGWorldActor->bUse2DGrid);
					const double GenerationRadius = OriginalComponent->GetGenerationRadiusFromGrid(PCGHiGenGrid::GridSizeToGrid(GridSize));
					const int32 GridRadius = FMath::CeilToInt32(GenerationRadius / GridSize); // Radius discretized to # of grid cells.
					const int32 VerticalGridRadius = InPCGWorldActor->bUse2DGrid ? 0 : GridRadius; // Flatten the vertical grid radius in the 2D case.

					const double HalfGridSize = GridSize / 2.0f;
					FVector HalfExtent(HalfGridSize, HalfGridSize, HalfGridSize);

					FBox ModifiedBounds = OriginalComponent->GetGridBounds();
					if (InPCGWorldActor->bUse2DGrid)
					{
						FVector MinBounds = ModifiedBounds.Min;
						FVector MaxBounds = ModifiedBounds.Max;

						MinBounds.Z = 0;
						MaxBounds.Z = GridSize;
						ModifiedBounds = FBox(MinBounds, MaxBounds);

						// In case of 2D grid, it's like the actor has infinite bounds on the Z axis.
						HalfExtent.Z = HALF_WORLD_MAX1;
					}

					// TODO: Perhaps rasterize sphere instead of walking a naive cube. although maybe the perf on that isn't worthwhile.
					for (int32 Z = GenSourceGridPosition.Z - VerticalGridRadius; Z <= GenSourceGridPosition.Z + VerticalGridRadius; ++Z)
					{
						for (int32 Y = GenSourceGridPosition.Y - GridRadius; Y <= GenSourceGridPosition.Y + GridRadius; ++Y)
						{
							for (int32 X = GenSourceGridPosition.X - GridRadius; X <= GenSourceGridPosition.X + GridRadius; ++X)
							{
								FIntVector GridCoords(X, Y, Z);
								FGridGenerationKey Key(GridSize, GridCoords, OriginalComponent);

								// Ignore components that have already been generated or marked for generation.
								if (GeneratedComponents.Find({ GridSize, GridCoords, OriginalComponent }))
								{
									continue;
								}

								const FVector Center = FVector(GridCoords.X + 0.5, GridCoords.Y + 0.5, GridCoords.Z + 0.5) * GridSize;
								const FBox CellBounds(Center - HalfExtent, Center + HalfExtent);

								// Overlap cell with the partitioned component.
								const FBox IntersectedBounds = ModifiedBounds.Overlap(CellBounds);
								if (!IntersectedBounds.IsValid || IntersectedBounds.GetVolume() <= UE_DOUBLE_SMALL_NUMBER)
								{
									continue;
								}

								if (InPCGWorldActor->bUse2DGrid)
								{
									ModifiedGenSourcePosition.Z = IntersectedBounds.Min.Z;
								}

								// Verify the grid cell actually lies within the generation radius.
								// TODO: this is no longer necessary if we rasterize the sphere instead.
								const double LocalDistanceSquared = CellBounds.ComputeSquaredDistanceToPoint(ModifiedGenSourcePosition);
								if (LocalDistanceSquared <= GenerationRadius * GenerationRadius)
								{
									AddComponentToGenerate(Key, GenSource, Policy, IntersectedBounds, InPCGWorldActor->bUse2DGrid);
								}
							}
						}
					}
				}
			}
		}
	}

	// Collect all non-partitioned components.
	for (UPCGComponent* OriginalComponent : ActorAndComponentMapping->NonPartitionedOctree.GetAllComponents())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CollectNonPartitionedComponents);

		if (!ensure(OriginalComponent) || !OriginalComponent->GetGraph())
		{
			continue;
		}

		// The generation key for a non-partitioned component should always have unbounded grid size and 0,0,0 cell coord.
		if (GeneratedComponents.Contains({ PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent }))
		{
			continue;
		}

		const UPCGSchedulingPolicyBase* Policy = OriginalComponent->GetRuntimeGenSchedulingPolicy();

		// TODO: For each execution domain (for now only GenAtRuntime/dynamic), assuming we run Preview through this scheduler, which it seems like we will.
		if (OriginalComponent->IsManagedByRuntimeGenSystem())
		{
			// Unbounded will grab the base GenerationRadius used for non-partitioned and unbounded.
			const double MaxGenerationRadius = OriginalComponent->GetGenerationRadiusFromGrid(EPCGHiGenGrid::Unbounded);

			for (const IPCGGenSourceBase* GenSource : InGenSources)
			{
				const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
				if (!GenSourcePositionOptional.IsSet())
				{
					continue;
				}

				const FVector GenSourcePosition = GenSourcePositionOptional.GetValue();
				const FBox OriginalComponentBounds = OriginalComponent->GetGridBounds();

				FVector ModifiedGenSourcePosition = GenSourcePosition;
				if (InPCGWorldActor->bUse2DGrid)
				{
					ModifiedGenSourcePosition.Z = OriginalComponentBounds.Min.Z;
				}

				const double DistanceSquared = OriginalComponentBounds.ComputeSquaredDistanceToPoint(ModifiedGenSourcePosition);

				// Max radius for a non-partitioned component is just the base GenerationRadius.
				if (DistanceSquared <= MaxGenerationRadius * MaxGenerationRadius)
				{
					// Unbounded grid size means not-partitioned.
					FGridGenerationKey Key(PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent);
					AddComponentToGenerate(Key, GenSource, Policy, OriginalComponentBounds, /*bUse2DGrid=*/false);
				}
			}
		}
	}
}

void FPCGRuntimeGenScheduler::TickCleanup(const TSet<IPCGGenSourceBase*>& InGenSources, const APCGWorldActor* InPCGWorldActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickCleanup);

	check(ActorAndComponentMapping && InPCGWorldActor);

	// Generated Entry Key, Local/Generated Component
	using PCGComponentToClean = TTuple<FGridGenerationKey, UPCGComponent*>;
	TArray<PCGComponentToClean> ComponentsToClean;

	// Find any generated components which should be cleaned.
	for (FGridGenerationKey GenerationKey : GeneratedComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::SelectComponentsToCleanup);

		const uint32 GridSize = GenerationKey.GetGridSize();
		const FIntVector& GridCoords = GenerationKey.GetGridCoords();
		UPCGComponent* OriginalComponent = GenerationKey.GetOriginalComponent();
		check(OriginalComponent);

		const EPCGHiGenGrid Grid = PCGHiGenGrid::GridSizeToGrid(GridSize);

		// If the Grid is unbounded, we have a non-partitioned or unbounded component.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			const FBox Bounds = OriginalComponent->GetGridBounds();

			double MinSquaredDistanceToGenSource = UE_DOUBLE_BIG_NUMBER;
			for (const IPCGGenSourceBase* GenSource : InGenSources)
			{
				if (!ensure(GenSource))
				{
					continue;
				}

				const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
				if (!GenSourcePositionOptional.IsSet())
				{
					continue;
				}

				FVector GenSourcePosition = GenSourcePositionOptional.GetValue();

				// Only consider 2D distance when using a 2D grid.
				if (InPCGWorldActor->bUse2DGrid)
				{
					GenSourcePosition.Z = Bounds.Min.Z;
				}

				MinSquaredDistanceToGenSource = FMath::Min(MinSquaredDistanceToGenSource, Bounds.ComputeSquaredDistanceToPoint(GenSourcePosition));
			}

			const double CleanupRadius = OriginalComponent->GetCleanupRadiusFromGrid(Grid);
			if (MinSquaredDistanceToGenSource > CleanupRadius * CleanupRadius)
			{
				ComponentsToClean.Add({ GenerationKey, OriginalComponent });
			}
		}
		// Otherwise, we have a local component.
		else
		{
			UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridSize, GridCoords, OriginalComponent, /*bRuntimeGenerated=*/true);
			APCGPartitionActor* PartitionActor = LocalComponent ? Cast<APCGPartitionActor>(LocalComponent->GetOwner()) : nullptr;
			if (!PartitionActor)
			{
				// Attempt to clean even in failure case to avoid leaking resources.
				ComponentsToClean.Add({ GenerationKey, LocalComponent });
				continue;
			}

			const FBox GridBounds = PartitionActor->GetFixedBounds();

			double MinSquaredDistanceToGenSource = UE_DOUBLE_BIG_NUMBER;
			for (const IPCGGenSourceBase* GenSource : InGenSources)
			{
				if (!ensure(GenSource))
				{
					continue;
				}

				const TOptional<FVector> GenSourcePositionOptional = GenSource->GetPosition();
				if (!GenSourcePositionOptional.IsSet())
				{
					continue;
				}

				FVector GenSourcePosition = GenSourcePositionOptional.GetValue();

				// Only consider 2D distance when using a 2D grid.
				if (InPCGWorldActor->bUse2DGrid)
				{
					GenSourcePosition.Z = GridBounds.Min.Z;
				}

				MinSquaredDistanceToGenSource = FMath::Min(MinSquaredDistanceToGenSource, GridBounds.ComputeSquaredDistanceToPoint(GenSourcePosition));
			}

			const double CleanupRadius = OriginalComponent->GetCleanupRadiusFromGrid(Grid);
			if (MinSquaredDistanceToGenSource > CleanupRadius * CleanupRadius)
			{
				ComponentsToClean.Add({ GenerationKey, LocalComponent });
			}
		}
	}

	for (const PCGComponentToClean& ComponentToClean : ComponentsToClean)
	{
		CleanupComponent(ComponentToClean.Get<0>(), ComponentToClean.Get<1>());
	}
}

void FPCGRuntimeGenScheduler::TickScheduleGeneration(TMap<FGridGenerationKey, double>& ComponentsToGenerate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::TickScheduleGeneration);

	check(Subsystem && ActorAndComponentMapping);

	for (const TPair<FGridGenerationKey, double>& GenerateEntry : ComponentsToGenerate)
	{
		const FGridGenerationKey& Key = GenerateEntry.Key;
		const double Priority = GenerateEntry.Value;

		const uint32 GridSize = GenerateEntry.Key.GetGridSize();
		const FIntVector GridCoords = GenerateEntry.Key.GetGridCoords();
		UPCGComponent* OriginalComponent = GenerateEntry.Key.GetOriginalComponent();
		check(OriginalComponent);

		const EPCGHiGenGrid Grid = PCGHiGenGrid::GridSizeToGrid(GridSize);

		// If the Grid is unbounded, we have a non-partitioned or unbounded component.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			if (ensure(OriginalComponent) && !OriginalComponent->IsGenerating())
			{
				if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread() && OriginalComponent->GetOwner())
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] GENERATE: '%s' (priority %lf)"), *OriginalComponent->GetOwner()->GetActorNameOrLabel(), Priority);
				}

				// Force to refresh if the component is already generated.
				OriginalComponent->GenerateLocal(EPCGComponentGenerationTrigger::GenerateAtRuntime, /*bForce=*/true, Grid);
			}
		}
		// Otherwise we have a local component.
		else
		{
			// Grab local component and PA if they exist already.
			UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridSize, GridCoords, OriginalComponent, /*bRuntimeGenerated=*/true);
			TObjectPtr<APCGPartitionActor> PartitionActor = LocalComponent ? Cast<APCGPartitionActor>(LocalComponent->GetOwner()) : nullptr;

			if (!LocalComponent || !ensure(PartitionActor))
			{
				// Local component & PA do not exist, create them.
				if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
				{
					// Get RuntimeGenPA from pool.
					PartitionActor = GetPartitionActorFromPool(GridSize, GridCoords);

					if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] UNPOOL PARTITION ACTOR: '%s' (priority %lf, %d remaining out of %d)"),
							*APCGPartitionActor::GetPCGPartitionActorName(GridSize, GridCoords, /*bRuntimeGenerated=*/true),
							Priority,
							PartitionActorPool.Num(),
							PartitionActorPoolSize);
					}
				}
				else
				{
					if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] CREATE PARTITION ACTOR: '%s' (priority %lf)"),
							*APCGPartitionActor::GetPCGPartitionActorName(GridSize, GridCoords, /*bRuntimeGenerated=*/true),
							Priority);
					}

					// Find or Create RuntimeGenPA.
					PartitionActor = Subsystem->FindOrCreatePCGPartitionActor(FGuid(), GridSize, GridCoords, /*bRuntimeGenerated=*/true);
				}

				if (!ensure(PartitionActor))
				{
					continue;
				}

				// Update component mapping for this PA (add local component).
				{
					FWriteScopeLock WriteLock(ActorAndComponentMapping->ComponentToRuntimeGenPartitionActorsMapLock);
					TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ActorAndComponentMapping->ComponentToRuntimeGenPartitionActorsMap.Find(OriginalComponent);

					if (!PartitionActorsPtr)
					{
						PartitionActorsPtr = &ActorAndComponentMapping->ComponentToRuntimeGenPartitionActorsMap.Emplace(OriginalComponent);
					}

					// Log this original component before setting up the PA, so that we early out from RefreshComponent if it gets called
					// in the AddGraphInstance call below.
					OriginalComponentBeingGenerated = OriginalComponent;

					PartitionActor->AddGraphInstance(OriginalComponent);

					OriginalComponentBeingGenerated = nullptr;

					PartitionActorsPtr->Add(PartitionActor);
				}

				// Generate local component.
				LocalComponent = PartitionActor->GetLocalComponent(OriginalComponent);
			}

			if (ensure(LocalComponent) && !LocalComponent->IsGenerating())
			{
				if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] GENERATE: '%s' (priority %lf)"), *PartitionActor->GetActorNameOrLabel(), Priority);
				}

				// Higen graphs may have data links from original component to local components. The original component will be given a higher priority than local
				// components and will start generating first. If it is currently generating, local component needs to take a dependency to ensure execution completes.
				TArray<FPCGTaskId> Dependencies;
				if (OriginalComponent->IsGenerating() && OriginalComponent->GetGraph() && OriginalComponent->GetGraph()->IsHierarchicalGenerationEnabled())
				{
					const FPCGTaskId TaskId = OriginalComponent->GetGenerationTaskId();

					if (TaskId != InvalidPCGTaskId)
					{
						Dependencies.Add(TaskId);
					}
				}

				// Force to refresh if the component is already generated.
				LocalComponent->GenerateLocal(EPCGComponentGenerationTrigger::GenerateAtRuntime, /*bForce=*/true, LocalComponent->GetGenerationGrid(), Dependencies);
			}
		}

		GeneratedComponents.Add(Key);

		ScheduleFrameCounter += PCGRuntimeGenSchedulerHelpers::CVarFramesBetweenGraphSchedules.GetValueOnAnyThread();
		if (ScheduleFrameCounter > 0)
		{
			break;
		}
	}
}

void FPCGRuntimeGenScheduler::TickCVars(const APCGWorldActor* InPCGWorldActor)
{
	// If pooling has been disabled since last frame, we should destroy the pool.
	const bool bPoolingEnabled = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread();

	if (bPoolingWasEnabledLastFrame && !bPoolingEnabled)
	{
		CleanupLocalComponents(InPCGWorldActor);
		ResetPartitionActorPoolToSize(/*NewPoolSize=*/0);
	}

	bPoolingWasEnabledLastFrame = bPoolingEnabled;

	// Handle when the base PA PoolSize is modified. Cleanup all local components and reset the pool with the correct number of PAs.
	if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
	{
		// Don't allow a pool size <= 0
		const uint32 BasePoolSize = FMath::Max(1, PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());

		if (BasePoolSizeLastFrame != BasePoolSize)
		{
			BasePoolSizeLastFrame = BasePoolSize;

			CleanupLocalComponents(InPCGWorldActor);
			ResetPartitionActorPoolToSize(BasePoolSize);
		}
	}
}

void FPCGRuntimeGenScheduler::OnOriginalComponentRegistered(UPCGComponent* InOriginalComponent)
{
	// Ensure we are non-local runtime managed component.
	if (!InOriginalComponent || !InOriginalComponent->IsManagedByRuntimeGenSystem() || Cast<APCGPartitionActor>(InOriginalComponent->GetOwner()))
	{
		return;
	}

	CreateGridGuidsForComponent(InOriginalComponent);

	// When an original/non-partitioned component is registered, we need to dirty the state.
	bAnyRuntimeGenComponentsExistDirty = true;
}

void FPCGRuntimeGenScheduler::OnOriginalComponentUnregistered(UPCGComponent* InOriginalComponent)
{
	if (!InOriginalComponent || Cast<APCGPartitionActor>(InOriginalComponent->GetOwner()))
	{
		return;
	}

	check(ActorAndComponentMapping);

	// When an original/non-partitioned component is unregistered, we need to dirty the state.
	bAnyRuntimeGenComponentsExistDirty = true;

	// Gather all generated components which originated from this original component.
	TSet<FGridGenerationKey> KeysToCleanup;
	for (FGridGenerationKey GenerationKey : GeneratedComponents)
	{
		if (GenerationKey.GetOriginalComponent() == InOriginalComponent)
		{
			KeysToCleanup.Add(GenerationKey);
		}
	}

	for (FGridGenerationKey GenerationKey : KeysToCleanup)
	{
		const uint32 GridSize = GenerationKey.GetGridSize();
		const bool bIsOriginalComponent = GridSize == PCGHiGenGrid::UnboundedGridSize();

		// Get the generated component for this key (might be a local component).
		UPCGComponent* ComponentToCleanup = bIsOriginalComponent ? InOriginalComponent : ActorAndComponentMapping->GetLocalComponent(GridSize, GenerationKey.GetGridCoords(), InOriginalComponent, /*bRuntimeGenerated=*/true);

		if (ensure(ComponentToCleanup))
		{
			CleanupComponent(GenerationKey, ComponentToCleanup);
		}
	}
}

void FPCGRuntimeGenScheduler::CleanupLocalComponents(const APCGWorldActor* InPCGWorldActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CleanupLocalComponents);

	check(ActorAndComponentMapping && InPCGWorldActor);

	// Generated Entry Key, Local/Generated Component
	using PCGComponentToClean = TTuple<FGridGenerationKey, UPCGComponent*>;
	TArray<PCGComponentToClean> ComponentsToClean;

	// Find all generated local components.
	for (FGridGenerationKey GenerationKey : GeneratedComponents)
	{
		const uint32 GridSize = GenerationKey.GetGridSize();
		const FIntVector& GridCoords = GenerationKey.GetGridCoords();
		UPCGComponent* OriginalComponent = GenerationKey.GetOriginalComponent();
		check(OriginalComponent);

		const EPCGHiGenGrid Grid = PCGHiGenGrid::GridSizeToGrid(GridSize);

		// Only operate on LocalComponents.
		if (Grid != EPCGHiGenGrid::Unbounded)
		{
			UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridSize, GridCoords, OriginalComponent, /*bRuntimeGenerated=*/true);
			ComponentsToClean.Add({ GenerationKey, LocalComponent });
		}
	}

	for (const PCGComponentToClean& ComponentToClean : ComponentsToClean)
	{
		CleanupComponent(ComponentToClean.Get<0>(), ComponentToClean.Get<1>());
	}
}

void FPCGRuntimeGenScheduler::CleanupComponent(const FGridGenerationKey& GenerationKey, UPCGComponent* GeneratedComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CleanupComponent);

	check(ActorAndComponentMapping);

	const uint32 GridSize = GenerationKey.GetGridSize();
	const FIntVector GridCoords = GenerationKey.GetGridCoords();

	APCGPartitionActor* PartitionActor = nullptr;

	if (!GeneratedComponent)
	{
		UE_LOG(LogPCG, Warning, TEXT("Runtime generated component could not be recovered on grid %d at (%d, %d, %d). It has been lost or destroyed."), GridSize, GridCoords.X, GridCoords.Y, GridCoords.Z);

		// If the GeneratedComponent has been lost for some reason, get the PA directly from the ActorAndComponentMapping.
		PartitionActor = ActorAndComponentMapping->GetPartitionActor(GridSize, GridCoords, /*bRuntimeGenerated=*/true);
	}
	else // If the GeneratedComponent does exist, we can clean it up!.
	{
		PartitionActor = Cast<APCGPartitionActor>(GeneratedComponent->GetOwner());
		const EPCGHiGenGrid Grid = PCGHiGenGrid::GridSizeToGrid(GridSize);

		// If the Grid is unbounded, we have a non-partitioned or unbounded component.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread() && GeneratedComponent->GetOwner())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] CLEANUP: '%s'"), *GeneratedComponent->GetOwner()->GetActorNameOrLabel());
			}

			GeneratedComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
		}
		// Otherwise we have a local component.
		else if (ensure(PartitionActor))
		{
			UPCGComponent* OriginalComponent = PartitionActor->GetOriginalComponent(GeneratedComponent);

			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] CLEANUP: '%s'"), *PartitionActor->GetActorNameOrLabel());
			}

			// This performs a CleanupLocalImmediate for us, no need to clean up ourselves.
			PartitionActor->RemoveGraphInstance(OriginalComponent);

			// Remove component mapping.
			{
				FWriteScopeLock WriteLock(ActorAndComponentMapping->ComponentToRuntimeGenPartitionActorsMapLock);
				TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ActorAndComponentMapping->ComponentToRuntimeGenPartitionActorsMap.Find(OriginalComponent);

				if (PartitionActorsPtr)
				{
					PartitionActorsPtr->Remove(PartitionActor);

					if (PartitionActorsPtr->IsEmpty())
					{
						ActorAndComponentMapping->ComponentToRuntimeGenPartitionActorsMap.Remove(OriginalComponent);
					}
				}
			}
		}
	}

	// Cleanup the PA if it no longer has any components (return to pool or destroy).
	if (PartitionActor && PartitionActor->GetAllLocalPCGComponents().IsEmpty())
	{
		PartitionActor->UnregisterPCG();

		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnablePooling.GetValueOnAnyThread())
		{
			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] RETURNING PARTITION ACTOR TO POOL: '%s' (%d remaining out of %d)"), *PartitionActor->GetActorNameOrLabel(), PartitionActorPool.Num() + 1, PartitionActorPoolSize);
			}

#if WITH_EDITOR
			PartitionActor->Rename(nullptr, PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders);
			PartitionActor->SetActorLabel(*PCGRuntimeGenSchedulerConstants::PooledPartitionActorName);
#endif
			PartitionActorPool.Push(PartitionActor);
		}
		else
		{
			if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] DESTROY PARTITION ACTOR: '%s'"), *PartitionActor->GetActorNameOrLabel());
			}

#if WITH_EDITOR
			PartitionActor->Rename(nullptr, PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders);
#endif
			World->DestroyActor(PartitionActor);
		}
	}

	GeneratedComponents.Remove(GenerationKey);
}

void FPCGRuntimeGenScheduler::CleanupDelayedRefreshComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::CleanupDelayedRefreshComponents);

	check(ActorAndComponentMapping);

	// Check that each refreshed local component is still intersecting its original component.
	// If it is not, it would be leaked instead of refreshed, so we should force a full cleanup.
	for (const FGridGenerationKey& GenerationKey : GeneratedComponentsToRemove)
	{
		const uint32 GridSize = GenerationKey.GetGridSize();
		const EPCGHiGenGrid Grid = PCGHiGenGrid::GridSizeToGrid(GridSize);

		// The unbounded grid level will always lie inside the original component, so we can skip it.
		if (Grid == EPCGHiGenGrid::Unbounded)
		{
			continue;
		}

		const UPCGComponent* OriginalComponent = GenerationKey.GetOriginalComponent();
		const FIntVector& GridCoords = GenerationKey.GetGridCoords();

		UPCGComponent* LocalComponent = OriginalComponent ? ActorAndComponentMapping->GetLocalComponent(GridSize, GridCoords, OriginalComponent, /*bRuntimeGenerated=*/true) : nullptr;
		APCGPartitionActor* PartitionActor = LocalComponent ? Cast<APCGPartitionActor>(LocalComponent->GetOwner()) : nullptr;

		if (LocalComponent && PartitionActor)
		{
			const FBox OriginalBounds = OriginalComponent->GetGridBounds();
			const FBox LocalBounds = PartitionActor->GetFixedBounds();

			if (!OriginalBounds.Intersect(LocalBounds))
			{
				CleanupComponent(GenerationKey, LocalComponent);
			}
		}
		else
		{
			// If the component or partition actor could not be recovered, just clean up.
			CleanupComponent(GenerationKey, /*GenerationKey=*/nullptr);
		}
	}

	// Remove any remaining generation keys that have been registered for deferred removal.
	GeneratedComponents = GeneratedComponents.Difference(GeneratedComponentsToRemove);
	GeneratedComponentsToRemove.Empty();
}

void FPCGRuntimeGenScheduler::RefreshComponent(UPCGComponent* InComponent, bool bRemovePartitionActors)
{
	if (!InComponent || !ensure(IsInGameThread()))
	{
		return;
	}

	APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(InComponent->GetOwner());
	const bool bIsLocalComponent = PartitionActor != nullptr;
	UPCGComponent* OriginalComponent = bIsLocalComponent ? PartitionActor->GetOriginalComponent(InComponent) : InComponent;

	// If we are mid way through setting up an original component, early out from this refresh.
	if (!OriginalComponent || OriginalComponent == OriginalComponentBeingGenerated)
	{
		return;
	}

	const bool bLoggingEnabled = PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread();

	// Useful because we can run into generation order issues if components are left to continue generating.
	if (InComponent->IsGenerating())
	{
		InComponent->CancelGeneration();
	}

	if (!bRemovePartitionActors)
	{
		// Refresh path - mark component dirty and removed generated keys which will cause it to be scheduled for regeneration.

		// Register for deferred removal from generated components set, component will be regenerated later (and in grid order
		// so that e.g. unbounded is generated first).
		if (PartitionActor)
		{
			if (bLoggingEnabled)
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] SHALLOW REFRESH LOCAL COMPONENT: '%s'"), *PartitionActor->GetActorNameOrLabel());
			}

			GeneratedComponentsToRemove.Emplace({ PartitionActor->GetPCGGridSize(), PartitionActor->GetGridCoord(), OriginalComponent });
			InComponent->CleanupLocalImmediate(/*bRemoveComponents=*/false);
		}
		else
		{
			// Register original component for deferred removal.
			GeneratedComponentsToRemove.Emplace({ PCGHiGenGrid::UnboundedGridSize(), FIntVector(0), OriginalComponent });

			// Register local components for deferred removal if they have not already registered themselves.
			for (const FGridGenerationKey& Key : GeneratedComponents)
			{
				if (Key.GetOriginalComponent() == InComponent && !GeneratedComponentsToRemove.Contains(Key))
				{
					// TODO - clean up local immediate will have a flag in the future to clean up the local components on its own, so this call to CleanupLocalImmediate will not be required
					UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(
						Key.GetGridSize(),
						Key.GetGridCoords(),
						OriginalComponent,
						/*bRuntimeGenerated=*/true);

					if (bLoggingEnabled && LocalComponent && LocalComponent->GetOwner())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] SHALLOW REFRESH LOCAL COMPONENT: '%s'"), *LocalComponent->GetOwner()->GetActorNameOrLabel());
					}

					if (ensure(LocalComponent))
					{
						LocalComponent->CleanupLocalImmediate(/*bRemoveComponents=*/false);
					}

					GeneratedComponentsToRemove.Add(Key);
				}
			}

			if (bLoggingEnabled && OriginalComponent->GetOwner())
			{
				UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] SHALLOW REFRESH COMPONENT: '%s' PARTITIONED: %d"),
					*OriginalComponent->GetOwner()->GetActorNameOrLabel(),
					OriginalComponent->IsPartitioned() ? 1 : 0);
			}

			InComponent->CleanupLocalImmediate(/*bRemoveComponents=*/false);
		}
	}
	else
	{
		// Full cleanout path - cleanup existing components and return actors to the pool.

		auto RefreshLocalComponent = [this, OriginalComponent, bLoggingEnabled](UPCGComponent* LocalComponent)
		{
			check(LocalComponent);
			APCGPartitionActor* PartitionActor = CastChecked<APCGPartitionActor>(LocalComponent->GetOwner());

			// Find the specific generation key for this component, if it exists, cleanup and generate.
			FGridGenerationKey LocalComponentKey(PartitionActor->GetPCGGridSize(), PartitionActor->GetGridCoord(), OriginalComponent);

			if (GeneratedComponents.Find(LocalComponentKey))
			{
				if (bLoggingEnabled)
				{
					UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] DEEP REFRESH LOCAL COMPONENT: '%s'"), *PartitionActor->GetActorNameOrLabel());
				}

				CleanupComponent(LocalComponentKey, LocalComponent);
			}
		};

		if (bIsLocalComponent)
		{
			RefreshLocalComponent(InComponent);
		}
		else
		{
			TArray<FGridGenerationKey> GenerationKeys;

			for (FGridGenerationKey GenerationKey : GeneratedComponents)
			{
				if (GenerationKey.GetOriginalComponent() == OriginalComponent)
				{
					GenerationKeys.Add(GenerationKey);
				}
			}

			// Gather all generated components which originated from this original component.
			for (FGridGenerationKey GenerationKey : GenerationKeys)
			{
				const uint32 GridSize = GenerationKey.GetGridSize();
				const EPCGHiGenGrid Grid = PCGHiGenGrid::GridSizeToGrid(GridSize);

				// If the Grid is unbounded, we have a non-partitioned or unbounded component.
				if (Grid == EPCGHiGenGrid::Unbounded)
				{
					if (bLoggingEnabled && OriginalComponent->GetOwner())
					{
						UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] DEEP REFRESH COMPONENT: '%s' PARTITIONED: %d"),
							*OriginalComponent->GetOwner()->GetActorNameOrLabel(),
							OriginalComponent->IsPartitioned() ? 1 : 0);
					}

					CleanupComponent(GenerationKey, OriginalComponent);
				}
				// Otherwise we have a local component.
				else
				{
					const FIntVector GridCoords = GenerationKey.GetGridCoords();

					if (UPCGComponent* LocalComponent = ActorAndComponentMapping->GetLocalComponent(GridSize, GridCoords, OriginalComponent, /*bRuntimeGenerated=*/true))
					{
						RefreshLocalComponent(LocalComponent);
					}
					else
					{
						// If the local component could not be recovered, cleanup its entry to avoid leaking resources/locking the grid cell.
						CleanupComponent(GenerationKey, nullptr);
					}
				}
			}
		}
	}

	if (!bIsLocalComponent)
	{
		// When an original/non-partitioned component is refreshed, we need to dirty the state.
		bAnyRuntimeGenComponentsExistDirty = true;
	}
}

APCGPartitionActor* FPCGRuntimeGenScheduler::GetPartitionActorFromPool(uint32 GridSize, const FIntVector& GridCoords)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::GetPartitionActorFromPool);

	check(ActorAndComponentMapping);

	if (!World)
	{
		UE_LOG(LogPCG, Error, TEXT("[GetPartitionActorFromPool] World is null."));
		return nullptr;
	}

	// Attempt to find an existing RuntimeGen PA.
	if (APCGPartitionActor* ExistingActor = ActorAndComponentMapping->GetPartitionActor(GridSize, GridCoords, /*bRuntimeGenerated=*/true))
	{
		return ExistingActor;
	}

	// Double size of the pool if it is empty.
	if (PartitionActorPool.IsEmpty())
	{
		// If PartitionActorPoolSize is zero, then we should use the CVarBasePoolSize instead. Result must always at least be >= 1.
		const uint32 CurrentPoolSize = FMath::Max(1, PartitionActorPoolSize > 0 ? PartitionActorPoolSize : PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationBasePoolSize.GetValueOnAnyThread());

		if (PCGRuntimeGenSchedulerHelpers::CVarRuntimeGenerationEnableDebugging.GetValueOnAnyThread())
		{
			UE_LOG(LogPCG, Warning, TEXT("[RUNTIMEGEN] INCREASING TRANSIENT PARTITION ACTOR POOL SIZE BY (%d)"), CurrentPoolSize);
		}

		// If pooling was enabled late, the editor world RuntimeGenScheduler will not have created the initial pool, so we should create it now.
		AddPartitionActorPoolCount(CurrentPoolSize);
	}

	check(!PartitionActorPool.IsEmpty());
	APCGPartitionActor* PartitionActor = PartitionActorPool.Pop();

#if WITH_EDITOR
	const FName ActorName = *APCGPartitionActor::GetPCGPartitionActorName(GridSize, GridCoords, /*bRuntimeGenerated=*/true);

	PartitionActor->Rename(*ActorName.ToString(), PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders);
	PartitionActor->SetActorLabel(ActorName.ToString());
#endif

	const FVector CellCenter(FVector(GridCoords.X + 0.5, GridCoords.Y + 0.5, GridCoords.Z + 0.5) * GridSize);
	if (!PartitionActor->Teleport(CellCenter))
	{
		UE_LOG(LogPCG, Error, TEXT("[RUNTIMEGEN] Could not set the location of RuntimeGen partition actor '%s'."), *PartitionActor->GetActorNameOrLabel());
	}

#if WITH_EDITOR
	PartitionActor->SetLockLocation(true);
#endif

	// Empty GUID, RuntimeGen PAs don't need one.
	PartitionActor->PostCreation(FGuid(), GridSize);

	return PartitionActor;
}

void FPCGRuntimeGenScheduler::AddPartitionActorPoolCount(int32 Count)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRuntimeGenScheduler::AddPartitionActorPoolCount);

	PartitionActorPoolSize += Count;

	FActorSpawnParameters SpawnParams;
#if WITH_EDITOR
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.Name = *PCGRuntimeGenSchedulerConstants::PooledPartitionActorName;
#endif
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.ObjectFlags &= ~RF_Transactional;

	// Create RuntimeGen PA pool.
	for (int32 I = 0; I < Count; ++I)
	{
		// TODO: do these actors get networked automatically? do we want that or not?
		APCGPartitionActor* NewActor = World->SpawnActor<APCGPartitionActor>(SpawnParams);
		check(NewActor);
		NewActor->SetToRuntimeGenerated();
		PartitionActorPool.Add(NewActor);
#if WITH_EDITOR
		NewActor->SetActorLabel(PCGRuntimeGenSchedulerConstants::PooledPartitionActorName);
#endif
	}
}

void FPCGRuntimeGenScheduler::ResetPartitionActorPoolToSize(uint32 NewPoolSize)
{
	for (APCGPartitionActor* PartitionActor : PartitionActorPool)
	{
#if WITH_EDITOR
		PartitionActor->Rename(nullptr, PartitionActor->GetOuter(), REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders);
#endif
		World->DestroyActor(PartitionActor);
	}

	PartitionActorPool.Empty();
	PartitionActorPoolSize = 0;
	AddPartitionActorPoolCount(NewPoolSize);
}

void FPCGRuntimeGenScheduler::CreateGridGuidsForComponent(UPCGComponent* InComponent)
{
	if (InComponent && InComponent->IsPartitioned() && InComponent->IsManagedByRuntimeGenSystem())
	{
		if (APCGWorldActor* PCGWorldActor = PCGHelpers::GetPCGWorldActor(World))
		{
			bool bHasUnbounded;
			PCGHiGenGrid::FSizeArray GridSizes;
			ensure(PCGHelpers::GetGenerationGridSizes(InComponent->GetGraph(), PCGWorldActor, GridSizes, bHasUnbounded));

			PCGWorldActor->CreateGridGuidsIfNecessary(GridSizes, /*bAreGridsSerialized=*/false);
		}
	}
}
