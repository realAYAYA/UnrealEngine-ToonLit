// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWISubsystem.h"
#include "EngineUtils.h"
#include "MassLWITypes.h"
#include "MassLWIStaticMeshManager.h"
#include "MassLWIConfigActor.h"
#include "MassEntitySubsystem.h"
#include "MassLWIClientActorSpawnerSubsystem.h"
#include "VisualLogger/VisualLogger.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(MassLWISubsystem)

//-----------------------------------------------------------------------------
// UMassLWISubsystem
//-----------------------------------------------------------------------------
UMassLWISubsystem::UMassLWISubsystem()
{

}

void UMassLWISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UMassEntitySubsystem* EntitySubsystem = Collection.InitializeDependency<UMassEntitySubsystem>();
	check(EntitySubsystem);
	EntityManager = EntitySubsystem->GetMutableEntityManager().AsShared();

	// note that it's ok for LWISpawnerSubsystem to be null since in principle it's only available on clients,
	// while UMassLWISubsystem can be created on both.
	LWISpawnerSubsystem = Collection.InitializeDependency<UMassLWIClientActorSpawnerSubsystem>();
}

void UMassLWISubsystem::PostInitialize()
{
	Super::PostInitialize();

	RegisterExistingConfigActors();
}

void UMassLWISubsystem::Deinitialize()
{
	EntityManager = nullptr;
	Super::Deinitialize();
}

bool UMassLWISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	UWorld* World = Outer ? Outer->GetWorld() : nullptr;
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMassLWISubsystem::RegisterLWIManager(AMassLWIStaticMeshManager& Manager)
{
	check(EntityManager);
	if (Manager.IsRegisteredWithMass())
	{
		return;
	}
	checkSlow(RegisteredManagers.Find(&Manager) == INDEX_NONE);

	REDIRECT_OBJECT_TO_VLOG(&Manager, this);
	
	int32 RegistrationIndex = INDEX_NONE;
	if (FreeIndices.Num() > 0)
	{
		RegistrationIndex = FreeIndices.Pop(/*bAllowShrinking = */true);
		RegisteredManagers[RegistrationIndex] = &Manager;
	}
	else
	{
		RegistrationIndex = RegisteredManagers.Add(&Manager);
	}

	if (LWISpawnerSubsystem)
	{
		LWISpawnerSubsystem->RegisterRepresentedClass(Manager.GetRepresentedClass());
	}

	Manager.TransferDataToMass(*EntityManager);
	Manager.MarkRegisteredWithMass(FMassLWIManagerRegistrationHandle(RegistrationIndex));
}

void UMassLWISubsystem::UnregisterLWIManager(AMassLWIStaticMeshManager& Manager)
{
	if (Manager.IsRegisteredWithMass() == false)
	{
		return;
	}

	FMassLWIManagerRegistrationHandle IndexHandle = Manager.GetMassRegistrationHandle();
	if (RegisteredManagers.IsValidIndex(IndexHandle))
	{
		FreeIndices.Add(IndexHandle);
		RegisteredManagers[IndexHandle] = nullptr;
	}

	Manager.MarkUnregisteredWithMass();
}

void UMassLWISubsystem::RegisterConfigActor(AMassLWIConfigActor& ConfigActor)
{
	if (ConfigActor.IsRegisteredWithSubsystem())
	{
		return;
	}

	if (DefaultConfig.IsEmpty() && !ConfigActor.DefaultConfig.IsEmpty())
	{
		DefaultConfig = ConfigActor.DefaultConfig;
	}

}

void UMassLWISubsystem::RegisterExistingConfigActors()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AMassLWIConfigActor> It(World); It; ++It)
	{
		RegisterConfigActor(**It);
	}
}

const FMassEntityConfig* UMassLWISubsystem::GetConfigForClass(TSubclassOf<AActor> ActorClass) const
{
	if (const FMassEntityConfig* ConfigFound = ClassToConfigMap.Find(ActorClass))
	{
		return ConfigFound;
	}
	else if (DefaultConfig.IsEmpty() == false)
	{
		return &DefaultConfig;
	}
	return nullptr;
}
