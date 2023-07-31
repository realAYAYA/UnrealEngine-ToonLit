// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "InstancedStruct.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassSpawnerSubsystem.generated.h"

struct FMassEntityManager;
struct FMassEntityTemplate;
class UMassEntityTemplateRegistry;
struct FInstancedStruct;
struct FStructView;
struct FMassEntityTemplateID;
class UMassSimulationSubsystem;

UCLASS()
class MASSSPAWNER_API UMassSpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Spawns entities of the kind described by the given EntityTemplate. The spawned entities are fully initialized
	 *  meaning the EntityTemplate.InitializationPipeline gets run for all spawned entities.
	 *  @param EntityTemplate template to use for spawning entities
	 *  @param NumberToSpawn number of entities to spawn
	 *  @param OutEntities where the IDs of created entities get added. Note that the contents of OutEntities get overridden by the function.
	 *  @return true if spawning was successful, false otherwise. In case of failure see logs for more details. */
	void SpawnEntities(const FMassEntityTemplate& EntityTemplate, const uint32 NumberToSpawn, TArray<FMassEntityHandle>& OutEntities);

	void SpawnEntities(FMassEntityTemplateID TemplateID, const uint32 NumberToSpawn, FConstStructView SpawnData, TSubclassOf<UMassProcessor> InitializerClass, TArray<FMassEntityHandle>& OutEntities);

	void SpawnFromConfig(FStructView Config, const int32 NumToSpawn, FConstStructView SpawnData, TSubclassOf<UMassProcessor> InitializerClass);

	void DestroyEntities(const FMassEntityTemplateID TemplateID, TConstArrayView<FMassEntityHandle> Entities);

	UMassEntityTemplateRegistry& GetTemplateRegistryInstance() const { check(TemplateRegistryInstance); return *TemplateRegistryInstance; }

	void RegisterCollection(TArrayView<FInstancedStruct> Collection);

	const FMassEntityTemplate* GetMassEntityTemplate(FMassEntityTemplateID TemplateID) const;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PostInitialize() override;

	void DoSpawning(const FMassEntityTemplate& EntityTemplate, const int32 NumToSpawn, FConstStructView SpawnData, TSubclassOf<UMassProcessor> InitializerClass, TArray<FMassEntityHandle>& OutEntities);

	UMassProcessor* GetSpawnDataInitializer(TSubclassOf<UMassProcessor> InitializerClass);

	UPROPERTY()
	TArray<TObjectPtr<UMassProcessor>> SpawnDataInitializers;

	TSharedPtr<FMassEntityManager> EntityManager;

	UPROPERTY()
	TObjectPtr<UMassEntityTemplateRegistry> TemplateRegistryInstance;

	UPROPERTY()
	TObjectPtr<UMassSimulationSubsystem> SimulationSystem;
};

