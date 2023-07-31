// Copyright Epic Games, Inc. All Rights Reserved.UMassSimulationSettings

#include "MassReplicationProcessor.h"
#include "MassClientBubbleHandler.h"
#include "MassLODSubsystem.h"
#include "MassCommonFragments.h"

namespace UE::Mass::Replication
{
	int32 DebugClientReplicationLOD = -1;
	FAutoConsoleVariableRef CVarDebugReplicationViewerLOD(TEXT("ai.debug.ClientReplicationLOD"), DebugClientReplicationLOD, TEXT("Debug Replication LOD of the specified client index"), ECVF_Cheat);
} // UE::Mass::Crowd

//----------------------------------------------------------------------//
//  UMassReplicationProcessor
//----------------------------------------------------------------------//
UMassReplicationProcessor::UMassReplicationProcessor()
	: SyncClientData(*this)
	, CollectViewerInfoQuery(*this)
	, CalculateLODQuery(*this)
	, AdjustLODDistancesQuery(*this)
	, EntityQuery(*this)
{
#if !UE_ALLOW_DEBUG_REPLICATION_BUBBLES_STANDALONE
	ExecutionFlags = int32(EProcessorExecutionFlags::Server);
#else
	ExecutionFlags = int32(EProcessorExecutionFlags::All);
#endif // UE_ALLOW_DEBUG_REPLICATION_BUBBLES_STANDALONE

	ProcessingPhase = EMassProcessingPhase::PostPhysics;

	// Processor might need to create UObjects when synchronizing clients and viewers
	// (e.g. SpawnActor from UMassReplicationSubsystem::SynchronizeClientsAndViewers())
	bRequiresGameThreadExecution = true;
}

void UMassReplicationProcessor::ConfigureQueries()
{
	SyncClientData.AddRequirement<FMassReplicationLODFragment>(EMassFragmentAccess::ReadWrite);
	SyncClientData.AddRequirement<FMassReplicatedAgentFragment>(EMassFragmentAccess::ReadWrite);

	CollectViewerInfoQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	CollectViewerInfoQuery.AddRequirement<FMassReplicationViewerInfoFragment>(EMassFragmentAccess::ReadOnly);
	CollectViewerInfoQuery.AddSharedRequirement<FMassReplicationSharedFragment>(EMassFragmentAccess::ReadWrite);

	CalculateLODQuery.AddRequirement<FMassReplicationViewerInfoFragment>(EMassFragmentAccess::ReadOnly);
	CalculateLODQuery.AddRequirement<FMassReplicationLODFragment>(EMassFragmentAccess::ReadWrite);
	CalculateLODQuery.AddConstSharedRequirement<FMassReplicationParameters>();
	CalculateLODQuery.AddSharedRequirement<FMassReplicationSharedFragment>(EMassFragmentAccess::ReadWrite);

	AdjustLODDistancesQuery.AddRequirement<FMassReplicationViewerInfoFragment>(EMassFragmentAccess::ReadOnly);
	AdjustLODDistancesQuery.AddRequirement<FMassReplicationLODFragment>(EMassFragmentAccess::ReadWrite);
	AdjustLODDistancesQuery.AddSharedRequirement<FMassReplicationSharedFragment>(EMassFragmentAccess::ReadWrite);
	AdjustLODDistancesQuery.SetChunkFilter([](const FMassExecutionContext& Context)
	{
		const FMassReplicationSharedFragment& LODSharedFragment = Context.GetSharedFragment<FMassReplicationSharedFragment>();
		return LODSharedFragment.bHasAdjustedDistancesFromCount;
	});

	EntityQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FReplicationTemplateIDFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassReplicationLODFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassReplicatedAgentFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassReplicationParameters>();
	EntityQuery.AddSharedRequirement<FMassReplicationSharedFragment>(EMassFragmentAccess::ReadWrite);

	ProcessorRequirements.AddSubsystemRequirement<UMassLODSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UMassReplicationProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

#if UE_REPLICATION_COMPILE_SERVER_CODE
	UWorld* World = Owner.GetWorld();
	ReplicationSubsystem = UWorld::GetSubsystem<UMassReplicationSubsystem>(World);

	check(ReplicationSubsystem);
#endif //UE_REPLICATION_COMPILE_SERVER_CODE
}

void UMassReplicationProcessor::PrepareExecution(FMassEntityManager& EntityManager)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE

	check(ReplicationSubsystem);

	//first synchronize clients and viewers
	ReplicationSubsystem->SynchronizeClientsAndViewers();

	EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>([this](FMassReplicationSharedFragment& RepSharedFragment)
	{
		if (!RepSharedFragment.bEntityQueryInitialized)
		{
			RepSharedFragment.EntityQuery = EntityQuery;
			RepSharedFragment.EntityQuery.SetChunkFilter([&RepSharedFragment](const FMassExecutionContext& Context)
			{
				const FMassReplicationSharedFragment& CurRepSharedFragment = Context.GetSharedFragment<FMassReplicationSharedFragment>();
				return &CurRepSharedFragment == &RepSharedFragment;
			});
			RepSharedFragment.CachedReplicator->AddRequirements(RepSharedFragment.EntityQuery);
			RepSharedFragment.bEntityQueryInitialized = true;
		}

		const TArray<FMassClientHandle>& CurrentClientHandles = ReplicationSubsystem->GetClientReplicationHandles();
		const int32 MinNumHandles = FMath::Min(RepSharedFragment.CachedClientHandles.Num(), CurrentClientHandles.Num()); // Why is this the min not the max?

		//check to see if we don't have enough cached client handles
		if (RepSharedFragment.CachedClientHandles.Num() < CurrentClientHandles.Num())
		{
			RepSharedFragment.CachedClientHandles.Reserve(CurrentClientHandles.Num());
			RepSharedFragment.BubbleInfos.Reserve(CurrentClientHandles.Num());

			for (int32 Idx = RepSharedFragment.CachedClientHandles.Num(); Idx < CurrentClientHandles.Num(); ++Idx)
			{
				const FMassClientHandle& CurrentClientHandle = CurrentClientHandles[Idx];

				RepSharedFragment.CachedClientHandles.Add(CurrentClientHandle);
				AMassClientBubbleInfoBase* Info = CurrentClientHandle.IsValid() ?
					ReplicationSubsystem->GetClientBubbleChecked(RepSharedFragment.BubbleInfoClassHandle, CurrentClientHandle) :
					nullptr;

				check(Info);

				RepSharedFragment.BubbleInfos.Add(Info);
			}
		}
		//check to see if we have too many cached client handles
		else if (RepSharedFragment.CachedClientHandles.Num() > CurrentClientHandles.Num())
		{
			const int32 NumRemove = RepSharedFragment.CachedClientHandles.Num() - CurrentClientHandles.Num();

			RepSharedFragment.CachedClientHandles.RemoveAt(CurrentClientHandles.Num(), NumRemove, /* bAllowShrinking */ false);
			RepSharedFragment.BubbleInfos.RemoveAt(CurrentClientHandles.Num(), NumRemove, /* bAllowShrinking */ false);
		}

		//check to see if any cached client handles have changed, if they have set the BubbleInfo[] appropriately
		for (int32 Idx = 0; Idx < MinNumHandles; ++Idx)
		{
			const FMassClientHandle& CurrentClientHandle = CurrentClientHandles[Idx];
			FMassClientHandle& CachedClientHandle = RepSharedFragment.CachedClientHandles[Idx];

			const bool bChanged = (CurrentClientHandle != CachedClientHandle);
			if (bChanged)
			{
				AMassClientBubbleInfoBase* Info = CurrentClientHandle.IsValid() ?
					ReplicationSubsystem->GetClientBubbleChecked(RepSharedFragment.BubbleInfoClassHandle, CurrentClientHandle) :
					nullptr;

				RepSharedFragment.BubbleInfos[Idx] = Info;
				CachedClientHandle = CurrentClientHandle;
			}
		}
	});

#endif //UE_REPLICATION_COMPILE_SERVER_CODE
}

void UMassReplicationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
	UWorld* World = EntityManager.GetWorld();
	check(World);
	check(ReplicationSubsystem);

	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_Preperation);
		PrepareExecution(EntityManager);
	}

	const UMassLODSubsystem& LODSubsystem = Context.GetSubsystemChecked<UMassLODSubsystem>(EntityManager.GetWorld());
	const TArray<FViewerInfo>& AllViewersInfo = LODSubsystem.GetViewers();
	const TArray<FMassClientHandle>& ClientHandles = ReplicationSubsystem->GetClientReplicationHandles();
	for (const FMassClientHandle ClientHandle : ClientHandles)
	{
		if (ReplicationSubsystem->IsValidClientHandle(ClientHandle) == false)
		{
			continue;
		}

		FMassClientReplicationInfo& ClientReplicationInfo = ReplicationSubsystem->GetMutableClientReplicationInfoChecked(ClientHandle);

		// Figure out all viewer of this client
		TArray<FViewerInfo> Viewers;
		for (const FMassViewerHandle ClientViewerHandle : ClientReplicationInfo.Handles)
		{
			const FViewerInfo* ViewerInfo = AllViewersInfo.FindByPredicate([ClientViewerHandle](const FViewerInfo& ViewerInfo) { return ClientViewerHandle == ViewerInfo.Handle; });
			if (ensureMsgf(ViewerInfo, TEXT("Expecting to find the client viewer handle in the all viewers info list")))
			{
				Viewers.Add(*ViewerInfo);
			}
		}

		// Prepare LOD collector and calculator
		// Remember the max LOD distance from each
		float MaxLODDistance = 0.0f;
		EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>([&Viewers,&MaxLODDistance](FMassReplicationSharedFragment& RepSharedFragment)
		{
			RepSharedFragment.LODCollector.PrepareExecution(Viewers);
			RepSharedFragment.LODCalculator.PrepareExecution(Viewers);
			MaxLODDistance = FMath::Max(MaxLODDistance, RepSharedFragment.LODCalculator.GetMaxLODDistance());
		});

		// Fetch all entities to process
		const FVector HalfExtent(MaxLODDistance, MaxLODDistance, 0.0f);
		TArray<FMassEntityHandle> EntitiesInRange;
		for (const FViewerInfo& Viewer : Viewers)
		{
			FBox Bounds(Viewer.Location - HalfExtent, Viewer.Location + HalfExtent);
			ReplicationSubsystem->GetGrid().Query(Bounds, EntitiesInRange);
		}

		EntityQuery.CacheArchetypes(EntityManager);
		if (EntityQuery.GetArchetypes().Num() > 0)
		{
			// EntitySet stores array of entities per specified archetype, may contain duplicates.
			struct FEntitySet
			{
				void Reset()
				{
					Entities.Reset();
				}

				FMassArchetypeHandle Archetype;
				TArray<FMassEntityHandle> Entities;
			};
			TArray<FEntitySet> EntitySets;

			for (const FMassArchetypeHandle& Archetype : EntityQuery.GetArchetypes())
			{
				FEntitySet& Set = EntitySets.AddDefaulted_GetRef();
				Set.Archetype = Archetype;
			}

			auto BuildEntitySet = [&EntitySets, &EntityManager](const TArray<FMassEntityHandle>& Entities)
			{
				FEntitySet* PrevSet = Entities.Num() ? &EntitySets[0] : nullptr;
				for (const FMassEntityHandle Entity : Entities)
				{
					// Add to set of supported archetypes. Dont process if we don't care about the type.
					const FMassArchetypeHandle Archetype = EntityManager.GetArchetypeForEntity(Entity);
					FEntitySet* Set = PrevSet && PrevSet->Archetype == Archetype ? PrevSet : EntitySets.FindByPredicate([&Archetype](const FEntitySet& Set) { return Archetype == Set.Archetype; });
					if (Set != nullptr)
					{
						// We don't care about duplicates here, the FMassArchetypeEntityCollection creation below will handle it
						Set->Entities.Add(Entity);
						PrevSet = Set;
					}
				}
			};

			BuildEntitySet(ClientReplicationInfo.HandledEntities);
			BuildEntitySet(EntitiesInRange);

			for (FEntitySet& Set : EntitySets)
			{
				if (Set.Entities.Num() == 0)
				{
					continue;
				}

				Context.SetEntityCollection(FMassArchetypeEntityCollection(Set.Archetype, Set.Entities, FMassArchetypeEntityCollection::FoldDuplicates));

				{
					QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_SyncToMass);
					SyncClientData.ForEachEntityChunk(EntityManager, Context, [&ClientReplicationInfo](FMassExecutionContext& Context)
					{
						const TArrayView<FMassReplicationLODFragment> ViewerLODList = Context.GetMutableFragmentView<FMassReplicationLODFragment>();
						TArrayView<FMassReplicatedAgentFragment> ReplicatedAgentList = Context.GetMutableFragmentView<FMassReplicatedAgentFragment>();

						const int32 NumEntities = Context.GetNumEntities();
						for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
						{
							FMassEntityHandle EntityHandle = Context.GetEntity(EntityIdx);
							FMassReplicatedAgentFragment& AgentFragment = ReplicatedAgentList[EntityIdx];
							FMassReplicationLODFragment& LODFragment = ViewerLODList[EntityIdx];

							if (FMassReplicatedAgentData* AgentData = ClientReplicationInfo.AgentsData.Find(EntityHandle))
							{
								LODFragment.LOD = AgentData->LOD;
								AgentFragment.AgentData = *AgentData;
							}
							else
							{
								LODFragment.LOD = EMassLOD::Off;
								AgentFragment.AgentData.Invalidate();
							}
						}
					});
				}

				{
					QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_LODCollection);
					CollectViewerInfoQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
					{
						const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
						const TArrayView<FMassReplicationViewerInfoFragment> ViewersInfoList = Context.GetMutableFragmentView<FMassReplicationViewerInfoFragment>();
						FMassReplicationSharedFragment& RepSharedFragment = Context.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCollector.CollectLODInfo(Context, LocationList, ViewersInfoList, ViewersInfoList);
					});
				}

				{
					QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_LODCaculation);
					CalculateLODQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
					{
						const TConstArrayView<FMassReplicationViewerInfoFragment> ViewersInfoList = Context.GetFragmentView<FMassReplicationViewerInfoFragment>();
						const TArrayView<FMassReplicationLODFragment> ViewerLODList = Context.GetMutableFragmentView<FMassReplicationLODFragment>();
						FMassReplicationSharedFragment& RepSharedFragment = Context.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCalculator.CalculateLOD(Context, ViewersInfoList, ViewerLODList, ViewersInfoList);
					});
				}
				Context.ClearEntityCollection();
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_LODAdjustDistance);
				EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>([](FMassReplicationSharedFragment& RepSharedFragment)
				{
					RepSharedFragment.bHasAdjustedDistancesFromCount = RepSharedFragment.LODCalculator.AdjustDistancesFromCount();
				});
			}

			for (FEntitySet& Set : EntitySets)
			{
				if (Set.Entities.Num() == 0)
				{
					continue;
				}
				Context.SetEntityCollection(FMassArchetypeEntityCollection(Set.Archetype, Set.Entities, FMassArchetypeEntityCollection::FoldDuplicates));

				{
					QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_LODAdjustLODFromCount);
					AdjustLODDistancesQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
					{
						const TConstArrayView<FMassReplicationViewerInfoFragment> ViewersInfoList = Context.GetFragmentView<FMassReplicationViewerInfoFragment>();
						const TArrayView<FMassReplicationLODFragment> ViewerLODList = Context.GetMutableFragmentView<FMassReplicationLODFragment>();
						FMassReplicationSharedFragment& RepSharedFragment = Context.GetMutableSharedFragment<FMassReplicationSharedFragment>();
						RepSharedFragment.LODCalculator.AdjustLODFromCount(Context, ViewersInfoList, ViewerLODList, ViewersInfoList);
					});
				}

				{
					QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_ProcessClientReplication);
					FMassReplicationContext ReplicationContext(*World, LODSubsystem, *ReplicationSubsystem);
					EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>([&EntityManager, &Context, &ReplicationContext, &ClientHandle](FMassReplicationSharedFragment& RepSharedFragment)
					{
						RepSharedFragment.CurrentClientHandle = ClientHandle;

						RepSharedFragment.EntityQuery.ForEachEntityChunk(EntityManager, Context, [&ReplicationContext, &RepSharedFragment](FMassExecutionContext& Context)
						{
							RepSharedFragment.CachedReplicator->ProcessClientReplication(Context, ReplicationContext);
						});
					});
				}

				{
					QUICK_SCOPE_CYCLE_COUNTER(UMassReplicationProcessor_SyncFromMass);
					SyncClientData.ForEachEntityChunk(EntityManager, Context, [&ClientReplicationInfo](FMassExecutionContext& Context)
					{
						TArrayView<FMassReplicatedAgentFragment> ReplicatedAgentList = Context.GetMutableFragmentView<FMassReplicatedAgentFragment>();

						const int32 NumEntities = Context.GetNumEntities();
						for (int EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
						{
							FMassEntityHandle EntityHandle = Context.GetEntity(EntityIdx);
							FMassReplicatedAgentFragment& AgentFragment = ReplicatedAgentList[EntityIdx];
							ClientReplicationInfo.AgentsData.Add(EntityHandle, AgentFragment.AgentData);
						}
					});
				}

				// Optional debug display
				if (UE::Mass::Replication::DebugClientReplicationLOD == ClientHandle.GetIndex())
				{
					EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>([World, &EntityManager, &Context](FMassReplicationSharedFragment& RepSharedFragment)
					{
						RepSharedFragment.EntityQuery.ForEachEntityChunk(EntityManager, Context, [World, &RepSharedFragment](FMassExecutionContext& Context)
						{
							const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
							const TConstArrayView<FMassReplicationLODFragment> ViewerLODList = Context.GetFragmentView<FMassReplicationLODFragment>();
							RepSharedFragment.LODCalculator.DebugDisplayLOD(Context, ViewerLODList, TransformList, World);
						});
					});
				}

				Context.ClearEntityCollection();
			}
		}
		ClientReplicationInfo.HandledEntities = MoveTemp(EntitiesInRange);

		// Cleanup any AgentData that isn't relevant anymore (that is EMassLOD::OFF)
		for (FMassReplicationAgentDataMap::TIterator It = ClientReplicationInfo.AgentsData.CreateIterator(); It; ++It)
		{
			FMassReplicatedAgentData& AgentData = It.Value();
			if (AgentData.LOD == EMassLOD::Off)
			{
				checkf(!AgentData.Handle.IsValid(), TEXT("This replicated agent should have been removed from this client and was not"));
				It.RemoveCurrent();
			}
		}
	}
#endif //UE_REPLICATION_COMPILE_SERVER_CODE
}
