// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWIClientActorSpawnerSubsystem.h"
#include "Engine/World.h"


//-----------------------------------------------------------------------------
// UMassLWIClientActorSpawnerSubsystem
//-----------------------------------------------------------------------------
void UMassLWIClientActorSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	check(World);
	check(World->GetNetMode() == NM_Client);
	
	WorldOnActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UMassLWIClientActorSpawnerSubsystem::OnActorSpawned));
}

bool UMassLWIClientActorSpawnerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	UWorld* World = Cast<UWorld>(Outer);
	return (World != nullptr && World->GetNetMode() == NM_Client);
}

void UMassLWIClientActorSpawnerSubsystem::OnActorSpawned(AActor* InActor)
{
	check(InActor);
	// check if it's an actor we're waiting for
	if (FMassStoredActorsContainer* StoredActors = PendingActors.Find(InActor->GetClass()))
	{
		// getting here means we're waiting for this class of actor
		StoredActors->Container.Add(InActor);
	}
}

ESpawnRequestStatus UMassLWIClientActorSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor) const
{
	constexpr double ComparisonPrecission = 1.0;
	const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<const FMassActorSpawnRequest>();
	TArray<TObjectPtr<AActor>>& StoredActors = PendingActors.FindOrAdd(SpawnRequest.Template).Container;

	for (auto It = StoredActors.CreateIterator(); It; ++It)
	{
		if (*It && (*It)->GetActorTransform().Equals(SpawnRequest.Transform, ComparisonPrecission))
		{
			OutSpawnedActor = *It;
			It.RemoveCurrentSwap();
			return ESpawnRequestStatus::Succeeded;
		}
	}

	return ESpawnRequestStatus::Pending;
}

void UMassLWIClientActorSpawnerSubsystem::RegisterRepresentedClass(UClass* RepresentedClass)
{
	if (TSubclassOf<AActor> ActorClass = RepresentedClass)
	{
		PendingActors.Add(MoveTemp(ActorClass));
	}
}
