// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsRepresentationSubsystem.h"
#include "InstancedActorsSettings.h"


void UInstancedActorsRepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// @todo Add support for non-replay NM_Standalone where we should use UServerInstancedActorsSpawnerSubsystem for 
	// authoritative actor spawning.
	if (GetWorldRef().GetNetMode() == NM_DedicatedServer)
	{
		ActorSpawnerSubsystem = Cast<UMassActorSpawnerSubsystem>(Collection.InitializeDependency(GET_INSTANCEDACTORS_CONFIG_VALUE(GetServerActorSpawnerSubsystemClass())));
	}
	else
	{
		ActorSpawnerSubsystem = Cast<UMassActorSpawnerSubsystem>(Collection.InitializeDependency(GET_INSTANCEDACTORS_CONFIG_VALUE(GetClientActorSpawnerSubsystemClass())));
	}

	ensureMsgf(ActorSpawnerSubsystem, TEXT("Trying to initialize dependency on class %s failed. Verify InstanedActors settings.")
		, *GetNameSafe(ActorSpawnerSubsystem));
}
