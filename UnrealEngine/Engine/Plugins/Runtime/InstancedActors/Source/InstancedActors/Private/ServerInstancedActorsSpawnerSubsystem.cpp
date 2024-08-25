// Copyright Epic Games, Inc. All Rights Reserved.


#include "ServerInstancedActorsSpawnerSubsystem.h"
#include "InstancedActorsComponent.h"
#include "InstancedActorsManager.h"
#include "InstancedActorsData.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "InstancedActorsSettings.h"


bool UServerInstancedActorsSpawnerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// do not instantiate if configured to use a different (sub)class
	if (GET_INSTANCEDACTORS_CONFIG_VALUE(ServerActorSpawnerSubsystemClass) != GetClass())
	{
		return false;
	}

	// @todo Add support for non-replay NM_Standalone where we should use UServerInstancedActorsSpawnerSubsystem for 
	// authoritative actor spawning.
	UWorld* World = Cast<UWorld>(Outer);
	return (World != nullptr && World->GetNetMode() == NM_DedicatedServer);
}

bool UServerInstancedActorsSpawnerSubsystem::ReleaseActorToPool(AActor* Actor)
{
	return Super::ReleaseActorToPool(Actor);
}

void UServerInstancedActorsSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UMassEntitySubsystem* EntitySubsystem = Collection.InitializeDependency<UMassEntitySubsystem>();
	check(EntitySubsystem);
	EntityManager = EntitySubsystem->GetMutableEntityManager().AsShared();
}

void UServerInstancedActorsSpawnerSubsystem::Deinitialize()
{
	EntityManager.Reset();

	Super::Deinitialize();
}

ESpawnRequestStatus UServerInstancedActorsSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UServerInstancedActorsSpawnerSubsystem::SpawnActor);

	UWorld* World = GetWorld();
	check(World);
	check(World->GetNetMode() == NM_DedicatedServer);

	const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<const FMassActorSpawnRequest>();
	UInstancedActorsData* InstanceData = UInstancedActorsData::GetInstanceDataForEntity(*EntityManager, SpawnRequest.MassAgent);
	check(InstanceData);
	const FInstancedActorsInstanceIndex InstanceIndex = InstanceData->GetInstanceIndexForEntity(SpawnRequest.MassAgent);
	const FInstancedActorsInstanceHandle InstanceHandle(*InstanceData, InstanceIndex);

	// Record currently spawning IA instance for OnInstancedActorComponentInitialize to check
	TransientActorSpawningInstance = InstanceHandle;
	ON_SCOPE_EXIT 
	{
		TransientActorBeingSpawned = nullptr;
		TransientActorSpawningInstance.Reset();
	};

	InOutSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	OutSpawnedActor = World->SpawnActor<AActor>(SpawnRequest.Template, SpawnRequest.Transform, InOutSpawnParameters);
	// @todo this is a temporary solution, the whole idea is yucky and needs to be reimplemented.
	// Before this addition TransientActorBeingSpawned was only being set in Juno's custom 
	// InOutSpawnParameters.CustomPreSpawnInitalization delegate
	TransientActorBeingSpawned = OutSpawnedActor;

	// Add an UInstancedActorsComponent if one isn't present and ensure replication is enabled to replicate the InstanceHandle 
	// to clients for Mass entity matchup in UInstancedActorsComponent::OnRep_InstanceHandle
	UInstancedActorsComponent* InstancedActorComponent = OutSpawnedActor->GetComponentByClass<UInstancedActorsComponent>();
	if (InstancedActorComponent)
	{
		// If the component is set to replicate by default, we assume AddComponentTypesAllowListedForReplication has 
		// already been performed.
		if (!InstancedActorComponent->GetIsReplicated())
		{
			InstancedActorComponent->SetIsReplicated(true);
		}
	}
	else
	{
		// No exising UInstancedActorsComponent class or subclass, add a new UInstancedActorsComponent
		InstancedActorComponent = NewObject<UInstancedActorsComponent>(OutSpawnedActor);
		InstancedActorComponent->SetIsReplicated(true);
		InstancedActorComponent->RegisterComponent();
	}
	
	return IsValid(OutSpawnedActor) ? ESpawnRequestStatus::Succeeded : ESpawnRequestStatus::Failed;
}

void UServerInstancedActorsSpawnerSubsystem::OnInstancedActorComponentInitialize(UInstancedActorsComponent& InstancedActorComponent) const
{
	// Does this component belong to an actor we're in the middle of spawning in UServerInstancedActorsSpawnerSubsystem::SpawnActor?
	//
	// Note: This may not always be the case, as OnInstancedActorComponentInitialize is called by UInstancedActorsComponent::InitializeComponent 
	// regardless of whether the actor was spawned by Instanced Actors or not, as we can't yet know if it was (this callback is in place to
	// *attempt* to find that out). Actors using UInstancedActorComponents aren't 'required' to be spawned with Instanced Actors, rather: the
	// components are expected to provide functionality without Mass and simply provide *additional* ability to continue their functionality once
	// the actor is 'dehydrated' into the lower LOD representation in Mass.
	if (InstancedActorComponent.GetOwner() == TransientActorBeingSpawned)
	{
		// Pass the IA instance responsible for spawning this actor. Importantly the UInstancedActorsComponent will now have a link
		// to Mass before / by the time it receives BeginPlay.
		check(TransientActorSpawningInstance.IsValid());
		InstancedActorComponent.InitializeComponentForInstance(TransientActorSpawningInstance);
	}
}
