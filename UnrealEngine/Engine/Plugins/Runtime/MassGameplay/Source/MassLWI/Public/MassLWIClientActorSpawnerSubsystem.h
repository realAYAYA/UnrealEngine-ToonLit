// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassExternalSubsystemTraits.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassLWIClientActorSpawnerSubsystem.generated.h"


USTRUCT()
struct MASSLWI_API FMassStoredActorsContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Container;
};

UCLASS()
class MASSLWI_API UMassLWIClientActorSpawnerSubsystem : public UMassActorSpawnerSubsystem
{
	GENERATED_BODY()

public:
	/** checks if RepresentedClass represents an actor class and if so creates an entry in PendingActors for the type */
	void RegisterRepresentedClass(UClass* RepresentedClass);

protected:
	// USubsystem API begin
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem API end

	// UMassActorSpawnerSubsystem API begin
	virtual ESpawnRequestStatus SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor) const override;
	// UMassActorSpawnerSubsystem API end

	void OnActorSpawned(AActor* InActor);

	FDelegateHandle WorldOnActorSpawnedHandle;

	UPROPERTY()
	mutable TMap<TSubclassOf<AActor>, FMassStoredActorsContainer> PendingActors;
};

template<>
struct TMassExternalSubsystemTraits<UMassLWIClientActorSpawnerSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};