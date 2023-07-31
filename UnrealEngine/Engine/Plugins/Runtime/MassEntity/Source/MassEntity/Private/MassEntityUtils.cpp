// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityUtils.h"
#include "MassEntityTypes.h"
#include "MassArchetypeTypes.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"

namespace UE::Mass::Utils
{

EProcessorExecutionFlags GetProcessorExecutionFlagsForWold(const UWorld& World)
{
	EProcessorExecutionFlags ExecutionFlags = EProcessorExecutionFlags::None;
	const ENetMode NetMode = World.GetNetMode();
	switch (NetMode)
	{
		case NM_ListenServer:
			ExecutionFlags = EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Server;
			break;
		case NM_DedicatedServer:
			ExecutionFlags = EProcessorExecutionFlags::Server;
			break;
		case NM_Client:
			ExecutionFlags = EProcessorExecutionFlags::Client;
			break;
		default:
			check(NetMode == NM_Standalone);
			ExecutionFlags = EProcessorExecutionFlags::Standalone;
			break;
	}

	return ExecutionFlags;
}

void CreateEntityCollections(const FMassEntityManager& EntityManager, const TConstArrayView<FMassEntityHandle> Entities
	, const FMassArchetypeEntityCollection::EDuplicatesHandling DuplicatesHandling, TArray<FMassArchetypeEntityCollection>& OutEntityCollections)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("Mass_CreateSparseChunks");

	TMap<const FMassArchetypeHandle, TArray<FMassEntityHandle>> ArchetypeToEntities;

	for (const FMassEntityHandle& Entity : Entities)
	{
		if (EntityManager.IsEntityValid(Entity))
		{
			FMassArchetypeHandle Archetype = EntityManager.GetArchetypeForEntityUnsafe(Entity);
			TArray<FMassEntityHandle>& PerArchetypeEntities = ArchetypeToEntities.FindOrAdd(Archetype);
			PerArchetypeEntities.Add(Entity);
		}
	}

	for (auto& Pair : ArchetypeToEntities)
	{
		OutEntityCollections.Add(FMassArchetypeEntityCollection(Pair.Key, Pair.Value, DuplicatesHandling));
	}
}

FMassEntityManager* GetEntityManager(const UWorld* World)
{
	UMassEntitySubsystem* EntityManager = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	check(EntityManager);
	return &EntityManager->GetMutableEntityManager();
}

FMassEntityManager& GetEntityManagerChecked(const UWorld& World)
{
	UMassEntitySubsystem* EntityManager = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntityManager);
	return EntityManager->GetMutableEntityManager();
}

} // namespace UE::Mass::Utils