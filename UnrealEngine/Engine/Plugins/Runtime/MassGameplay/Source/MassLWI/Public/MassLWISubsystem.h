// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassRepresentationSubsystem.h"
#include "MassLWISubsystem.generated.h"


struct FActorInstanceHandle;
class AMassLWIStaticMeshManager;
class AMassLWIConfigActor;
class UMassLWIClientActorSpawnerSubsystem;
struct FMassEntityManager;


UCLASS()
class MASSLWI_API UMassLWISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	//~USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~End of USubsystem interface

public:
	UMassLWISubsystem();

	void RegisterLWIManager(AMassLWIStaticMeshManager& Manager);
	void UnregisterLWIManager(AMassLWIStaticMeshManager& Manager);

	const FMassEntityConfig* GetConfigForClass(TSubclassOf<AActor> ActorClass) const;

	void RegisterConfigActor(AMassLWIConfigActor& ConfigActor);

protected:
	void RegisterExistingConfigActors();

	UPROPERTY()
	TArray<TObjectPtr<AMassLWIStaticMeshManager>> RegisteredManagers;

	TArray<int32> FreeIndices;

	UPROPERTY()
	FMassEntityConfig DefaultConfig;

	TMap<UClass*, FMassEntityConfig> ClassToConfigMap;

	UPROPERTY()
	TObjectPtr<UMassLWIClientActorSpawnerSubsystem> LWISpawnerSubsystem;

private:
	TSharedPtr<FMassEntityManager> EntityManager;
};

template<>
struct TMassExternalSubsystemTraits<UMassLWISubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};
