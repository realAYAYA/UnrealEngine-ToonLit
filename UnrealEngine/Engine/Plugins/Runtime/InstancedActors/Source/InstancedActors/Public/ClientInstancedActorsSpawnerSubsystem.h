// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassExternalSubsystemTraits.h"
#include "MassActorSpawnerSubsystem.h"
#include "ClientInstancedActorsSpawnerSubsystem.generated.h"


/** 
 * Used on Clients to handle actor spawning synchronized with the Server. At the moment it boils down to storing
 * actor spawning requests and putting them in Pending state until the server-spawned actor gets replicated
 * over to the Client.
 */
UCLASS()
class INSTANCEDACTORS_API UClientInstancedActorsSpawnerSubsystem : public UMassActorSpawnerSubsystem
{
	GENERATED_BODY()

protected:
	//~ Begin USubsystem Overrides
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ Begin USubsystem Overrides

	//~ Begin UMassActorSpawnerSubsystem Overrides
	virtual ESpawnRequestStatus SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const override;
	virtual bool ReleaseActorToPool(AActor* Actor) override;
	//~ End UMassActorSpawnerSubsystem Overrides
};
