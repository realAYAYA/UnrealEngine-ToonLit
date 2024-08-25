// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientInstancedActorsSpawnerSubsystem.h"
#include "InstancedActorsComponent.h"
#include "InstancedActorsDebug.h"
#include "InstancedActorsData.h"
#include "InstancedActorsTypes.h"

#include "Components/StaticMeshComponent.h"
#include "MassEntitySubsystem.h"
#include "Engine/Level.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "InstancedActorsManager.h"
#include "InstancedActorsSettings.h"


//-----------------------------------------------------------------------------
// UClientInstancedActorsSpawnerSubsystem
//-----------------------------------------------------------------------------
bool UClientInstancedActorsSpawnerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// do not instantiate if configured to use a different (sub)class
	if (GET_INSTANCEDACTORS_CONFIG_VALUE(ClientActorSpawnerSubsystemClass) != GetClass())
	{
		return false;
	}

	// @todo Add support for non-replay NM_Standalone where we should use UServerInstancedActorsSpawnerSubsystem for 
	// authoritative actor spawning.
	UWorld* World = Cast<UWorld>(Outer);
	return (World != nullptr && World->GetNetMode() != NM_DedicatedServer);
}

ESpawnRequestStatus UClientInstancedActorsSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const
{
	// UInstancedActorsVisualizationTrait should be setting all LOD representations to StaticMeshInstance to avoid
	// ever attempting to natively spawn an actor.
	// 
	// Instead we rely on UInstancedActorsComponent's, added dynamically to all actors on spawn in UServerInstancedActorsSpawnerSubsystem::SpawnActor, 
	// to replicate over and 'inject' the Actor into Mass in UInstancedActorsComponent::OnRep_InstanceHandle.
	//
	// UInstancedActorsVisualizationTrait also sets bForceActorRepresentationWhileAvailable on clients so that once the replicated 
	// actor is registered with Mass, we switch into Actor representation and stay there until Actor destruction is replicate,
	// whereupon we switch back to whatever the natural wanted representation is (ISMC)
	ensureMsgf(false, TEXT("UClientInstancedActorsSpawnerSubsystem::SpawnActor unexpectedly called on client where we shouldn't ever be trying to spawn new actors."));

	return ESpawnRequestStatus::Pending;
}

bool UClientInstancedActorsSpawnerSubsystem::ReleaseActorToPool(AActor* Actor)
{
	// As we set bForceActorRepresentationWhileAvailable on clients (see above), we should never be attempting to 
	// explicitly destroy actors from Mass. Rather, actor destruction should be happening via replication from server,
	// relying on UInstancedActorsComponent::EndPlay to clean up the actor reference in Mass.
	ensureMsgf(false, TEXT("UClientInstancedActorsSpawnerSubsystem::ReleaseActorToPool unexpectedly called on client where we shouldn't ever be trying to destroy actors."));

	return true;
}
