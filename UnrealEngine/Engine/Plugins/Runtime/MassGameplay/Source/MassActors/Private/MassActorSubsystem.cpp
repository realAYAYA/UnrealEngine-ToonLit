// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassActorSubsystem.h"
#include "MassCommonTypes.h"
#include "Engine/World.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationSubsystem.h"
#include "VisualLogger/VisualLogger.h"

//----------------------------------------------------------------------//
//  FMassActorFragment 
//----------------------------------------------------------------------//

void FMassActorFragment::SetAndUpdateHandleMap(const FMassEntityHandle MassAgent, AActor* InActor, const bool bInIsOwnedByMass)
{
	SetNoHandleMapUpdate(MassAgent, InActor, bInIsOwnedByMass);

	UWorld* World = InActor->GetWorld();
	check(World);
	if (UMassActorSubsystem* MassActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(World))
	{
		MassActorSubsystem->SetHandleForActor(InActor, MassAgent);
	}
}

void FMassActorFragment::ResetAndUpdateHandleMap()
{
	if (AActor* ActorPtr = Cast<AActor>(Actor.Get()))
	{
		UWorld* World = Actor->GetWorld();
		check(World);
		if (UMassActorSubsystem* MassActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(World))
		{
			MassActorSubsystem->RemoveHandleForActor(ActorPtr);
		}
	}

	ResetNoHandleMapUpdate();
}

void FMassActorFragment::SetNoHandleMapUpdate(const FMassEntityHandle MassAgent, AActor* InActor, const bool bInIsOwnedByMass)
{
	check(InActor);
	check(!Actor.IsValid());
	check(MassAgent.IsValid());
	Actor = InActor;
	bIsOwnedByMass = bInIsOwnedByMass;
}

void FMassActorFragment::ResetNoHandleMapUpdate()
{
	Actor.Reset();
	bIsOwnedByMass = false;
}

//----------------------------------------------------------------------//
//  UMassActorSubsystem 
//----------------------------------------------------------------------//
void UMassActorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// making sure UMassSimulationSubsystem gets created before the MassActorSubsystem
	Collection.InitializeDependency<UMassSimulationSubsystem>();
	
	if (UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld()))
	{
		EntityManager = EntitySubsystem->GetMutableEntityManager().AsShared();
	}
}

void UMassActorSubsystem::Deinitialize()
{
	EntityManager.Reset();
}

FMassEntityHandle UMassActorSubsystem::GetEntityHandleFromActor(const TObjectKey<const AActor> Actor)
{
	UE_MT_SCOPED_READ_ACCESS(ActorHandleMapDetector);
	FMassEntityHandle* Entity = ActorHandleMap.Find(Actor);
	if (!Entity)
	{
		return FMassEntityManager::InvalidEntity;
	}

	check(TObjectKey<const AActor>(GetActorFromHandle(*Entity)) == Actor);
	return *Entity;
}

AActor* UMassActorSubsystem::GetActorFromHandle(const FMassEntityHandle Handle) const
{
	check(EntityManager);
	FMassActorFragment* Data = EntityManager->GetFragmentDataPtr<FMassActorFragment>(Handle);
	return Data != nullptr ? Data->GetMutable() : nullptr;
}

void UMassActorSubsystem::SetHandleForActor(const TObjectKey<const AActor> Actor, const FMassEntityHandle Handle)
{
	UE_MT_SCOPED_WRITE_ACCESS(ActorHandleMapDetector);
	ActorHandleMap.Add(Actor, Handle);
}

void UMassActorSubsystem::RemoveHandleForActor(const TObjectKey<const AActor> Actor)
{
	UE_MT_SCOPED_WRITE_ACCESS(ActorHandleMapDetector);
	ActorHandleMap.Remove(Actor);
}

void UMassActorSubsystem::DisconnectActor(const TObjectKey<const AActor> Actor, const FMassEntityHandle Handle)
{
	if (Handle.IsValid() == false)
	{
		return;
	}

	FMassEntityHandle FoundEntity;
	{
		UE_MT_SCOPED_WRITE_ACCESS(ActorHandleMapDetector);
		// We're assuming the Handle does match Actor, so we're RemoveAndCopyValue. If if doesn't we'll add it back.
		// The expectation is that this won't happen on a regular basis..
		if (ActorHandleMap.RemoveAndCopyValue(Actor, FoundEntity) == false)
		{
			// the entity doesn't match the actor
			return;
		}
	}

	if (FoundEntity == Handle)
	{
		check(EntityManager);
		if (FMassActorFragment* Data = EntityManager->GetFragmentDataPtr<FMassActorFragment>(Handle))
		{
			Data->ResetAndUpdateHandleMap();
		}
	}
	else
	{
		// unexpected mismatch. Add back and notify.
		UE_VLOG_UELOG(this, LogMass, Warning, TEXT("%s: Trying to disconnect actor %s while the Handle given doesn't match the system\'s records")
			, ANSI_TO_TCHAR(__FUNCTION__), *AActor::GetDebugName(Actor.ResolveObjectPtr()));
		SetHandleForActor(Actor, Handle);
	}
}