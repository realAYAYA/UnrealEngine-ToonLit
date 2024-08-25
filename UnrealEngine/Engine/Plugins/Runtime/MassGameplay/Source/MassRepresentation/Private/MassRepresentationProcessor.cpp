// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassRepresentationProcessor.h"
#include "MassRepresentationDebug.h"
#include "MassRepresentationSubsystem.h"
#include "MassRepresentationUtils.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "Engine/World.h"
#include "MassRepresentationActorManagement.h"
#include "MassCommonUtils.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"


DECLARE_CYCLE_STAT(TEXT("Mass Visualization Execute"), STAT_Mass_VisProcessor_Execute, STATGROUP_Mass);

namespace UE::Mass::Representation
{
	int32 bAllowKeepActorExtraFrame = 1;
	FAutoConsoleVariableRef CVarAllowKeepActorExtraFrame(TEXT("ai.massrepresentation.AllowKeepActorExtraFrame"), bAllowKeepActorExtraFrame, TEXT("Allow the mass representation to keep actor an extra frame when switching to ISM"), ECVF_Default);
}

//----------------------------------------------------------------------//
// UMassRepresentationProcessor(); 
//----------------------------------------------------------------------//
UMassRepresentationProcessor::UMassRepresentationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
}

void UMassRepresentationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassRepresentationParameters>();
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassActorSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassRepresentationProcessor::UpdateRepresentation(FMassExecutionContext& Context, const FMassRepresentationUpdateParams& Params)
{
	UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
	check(RepresentationSubsystem);
	const FMassRepresentationParameters& RepresentationParams = Context.GetConstSharedFragment<FMassRepresentationParameters>();
	UMassRepresentationActorManagement* RepresentationActorManagement = RepresentationParams.CachedRepresentationActorManagement;
	check(RepresentationActorManagement);

	UMassActorSubsystem* MassActorSubsystem = Context.GetMutableSubsystem<UMassActorSubsystem>();

	FMassEntityManager& CachedEntityManager = Context.GetEntityManagerChecked();

	const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
	const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
	const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
	const TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();

	const bool bDoKeepActorExtraFrame = UE::Mass::Representation::bAllowKeepActorExtraFrame ? RepresentationParams.bKeepLowResActors : false;

	const int32 NumEntities = Context.GetNumEntities();
	for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	{
		const FMassEntityHandle MassAgentEntityHandle = Context.GetEntity(EntityIdx);
		const FTransformFragment& TransformFragment = TransformList[EntityIdx];
		const FMassRepresentationLODFragment& RepresentationLOD = RepresentationLODList[EntityIdx];
		FMassRepresentationFragment& Representation = RepresentationList[EntityIdx];
		FMassActorFragment& ActorInfo = ActorList[EntityIdx];
		AActor* Actor = ActorInfo.GetMutable();

		// Keeping a copy of the that last calculated previous representation
		const EMassRepresentationType PrevRepresentationCopy = Representation.PrevRepresentation;
		Representation.PrevRepresentation = Representation.CurrentRepresentation;
		
		EMassRepresentationType WantedRepresentationType = RepresentationParams.LODRepresentation[FMath::Min((int32)RepresentationLOD.LOD, (int32)EMassLOD::Off)];

		// Make sure we do not have actor spawned in areas not fully loaded
		if (Params.bTestCollisionAvailibilityForActorVisualization
			&& (WantedRepresentationType == EMassRepresentationType::HighResSpawnedActor || WantedRepresentationType == EMassRepresentationType::LowResSpawnedActor) 
			&& !RepresentationSubsystem->IsCollisionLoaded(RepresentationParams.WorldPartitionGridNameContainingCollision, TransformFragment.GetTransform()))
		{
			WantedRepresentationType = RepresentationParams.CachedDefaultRepresentationType;
		}

		// If bForceActorRepresentationForExternalActors is enabled and we have an Actor reference for this entity, forcibly use it
		// by enforcing an actor representation as the WantedRepresentation. If we're coming from ISMC, we'll remove the instance
		// and switch to this actor, commiting either LowResSpawnedActor or HighResSpawnedActor as the new CurrentRepresentation.
		// Once the Actor is destroyed however, this override stops, allowing the natural WantedRepresentationType to return.
		//
		// Useful for server-authoritative Actor spawning, with replicated Actors inserting themselves into Mass whilst they're
		// replicated, enforcing actor representation on clients whilst they're present.
		if (RepresentationParams.bForceActorRepresentationForExternalActors && IsValid(Actor) && !ActorInfo.IsOwnedByMass())
		{
			WantedRepresentationType = Representation.CurrentRepresentation == EMassRepresentationType::LowResSpawnedActor ? EMassRepresentationType::LowResSpawnedActor : EMassRepresentationType::HighResSpawnedActor;
		}

		// Has Actor unexpectedly been unset / destroyed since we last ran? 
		if (!IsValid(Actor) && (Representation.CurrentRepresentation == EMassRepresentationType::LowResSpawnedActor || Representation.CurrentRepresentation == EMassRepresentationType::HighResSpawnedActor))
		{
			// Set CurrentRepresentation = None so we get a chance to see CurrentRepresentation != WantedRepresentationType and spawn 
			// another actor.
			Representation.CurrentRepresentation = EMassRepresentationType::None;
		}
		
		auto DisableActorForISM = [&](AActor*& Actor)
		{
			if (!Actor || ActorInfo.IsOwnedByMass())
			{
				// Execute only if the high res is different than the low res Actor 
				// Or if we do not wish to keep the low res actor while in TransformList
				if (Representation.HighResTemplateActorIndex != Representation.LowResTemplateActorIndex || !RepresentationParams.bKeepLowResActors)
				{
					// Try releasing the high actor or any high res spawning request
					if (ReleaseActorOrCancelSpawning(*RepresentationSubsystem, MassActorSubsystem, MassAgentEntityHandle, ActorInfo, Representation.HighResTemplateActorIndex, Representation.ActorSpawnRequestHandle, Context.Defer()))
					{
						Actor = ActorInfo.GetOwnedByMassMutable();
					}
					// Do not do the same with low res if indicated so
					if (!RepresentationParams.bKeepLowResActors && ReleaseActorOrCancelSpawning(*RepresentationSubsystem, MassActorSubsystem, MassAgentEntityHandle, ActorInfo, Representation.LowResTemplateActorIndex, Representation.ActorSpawnRequestHandle, Context.Defer()))
					{
						Actor = ActorInfo.GetOwnedByMassMutable();
					}
				}
				// If we already queued spawn request but have changed our mind, continue with it but once we get the actor back, disable it immediately
				if (Representation.ActorSpawnRequestHandle.IsValid())
				{
					Actor = RepresentationActorManagement->GetOrSpawnActor(*RepresentationSubsystem, CachedEntityManager, MassAgentEntityHandle, TransformFragment.GetTransform(), Representation.LowResTemplateActorIndex, Representation.ActorSpawnRequestHandle, RepresentationActorManagement->GetSpawnPriority(RepresentationLOD));
				}
			}
			if (Actor != nullptr)
			{
				RepresentationActorManagement->SetActorEnabled(EMassActorEnabledType::Disabled, *Actor, EntityIdx, Context.Defer());
			}
		};

		// Process switch between representation if there is a change in the representation or there is a pending spawning request
		if (WantedRepresentationType != Representation.CurrentRepresentation || Representation.ActorSpawnRequestHandle.IsValid())
		{
			if (Representation.CurrentRepresentation == EMassRepresentationType::None)
			{
				Representation.PrevTransform = TransformFragment.GetTransform();
				Representation.PrevLODSignificance = RepresentationLOD.LODSignificance;
			}

			switch (WantedRepresentationType)
			{
				case EMassRepresentationType::HighResSpawnedActor:
				case EMassRepresentationType::LowResSpawnedActor:
				{
					const bool bHighResActor = WantedRepresentationType == EMassRepresentationType::HighResSpawnedActor;

					// Reuse actor, if it is valid and not owned by mass or same representation as low res without a valid spawning request
					AActor* NewActor = nullptr;
					if (!Actor || ActorInfo.IsOwnedByMass())
					{
						const int16 WantedTemplateActorIndex = bHighResActor ? Representation.HighResTemplateActorIndex : Representation.LowResTemplateActorIndex;

						// If the low res is different than the high res, cancel any pending spawn request that is the opposite of what is needed.
						if (Representation.LowResTemplateActorIndex != Representation.HighResTemplateActorIndex)
						{
							ReleaseActorOrCancelSpawning(*RepresentationSubsystem, MassActorSubsystem, MassAgentEntityHandle, ActorInfo, bHighResActor ? Representation.LowResTemplateActorIndex : Representation.HighResTemplateActorIndex, Representation.ActorSpawnRequestHandle, Context.Defer(), /*bCancelSpawningOnly*/true);
							Actor = ActorInfo.GetOwnedByMassMutable();
						}

						// If there isn't any actor yet or
						// If the actor isn't matching the one needed or
						// If there is still a pending spawn request
						// Then try to retrieve/spawn the new actor
						if (!Actor ||
							!RepresentationSubsystem->DoesActorMatchTemplate(*Actor, WantedTemplateActorIndex) ||
							Representation.ActorSpawnRequestHandle.IsValid())
						{
							NewActor = RepresentationActorManagement->GetOrSpawnActor(*RepresentationSubsystem, CachedEntityManager, MassAgentEntityHandle, TransformFragment.GetTransform(), WantedTemplateActorIndex, Representation.ActorSpawnRequestHandle, RepresentationActorManagement->GetSpawnPriority(RepresentationLOD));
						}
						else
						{
							NewActor = Actor;
						}
					}
					else
					{
						NewActor = Actor;
					}

					if (NewActor)
					{
						// Make sure our (re)activated actor is at the simulated position
						// Needs to be done before enabling the actor so the animation initialization can use the new values
						if (Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
						{
							RepresentationActorManagement->TeleportActor(Representation.PrevTransform, *NewActor, Context.Defer());
						}

						RepresentationActorManagement->SetActorEnabled(bHighResActor ? EMassActorEnabledType::HighRes : EMassActorEnabledType::LowRes, *NewActor, EntityIdx, Context.Defer());
						Representation.CurrentRepresentation = WantedRepresentationType;
					}
					else if (!Actor)
					{
						Representation.CurrentRepresentation = RepresentationParams.CachedDefaultRepresentationType;
					}
					break;
				}
				case EMassRepresentationType::StaticMeshInstance:
					if (!bDoKeepActorExtraFrame || 
					   (Representation.PrevRepresentation != EMassRepresentationType::HighResSpawnedActor && Representation.PrevRepresentation != EMassRepresentationType::LowResSpawnedActor))
					{
						DisableActorForISM(Actor);
					}
 
					Representation.CurrentRepresentation = EMassRepresentationType::StaticMeshInstance;
					break;
				case EMassRepresentationType::None:
					if (!Actor || ActorInfo.IsOwnedByMass())
					{
						// Try releasing both, could have an high res spawned actor and a spawning request for a low res one
						ReleaseActorOrCancelSpawning(*RepresentationSubsystem, MassActorSubsystem, MassAgentEntityHandle, ActorInfo, Representation.LowResTemplateActorIndex, Representation.ActorSpawnRequestHandle, Context.Defer());
						ReleaseActorOrCancelSpawning(*RepresentationSubsystem, MassActorSubsystem, MassAgentEntityHandle, ActorInfo, Representation.HighResTemplateActorIndex, Representation.ActorSpawnRequestHandle, Context.Defer());
					}
					else
					{
						RepresentationActorManagement->SetActorEnabled(EMassActorEnabledType::Disabled, *Actor, EntityIdx, Context.Defer());
					}
					Representation.CurrentRepresentation = EMassRepresentationType::None;
					break;
				default:
					checkf(false, TEXT("Unsupported LOD type"));
					break;
			}
		}
		else if (bDoKeepActorExtraFrame && 
				 Representation.PrevRepresentation == EMassRepresentationType::StaticMeshInstance &&
			    (PrevRepresentationCopy == EMassRepresentationType::HighResSpawnedActor || PrevRepresentationCopy == EMassRepresentationType::LowResSpawnedActor))
		{
			DisableActorForISM(Actor);
		}
	}

#if WITH_MASSGAMEPLAY_DEBUG
	// Optional debug display
	if (UE::Mass::Representation::Debug::DebugRepresentation == 1 || UE::Mass::Representation::Debug::DebugRepresentation >= 3)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DebugDisplayRepresentation)
		UWorld* World = CachedEntityManager.GetWorld();
		UE::Mass::Representation::Debug::DebugDisplayRepresentation(Context, RepresentationLODList, RepresentationList, TransformList, World);
	}
	// Optional vislog
	if (UE::Mass::Representation::Debug::DebugRepresentation >= 2)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(VisLogRepresentation)
		UE::Mass::Representation::Debug::VisLogRepresentation(Context, RepresentationLODList, RepresentationList, TransformList, RepresentationSubsystem);
	}
#endif
}

void UMassRepresentationProcessor::Execute(FMassEntityManager& InEntityManager, FMassExecutionContext& Context)
{
	// Update entities representation
	EntityQuery.ForEachEntityChunk(InEntityManager, Context, [this](FMassExecutionContext& Context)
	{
		UpdateRepresentation(Context, UpdateParams);
	});
}

bool UMassRepresentationProcessor::ReleaseActorOrCancelSpawning(UMassRepresentationSubsystem& RepresentationSubsystem, UMassActorSubsystem* MassActorSubsystem
	, const FMassEntityHandle MassAgent, FMassActorFragment& ActorInfo, const int16 TemplateActorIndex, FMassActorSpawnRequestHandle& SpawnRequestHandle
	, FMassCommandBuffer& CommandBuffer, bool bCancelSpawningOnly /*= false*/)
{
	if (TemplateActorIndex == INDEX_NONE)
	{
		// Nothing to release
		return false;
	}
	check(!ActorInfo.IsValid() || ActorInfo.IsOwnedByMass());

	AActor* Actor = ActorInfo.GetOwnedByMassMutable();
	// note that it's fine for Actor to be null. That means the RepresentationSubsystem will try to stop 
	// the spawning of whatever SpawnRequestHandle reference to
	const bool bSuccess = bCancelSpawningOnly ? RepresentationSubsystem.CancelSpawning(MassAgent, TemplateActorIndex, SpawnRequestHandle) :
			RepresentationSubsystem.ReleaseTemplateActorOrCancelSpawning(MassAgent, TemplateActorIndex, Actor, SpawnRequestHandle);
	if (bSuccess)
	{
		Actor = ActorInfo.GetOwnedByMassMutable();
		if (Actor && RepresentationSubsystem.DoesActorMatchTemplate(*Actor, TemplateActorIndex))
		{
			ActorInfo.ResetNoHandleMapUpdate();
			
			TObjectKey<const AActor> ActorKey(Actor); 
			if (MassActorSubsystem)
			{
				CommandBuffer.PushCommand<FMassDeferredSetCommand>([MassActorSubsystem, ActorKey](FMassEntityManager&)
				{
					MassActorSubsystem->RemoveHandleForActor(ActorKey);
				});
			}
		}
		return true;
	}
	return false;
}

//----------------------------------------------------------------------//
// UMassVisualizationProcessor 
//----------------------------------------------------------------------//
void UMassVisualizationProcessor::UpdateVisualization(FMassExecutionContext& Context)
{
	FMassVisualizationChunkFragment& ChunkData = UpdateChunkVisibility(Context);
	if (!ChunkData.ShouldUpdateVisualization())
	{
		return;
	}

	UpdateRepresentation(Context, UpdateParams);

	// Update entity visibility
	const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
	const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();

	const int32 NumEntities = Context.GetNumEntities();
	for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	{
		const FMassEntityHandle Entity = Context.GetEntity(EntityIdx);
		FMassRepresentationFragment& Representation = RepresentationList[EntityIdx];
		const FMassRepresentationLODFragment& RepresentationLOD = RepresentationLODList[EntityIdx];
		UpdateEntityVisibility(Entity, Representation, RepresentationLOD, ChunkData, Context.Defer());
	}
}

void UMassVisualizationProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();
	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassVisualizationProcessorTag>(EMassFragmentPresence::All);
}

FMassVisualizationChunkFragment& UMassVisualizationProcessor::UpdateChunkVisibility(FMassExecutionContext& Context) const
{
	const FMassRepresentationParameters& RepresentationParams = Context.GetConstSharedFragment<FMassRepresentationParameters>();
	bool bFirstUpdate = false;

	// Setup chunk fragment data about visibility
	FMassVisualizationChunkFragment& ChunkData = Context.GetMutableChunkFragment<FMassVisualizationChunkFragment>();
	EMassVisibility ChunkVisibility = ChunkData.GetVisibility();
	if (ChunkVisibility == EMassVisibility::Max)
	{
		// The visibility on the chunk fragment data isn't set yet, let see if the Archetype has an visibility tag and set it on the ChunkData
		ChunkVisibility = UE::Mass::Representation::GetVisibilityFromArchetype(Context);
		ChunkData.SetVisibility(ChunkVisibility);
		bFirstUpdate = RepresentationParams.bSpreadFirstVisualizationUpdate;
	}
	else
	{
		checkfSlow(UE::Mass::Representation::IsVisibilityTagSet(Context, ChunkVisibility), TEXT("Expecting the same Visibility as what we saved in the chunk data, maybe external code is modifying the tags"))
	}

	if (ChunkVisibility == EMassVisibility::CulledByDistance)
	{
		float DeltaTime = ChunkData.GetDeltaTime();
		if (bFirstUpdate)
		{
			// A DeltaTime of 0.0f means it will tick this frame.
			// @todo: Add some randomization for deterministic runs too. The randomization is used to distribute the infrequent ticks evenly on different frames.
			DeltaTime = UE::Mass::Utils::IsDeterministic() ? RepresentationParams.NotVisibleUpdateRate * 0.5f : FMath::RandRange(0.0f, RepresentationParams.NotVisibleUpdateRate);
		}
		else 
		{
			if (DeltaTime < 0.0f)
			{
				// @todo: Add some randomization for deterministic runs too. The randomization is used to distribute the infrequent ticks evenly on different frames.
				DeltaTime += UE::Mass::Utils::IsDeterministic() ? RepresentationParams.NotVisibleUpdateRate : (RepresentationParams.NotVisibleUpdateRate * (1.0f + FMath::RandRange(-0.1f, 0.1f)));
			}
			DeltaTime -= Context.GetDeltaTimeSeconds();
		}

		ChunkData.Update(DeltaTime);
	}

	return ChunkData;
}

void UMassVisualizationProcessor::UpdateEntityVisibility(const FMassEntityHandle Entity, const FMassRepresentationFragment& Representation, const FMassRepresentationLODFragment& RepresentationLOD, FMassVisualizationChunkFragment& ChunkData, FMassCommandBuffer& CommandBuffer)
{
	// Move the visible entities together into same chunks so we can skip entire chunk when not visible as an optimization
	const EMassVisibility Visibility = Representation.CurrentRepresentation != EMassRepresentationType::None ? 
		EMassVisibility::CanBeSeen : RepresentationLOD.Visibility;
	const EMassVisibility ChunkVisibility = ChunkData.GetVisibility();
	if (ChunkVisibility != Visibility)
	{
		UE::Mass::Representation::PushSwapTagsCommand(CommandBuffer, Entity, ChunkVisibility, Visibility);
		ChunkData.SetContainsNewlyVisibleEntity(Visibility == EMassVisibility::CanBeSeen);
	}
}

void UMassVisualizationProcessor::Execute(FMassEntityManager& InEntityManager, FMassExecutionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_Mass_VisProcessor_Execute);

	int32 TotalEntitiesProcessed = 0;
	// Update entities visualization
	EntityQuery.ForEachEntityChunk(InEntityManager, Context, [this, &TotalEntitiesProcessed](FMassExecutionContext& Context)
	{
		TotalEntitiesProcessed += Context.GetNumEntities();
		UpdateVisualization(Context);
	});

	UE_VLOG(this, LogMassRepresentation, Verbose, TEXT("UMassVisualizationProcessor::Execute processed %d entities"), TotalEntitiesProcessed);
}

//----------------------------------------------------------------------//
// UMassRepresentationFragmentDestructor 
//----------------------------------------------------------------------//
UMassRepresentationFragmentDestructor::UMassRepresentationFragmentDestructor()
	: EntityQuery(*this)
{
	ObservedType = FMassRepresentationFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // due to FMassRepresentationSubsystemSharedFragment.RepresentationSubsystem use
}

void UMassRepresentationFragmentDestructor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassRepresentationParameters>();
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassActorSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassRepresentationFragmentDestructor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		UMassActorSubsystem* ActorSubsystem = Context.GetMutableSubsystem<UMassActorSubsystem>();

		const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		const TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();

		const int32 NumEntities = Context.GetNumEntities();
		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassRepresentationFragment& Representation = RepresentationList[i];
			FMassActorFragment& ActorInfo = ActorList[i];

			const FMassEntityHandle MassAgentEntityHandle = Context.GetEntity(i);

			UMassRepresentationActorManagement::ReleaseAnyActorOrCancelAnySpawning(*RepresentationSubsystem, MassAgentEntityHandle, ActorInfo, Representation, ActorSubsystem);
		}
	});
}
