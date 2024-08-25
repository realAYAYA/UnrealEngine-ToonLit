// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "InstancedStruct.h"
#include "MassSubsystemBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassSpawnerSubsystem.generated.h"

struct FMassEntityManager;
struct FMassEntityTemplate;
struct FInstancedStruct;
struct FStructView;
struct FMassEntityTemplateID;
class UMassSimulationSubsystem;

UCLASS()
class MASSSPAWNER_API UMassSpawnerSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:
	UMassSpawnerSubsystem();

	/** Spawns entities of the kind described by the given EntityTemplate. The spawned entities are fully initialized
	 *  meaning the EntityTemplate.InitializationPipeline gets run for all spawned entities.
	 *  @param EntityTemplate template to use for spawning entities
	 *  @param NumberToSpawn number of entities to spawn
	 *  @param OutEntities where the IDs of created entities get added. Note that the contents of OutEntities get overridden by the function.
	 *  @return true if spawning was successful, false otherwise. In case of failure see logs for more details. */
	void SpawnEntities(const FMassEntityTemplate& EntityTemplate, const uint32 NumberToSpawn, TArray<FMassEntityHandle>& OutEntities);

	void SpawnEntities(FMassEntityTemplateID TemplateID, const uint32 NumberToSpawn, FConstStructView SpawnData, TSubclassOf<UMassProcessor> InitializerClass, TArray<FMassEntityHandle>& OutEntities);

	void DestroyEntities(TConstArrayView<FMassEntityHandle> Entities);

	UE_DEPRECATED(5.3, "This flavor of DestroyEntities has been deprecated. Use the one without the FMassEntityTemplateID parameter")
	void DestroyEntities(const FMassEntityTemplateID TemplateID, TConstArrayView<FMassEntityHandle> Entities)
	{
		DestroyEntities(Entities);
	}

	const FMassEntityTemplateRegistry& GetTemplateRegistryInstance() const { return TemplateRegistryInstance; }
	FMassEntityTemplateRegistry& GetMutableTemplateRegistryInstance() { return TemplateRegistryInstance; }

	const FMassEntityTemplate* GetMassEntityTemplate(FMassEntityTemplateID TemplateID) const;

protected:
	// UWorldSubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// UWorldSubsystem END

	void DoSpawning(const FMassEntityTemplate& EntityTemplate, const int32 NumToSpawn, FConstStructView SpawnData, TSubclassOf<UMassProcessor> InitializerClass, TArray<FMassEntityHandle>& OutEntities);

	UMassProcessor* GetSpawnDataInitializer(TSubclassOf<UMassProcessor> InitializerClass);

	UPROPERTY()
	TArray<TObjectPtr<UMassProcessor>> SpawnDataInitializers;

	TSharedPtr<FMassEntityManager> EntityManager;

	FMassEntityTemplateRegistry TemplateRegistryInstance;
};

