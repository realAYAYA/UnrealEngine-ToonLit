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
EProcessorExecutionFlags GetProcessorExecutionFlagsForWorld(const UWorld& World)
{
	if (World.IsEditorWorld() && !World.IsGameWorld())
	{
		return EProcessorExecutionFlags::Editor;
	}
	else
	{
		const ENetMode NetMode = World.GetNetMode();
		switch (NetMode)
		{
		case NM_ListenServer:
			return EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Server;
		case NM_DedicatedServer:
			return EProcessorExecutionFlags::Server;
		case NM_Client:
			return EProcessorExecutionFlags::Client;
		case NM_Standalone:
			return EProcessorExecutionFlags::Standalone;
		default:
			checkf(false, TEXT("Unsupported ENetMode type (%i) found while determining MASS processor execution flags."), NetMode);
			return EProcessorExecutionFlags::None;
		}
	}
}

EProcessorExecutionFlags DetermineProcessorExecutionFlags(const UWorld* World, EProcessorExecutionFlags ExecutionFlagsOverride)
{
	if (ExecutionFlagsOverride != EProcessorExecutionFlags::None)
	{
		return ExecutionFlagsOverride;
	}
	if (World)
	{
		return UE::Mass::Utils::GetProcessorExecutionFlagsForWorld(*World);
	}
	return EProcessorExecutionFlags::All;
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
	return EntityManager
		? &EntityManager->GetMutableEntityManager()
		: nullptr;
}

FMassEntityManager& GetEntityManagerChecked(const UWorld& World)
{
	UMassEntitySubsystem* EntityManager = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntityManager);
	return EntityManager->GetMutableEntityManager();
}

} // namespace UE::Mass::Utils