// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStateTreeProcessors.h"
#include "StateTree.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassNavigationTypes.h"
#include "MassSimulationLOD.h"
#include "MassComponentHitTypes.h"
#include "MassSmartObjectTypes.h"
#include "MassZoneGraphAnnotationTypes.h"
#include "MassStateTreeSubsystem.h"
#include "MassSignalSubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Engine/World.h"
#include "MassBehaviorSettings.h"
#include "VisualLogger/VisualLogger.h"
#include "MassDebugger.h"


CSV_DEFINE_CATEGORY(StateTreeProcessor, true);

namespace UE::MassBehavior
{

template<typename TFunc>
void ForEachEntityInChunk(
	FMassExecutionContext& Context,
	FMassEntityManager& EntityManager,
	UMassSignalSubsystem& SignalSubsystem,
	UMassStateTreeSubsystem& MassStateTreeSubsystem,
	TFunc&& Callback)
{
	const TArrayView<FMassStateTreeInstanceFragment> StateTreeInstanceList = Context.GetMutableFragmentView<FMassStateTreeInstanceFragment>();
	const FMassStateTreeSharedFragment& SharedStateTree = Context.GetConstSharedFragment<FMassStateTreeSharedFragment>();

	// Assuming that all the entities share same StateTree, because they all should have the same storage fragment.
	const int32 NumEntities = Context.GetNumEntities();
	check(NumEntities > 0);
	
	const UStateTree* StateTree = SharedStateTree.StateTree;

	for (int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
	{
		const FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
		FMassStateTreeInstanceFragment& StateTreeFragment = StateTreeInstanceList[EntityIndex];
		FStateTreeInstanceData* InstanceData = MassStateTreeSubsystem.GetInstanceData(StateTreeFragment.InstanceHandle);
		if (InstanceData != nullptr)
		{
			FMassStateTreeExecutionContext StateTreeContext(MassStateTreeSubsystem, *StateTree, *InstanceData, EntityManager, SignalSubsystem, Context);
			StateTreeContext.SetEntity(Entity);

			// Make sure all required external data are set.
			{
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTreeProcessorExternalDataValidation);
				// TODO: disable this when not in debug.
				if (!ensureMsgf(StateTreeContext.AreContextDataViewsValid(), TEXT("StateTree will not execute due to missing external data.")))
				{
					break;
				}
			}

			Callback(StateTreeContext, StateTreeFragment);
		}
	}
}

} // UE::MassBehavior


//----------------------------------------------------------------------//
// UMassStateTreeFragmentDestructor
//----------------------------------------------------------------------//
UMassStateTreeFragmentDestructor::UMassStateTreeFragmentDestructor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ObservedType = FMassStateTreeInstanceFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	bRequiresGameThreadExecution = true;
}

void UMassStateTreeFragmentDestructor::Initialize(UObject& Owner)
{
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	Super::Initialize(Owner);
}

void UMassStateTreeFragmentDestructor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassStateTreeInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassStateTreeSharedFragment>();
	EntityQuery.AddSubsystemRequirement<UMassStateTreeSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassStateTreeFragmentDestructor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (SignalSubsystem == nullptr)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntityManager, Context,
		[&EntityManager, SignalSubsystem = SignalSubsystem](FMassExecutionContext& Context)
		{
			UMassStateTreeSubsystem& MassStateTreeSubsystem = Context.GetMutableSubsystemChecked<UMassStateTreeSubsystem>();
			const TArrayView<FMassStateTreeInstanceFragment> StateTreeInstanceList = Context.GetMutableFragmentView<FMassStateTreeInstanceFragment>();
			const FMassStateTreeSharedFragment& SharedStateTree = Context.GetConstSharedFragment<FMassStateTreeSharedFragment>();

			const int32 NumEntities = Context.GetNumEntities();
	
			UE::MassBehavior::ForEachEntityInChunk(Context, EntityManager, *SignalSubsystem, MassStateTreeSubsystem,
				[](FMassStateTreeExecutionContext& StateTreeExecutionContext, FMassStateTreeInstanceFragment& StateTreeFragment)
				{
					// Stop the tree instance
					StateTreeExecutionContext.Stop();
				});

			// Free the StateTree instance memory
			for (int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
			{
				FMassStateTreeInstanceFragment& StateTreeInstance = StateTreeInstanceList[EntityIndex];
				if (StateTreeInstance.InstanceHandle.IsValid())
				{
					MassStateTreeSubsystem.FreeInstanceData(StateTreeInstance.InstanceHandle);
					StateTreeInstance.InstanceHandle = FMassStateTreeInstanceHandle();
				}
			}
		});
}

//----------------------------------------------------------------------//
// UMassStateTreeActivationProcessor
//----------------------------------------------------------------------//
UMassStateTreeActivationProcessor::UMassStateTreeActivationProcessor()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Behavior);
	bRequiresGameThreadExecution = true; // due to UMassStateTreeSubsystem RW access
}

void UMassStateTreeActivationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassStateTreeInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassStateTreeSharedFragment>();
	EntityQuery.AddTagRequirement<FMassStateTreeActivatedTag>(EMassFragmentPresence::None);
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddSubsystemRequirement<UMassStateTreeSubsystem>(EMassFragmentAccess::ReadWrite);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassStateTreeActivationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	const UMassBehaviorSettings* BehaviorSettings = GetDefault<UMassBehaviorSettings>();
	check(BehaviorSettings);
 
	// StateTree processor relies on signals to be ticked but we need an 'initial tick' to set the tree in the proper state.
	// The initializer provides that by sending a signal to all new entities that use StateTree.
	const double TimeInSeconds = EntityManager.GetWorld()->GetTimeSeconds();

	TArray<FMassEntityHandle> EntitiesToSignal;
	int32 ActivationCounts[EMassLOD::Max] {0,0,0,0};
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context,
		[&EntitiesToSignal, &ActivationCounts, SignalSubsystem = &SignalSubsystem, &EntityManager, MaxActivationsPerLOD = BehaviorSettings->MaxActivationsPerLOD, TimeInSeconds](FMassExecutionContext& Context)
		{
			UMassStateTreeSubsystem& MassStateTreeSubsystem = Context.GetMutableSubsystemChecked<UMassStateTreeSubsystem>();
			const int32 NumEntities = Context.GetNumEntities();
			// Check if we already reached the maximum for this frame
			const EMassLOD::Type ChunkLOD = FMassSimulationVariableTickChunkFragment::GetChunkLOD(Context);
			if (ActivationCounts[ChunkLOD] > MaxActivationsPerLOD[ChunkLOD])
			{
				return;
			}
			ActivationCounts[ChunkLOD] += NumEntities;

			const TArrayView<FMassStateTreeInstanceFragment> StateTreeInstanceList = Context.GetMutableFragmentView<FMassStateTreeInstanceFragment>();
			const FMassStateTreeSharedFragment& SharedStateTree = Context.GetConstSharedFragment<FMassStateTreeSharedFragment>();

			// Allocate and initialize the StateTree instance memory
			for (int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
			{
				FMassStateTreeInstanceFragment& StateTreeInstance = StateTreeInstanceList[EntityIndex];
				StateTreeInstance.InstanceHandle = MassStateTreeSubsystem.AllocateInstanceData(SharedStateTree.StateTree);
			}
			
			// Start StateTree. This may do substantial amount of work, as we select and enter the first state.
			UE::MassBehavior::ForEachEntityInChunk(Context, EntityManager, *SignalSubsystem, MassStateTreeSubsystem,
				[TimeInSeconds](FMassStateTreeExecutionContext& StateTreeExecutionContext, FMassStateTreeInstanceFragment& StateTreeFragment)
				{
					// Start the tree instance
					StateTreeExecutionContext.Start();
					StateTreeFragment.LastUpdateTimeInSeconds = TimeInSeconds;
				});

			// Adding a tag on each entities to remember we have sent the state tree initialization signal
			EntitiesToSignal.Reserve(EntitiesToSignal.Num() + NumEntities);
			for (int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
			{
				const FMassStateTreeInstanceFragment& StateTreeInstance = StateTreeInstanceList[EntityIndex];
				if (StateTreeInstance.InstanceHandle.IsValid())
				{
					const FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
					Context.Defer().AddTag<FMassStateTreeActivatedTag>(Entity);
					EntitiesToSignal.Add(Entity);
				}
			}
		});
	
	// Signal all entities inside the consolidated list
	if (EntitiesToSignal.Num())
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
	}
}

//----------------------------------------------------------------------//
// UMassStateTreeProcessor
//----------------------------------------------------------------------//
UMassStateTreeProcessor::UMassStateTreeProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRequiresGameThreadExecution = true;

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;

	// `Behavior` doesn't run on clients but `Tasks` do.
	// We define the dependencies here so task won't need to set their dependency on `Behavior`,
	// but only on `SyncWorldToMass`
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Tasks);
}

void UMassStateTreeProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());

	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::StateTreeActivate);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::LookAtFinished);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::NewStateTreeTaskRequired);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::StandTaskFinished);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::DelayedTransitionWakeup);

	// @todo MassStateTree: add a way to register/unregister from enter/exit state (need reference counting)
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::SmartObjectRequestCandidates);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::SmartObjectCandidatesReady);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::SmartObjectInteractionDone);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::SmartObjectInteractionAborted);

	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::FollowPointPathStart);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::FollowPointPathDone);
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::CurrentLaneChanged);

	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::AnnotationTagsChanged);

	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::HitReceived);

	// @todo MassStateTree: move this to its game plugin when possible
	SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::ContextualAnimTaskFinished);
}

void UMassStateTreeProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassStateTreeInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassStateTreeSharedFragment>();
	EntityQuery.AddSubsystemRequirement<UMassStateTreeSubsystem>(EMassFragmentAccess::ReadWrite);

	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassStateTreeProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();
	
	QUICK_SCOPE_CYCLE_COUNTER(StateTreeProcessor_Run);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StateTreeProcessorExecute);

	const double TimeInSeconds = EntityManager.GetWorld()->GetTimeSeconds();

	TArray<FMassEntityHandle> EntitiesToSignal;

	EntityQuery.ForEachEntityChunk(EntityManager, Context,
		[this, &TimeInSeconds, &EntitiesToSignal, &EntityManager, SignalSubsystem = &SignalSubsystem, &EntitySignals](FMassExecutionContext& Context)
		{
			// Keep stats regarding the amount of tree instances ticked per frame
			CSV_CUSTOM_STAT(StateTreeProcessor, NumTickedStateTree, Context.GetNumEntities(), ECsvCustomStatOp::Accumulate);

			UMassStateTreeSubsystem& MassStateTreeSubsystem = Context.GetMutableSubsystemChecked<UMassStateTreeSubsystem>();

			UE::MassBehavior::ForEachEntityInChunk(Context, EntityManager, *SignalSubsystem, MassStateTreeSubsystem,
				[TimeInSeconds, &EntitiesToSignal, &EntitySignals, &MassStateTreeSubsystem]
				(FMassStateTreeExecutionContext& StateTreeExecutionContext, FMassStateTreeInstanceFragment& StateTreeFragment)
				{
					// Compute adjusted delta time
					const float AdjustedDeltaTime = FloatCastChecked<float>(TimeInSeconds - StateTreeFragment.LastUpdateTimeInSeconds, /* Precision */ 1./256.);
					StateTreeFragment.LastUpdateTimeInSeconds = TimeInSeconds;

#if WITH_MASSGAMEPLAY_DEBUG
					const FMassEntityHandle Entity = StateTreeExecutionContext.GetEntity();
					if (UE::Mass::Debug::IsDebuggingEntity(Entity))
					{
						TArray<FName> Signals;
						EntitySignals.GetSignalsForEntity(Entity, Signals);
						FString SignalsString;
						for (const FName& SignalName : Signals)
						{
							if (!SignalsString.IsEmpty())
							{
								SignalsString += TEXT(", ");
							}
							SignalsString += SignalName.ToString();
						}
						UE_VLOG_UELOG(&MassStateTreeSubsystem, LogStateTree, Log, TEXT("%s: Ticking StateTree because of signals: %s"), *Entity.DebugGetDescription(), *SignalsString);
					}
#endif // WITH_MASSGAMEPLAY_DEBUG

					// Tick the tree instance
					StateTreeExecutionContext.Tick(AdjustedDeltaTime);

					// When last tick status is different than "Running", the state tree need to be tick again
					// For performance reason, tick again to see if we could find a new state right away instead of waiting the next frame.
					if (StateTreeExecutionContext.GetLastTickStatus() != EStateTreeRunStatus::Running)
					{
						StateTreeExecutionContext.Tick(0.0f);

						// Could not find new state yet, try again next frame
						if (StateTreeExecutionContext.GetLastTickStatus() != EStateTreeRunStatus::Running)
						{
							EntitiesToSignal.Add(StateTreeExecutionContext.GetEntity());
						}
					}
				});
		});

	if (EntitiesToSignal.Num())
	{
		SignalSubsystem.SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignal);
	}
}
