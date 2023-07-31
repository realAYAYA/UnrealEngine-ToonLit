// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassActorSpawnerSubsystem.h"
#include "MassAgentComponent.h"
#include "MassSimulationSettings.h"
#include "Engine/World.h"
#include "MassActorTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "MassActorPoolableInterface.h"
#include "MassSimulationSubsystem.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "ProfilingDebugging/CsvProfiler.h"

CSV_DEFINE_CATEGORY(MassActors, true);

namespace UE::MassActors
{
	int32 bUseActorPooling = 1;
	FAutoConsoleVariableRef CVarDebugRepresentationLOD(TEXT("ai.mass.actorpooling"), bUseActorPooling, TEXT("Use Actor Pooling"), ECVF_Scalability);
}

//----------------------------------------------------------------------//
// UMassActorSpawnerSubsystem 
//----------------------------------------------------------------------//
void UMassActorSpawnerSubsystem::RetryActorSpawnRequest(const FMassActorSpawnRequestHandle SpawnRequestHandle)
{
	check(SpawnRequestHandleManager.IsValidHandle(SpawnRequestHandle));
	const int32 Index = SpawnRequestHandle.GetIndex();
	check(SpawnRequests.IsValidIndex(Index));
	FMassActorSpawnRequest& SpawnRequest = SpawnRequests[SpawnRequestHandle.GetIndex()].GetMutable<FMassActorSpawnRequest>();
	if (ensureMsgf(SpawnRequest.SpawnStatus == ESpawnRequestStatus::Failed, TEXT("Can only retry failed spawn requests")))
	{
		UWorld* World = GetWorld();
		check(World);

		SpawnRequest.SpawnStatus = ESpawnRequestStatus::RetryPending;
		SpawnRequest.SerialNumber = RequestSerialNumberCounter.fetch_add(1);
		SpawnRequest.RequestedTime = World->GetTimeSeconds();
	}
}

bool UMassActorSpawnerSubsystem::RemoveActorSpawnRequest(FMassActorSpawnRequestHandle& SpawnRequestHandle)
{
	if (!ensureMsgf(SpawnRequestHandleManager.RemoveHandle(SpawnRequestHandle), TEXT("Invalid spawn request handle")))
	{
		return false;
	}

	check(SpawnRequests.IsValidIndex(SpawnRequestHandle.GetIndex()));
	FMassActorSpawnRequest& SpawnRequest = SpawnRequests[SpawnRequestHandle.GetIndex()].GetMutable<FMassActorSpawnRequest>();
	check(SpawnRequest.SpawnStatus != ESpawnRequestStatus::Processing);
	SpawnRequestHandle.Invalidate();
	SpawnRequest.Reset();
	return true;
}

void UMassActorSpawnerSubsystem::DestroyActor(AActor* Actor, bool bImmediate /*= false*/)
{
	// We need to unregister immediately MassAgentComponent as it will become out of sync with mass
	if (UMassAgentComponent* AgentComp = Actor->FindComponentByClass<UMassAgentComponent>())
	{
		if (AgentComp->IsRegistered())
		{
			AgentComp->UnregisterComponent();
		}
	}

	if(bImmediate)
	{
		if (!ReleaseActorToPool(Actor))
		{
			UWorld* World = GetWorld();
			check(World);

			World->DestroyActor(Actor);
			--NumActorSpawned;
		}
	}
	else
	{
		ActorsToDestroy.Add(Actor);
	}
}

bool UMassActorSpawnerSubsystem::ReleaseActorToPool(AActor* Actor)
{
	if (!UE::MassActors::bUseActorPooling || !bActorPoolingEnabled)
	{
		return false;
	}

	const bool bIsPoolableActor = Actor->Implements<UMassActorPoolableInterface>();
	if (bIsPoolableActor && IMassActorPoolableInterface::Execute_CanBePooled(Actor))
	{
		UMassAgentComponent* AgentComp = Actor->FindComponentByClass<UMassAgentComponent>();
		if (!AgentComp || AgentComp->IsReadyForPooling())
		{
			IMassActorPoolableInterface::Execute_PrepareForPooling(Actor);
			Actor->SetActorHiddenInGame(true);
			if (AgentComp)
			{
				AgentComp->UnregisterWithAgentSubsystem();
			}

			TArray<AActor*>& Pool = PooledActors.FindOrAdd(Actor->GetClass());
			checkf(Pool.Find(Actor) == INDEX_NONE, TEXT("Actor%s is already in the pool"), *AActor::GetDebugName(Actor));
			Pool.Add(Actor);
			++NumActorPooled;
			return true;
		}
	}
	return false;
}

FMassActorSpawnRequestHandle UMassActorSpawnerSubsystem::RequestActorSpawnInternal(const FConstStructView SpawnRequestView)
{
	// The handle manager has a freelist of the release indexes, so it can return us a index that we previously used.
	const FMassActorSpawnRequestHandle SpawnRequestHandle = SpawnRequestHandleManager.GetNextHandle();
	const int32 Index = SpawnRequestHandle.GetIndex();

	// Check if we need to grow the array, otherwise it is a previously released index that was returned.
	if (!SpawnRequests.IsValidIndex(Index))
	{
		checkf(SpawnRequests.Num() == Index, TEXT("This case should only be when we need to grow the array of one element."));
		SpawnRequests.Add(SpawnRequestView);
	}
	else
	{
		SpawnRequests[Index] = SpawnRequestView;
	}

	UWorld* World = GetWorld();
	check(World);

	// Initialize the spawn request status
	FMassActorSpawnRequest& SpawnRequest = GetMutableSpawnRequest<FMassActorSpawnRequest>(SpawnRequestHandle);
	SpawnRequest.SpawnStatus = ESpawnRequestStatus::Pending;
	SpawnRequest.SerialNumber = RequestSerialNumberCounter.fetch_add(1);
	SpawnRequest.RequestedTime = World->GetTimeSeconds();

	return SpawnRequestHandle;
}

FMassActorSpawnRequestHandle UMassActorSpawnerSubsystem::GetNextRequestToSpawn() const
{
	FMassActorSpawnRequestHandle BestSpawnRequestHandle;
	float BestPriority = MAX_FLT;
	bool bBestIsPending = false;
	uint32 BestSerialNumber = MAX_uint32;
	for (const FMassActorSpawnRequestHandle SpawnRequestHandle : SpawnRequestHandleManager.GetHandles())
	{
		if (!SpawnRequestHandle.IsValid())
		{
			continue;
		}
		const FMassActorSpawnRequest& SpawnRequest = GetSpawnRequest<FMassActorSpawnRequest>(SpawnRequestHandle);
		if (SpawnRequest.SpawnStatus == ESpawnRequestStatus::Pending)
		{
			if (!bBestIsPending ||
				SpawnRequest.Priority < BestPriority ||
				(SpawnRequest.Priority == BestPriority && SpawnRequest.SerialNumber < BestSerialNumber))
			{
				BestSpawnRequestHandle = SpawnRequestHandle;
				BestSerialNumber = SpawnRequest.SerialNumber;
				BestPriority = SpawnRequest.Priority;
				bBestIsPending = true;
			}
		}
		else if (!bBestIsPending && SpawnRequest.SpawnStatus == ESpawnRequestStatus::RetryPending)
		{
			// No priority on retries just FIFO
			if (SpawnRequest.SerialNumber < BestSerialNumber)
			{
				BestSpawnRequestHandle = SpawnRequestHandle;
				BestSerialNumber = SpawnRequest.SerialNumber;
			}
		}
	}

	return BestSpawnRequestHandle;
}

AActor* UMassActorSpawnerSubsystem::SpawnOrRetrieveFromPool(FConstStructView SpawnRequestView)
{
	if (UE::MassActors::bUseActorPooling != 0 && bActorPoolingEnabled)
	{
		const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<FMassActorSpawnRequest>();
		TArray<AActor*>* Pool = PooledActors.Find(SpawnRequest.Template);

		if (Pool && Pool->Num() > 0)
		{
			AActor* PooledActor = (*Pool)[0];
			Pool->RemoveAt(0);
			--NumActorPooled;
			PooledActor->SetActorHiddenInGame(false);
			PooledActor->SetActorTransform(SpawnRequest.Transform, false, nullptr, ETeleportType::ResetPhysics);

			IMassActorPoolableInterface::Execute_PrepareForGame(PooledActor);

			if (UMassAgentComponent* AgentComp = PooledActor->FindComponentByClass<UMassAgentComponent>())
			{
				AgentComp->RegisterWithAgentSubsystem();
				AgentComp->SetPuppetHandle(SpawnRequest.MassAgent);
			}

			return PooledActor;
		}
	}

	return SpawnActor(SpawnRequestView);
}

AActor* UMassActorSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassActorSpawnerSubsystem::SpawnActor);

	UWorld* World = GetWorld();
	check(World);

	const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<FMassActorSpawnRequest>();
	if (AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(SpawnRequest.Template, SpawnRequest.Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		// Add code here before construction script

		SpawnedActor->FinishSpawning(SpawnRequest.Transform);
		++NumActorSpawned;
		// The finish spawning might have failed and the spawned actor is destroyed.
		if (IsValidChecked(SpawnedActor))
		{
			if (UMassAgentComponent* AgentComp = SpawnedActor->FindComponentByClass<UMassAgentComponent>())
			{
				AgentComp->SetPuppetHandle(SpawnRequest.MassAgent);
			}
			return SpawnedActor;
		}
	}

	UE_VLOG_CAPSULE(this, LogMassActor, Error,
					SpawnRequest.Transform.GetLocation(),
					SpawnRequest.Template.GetDefaultObject()->GetSimpleCollisionHalfHeight(),
					SpawnRequest.Template.GetDefaultObject()->GetSimpleCollisionRadius(),
					SpawnRequest.Transform.GetRotation(),
					FColor::Red,
					TEXT("Unable to spawn actor for Mass entity [%s]"), *SpawnRequest.MassAgent.DebugGetDescription());
	return nullptr;
}

void UMassActorSpawnerSubsystem::ProcessPendingSpawningRequest(const double MaxTimeSlicePerTick)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassActorSpawnerSubsystem::ProcessPendingSpawningRequest);
	SpawnRequestHandleManager.ShrinkHandles();

	const double TimeSliceEnd = FPlatformTime::Seconds() + MaxTimeSlicePerTick;

	while (FPlatformTime::Seconds() < TimeSliceEnd)
	{
		FMassActorSpawnRequestHandle SpawnRequestHandle = GetNextRequestToSpawn();
		if (!SpawnRequestHandle.IsValid() ||
			!ensureMsgf(SpawnRequestHandleManager.IsValidHandle(SpawnRequestHandle), TEXT("GetNextRequestToSpawn returned an invalid handle, expecting an empty one or a valid one.")))
		{
			return;
		}

		FStructView SpawnRequestView = SpawnRequests[SpawnRequestHandle.GetIndex()];
		FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.GetMutable<FMassActorSpawnRequest>();

		if (!ensureMsgf(SpawnRequest.SpawnStatus == ESpawnRequestStatus::Pending ||
						SpawnRequest.SpawnStatus == ESpawnRequestStatus::RetryPending, TEXT("GetNextRequestToSpawn returned a request that was already processed, need to return only request with pending status.")))
		{
			return;
		}

		// Do the spawning
		SpawnRequest.SpawnStatus = ESpawnRequestStatus::Processing;

		// Call the pre spawn delegate on the spawn request
		if (SpawnRequest.ActorPreSpawnDelegate.IsBound())
		{
			SpawnRequest.ActorPreSpawnDelegate.Execute(SpawnRequestHandle, SpawnRequestView);
		}

		SpawnRequest.SpawnedActor = SpawnOrRetrieveFromPool(SpawnRequestView);

		SpawnRequest.SpawnStatus = SpawnRequest.SpawnedActor ? ESpawnRequestStatus::Succeeded : ESpawnRequestStatus::Failed;

		// Call the post spawn delegate on the spawn request
		if (SpawnRequest.ActorPostSpawnDelegate.IsBound())
		{
			if (SpawnRequest.ActorPostSpawnDelegate.Execute(SpawnRequestHandle, SpawnRequestView) == EMassActorSpawnRequestAction::Remove)
			{
				// If notified, remove the spawning request
				ensureMsgf(SpawnRequestHandleManager.RemoveHandle(SpawnRequestHandle), TEXT("When providing a delegate, the spawn request gets automatically removed, no need to remove it on your side"));
			}
		}
	}
}

void UMassActorSpawnerSubsystem::ProcessPendingDestruction(const double MaxTimeSlicePerTick)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassActorSpawnerSubsystem::ProcessPendingDestruction);

	UWorld* World = GetWorld();
	check(World);

	const ENetMode CurrentWorldNetMode = World->GetNetMode();
	const double HasToDestroyAllActorsOnServerSide = CurrentWorldNetMode != NM_Client && CurrentWorldNetMode != NM_Standalone;
	const double TimeSliceEnd = FPlatformTime::Seconds() + MaxTimeSlicePerTick;

	{
		// Try release to pool actors or destroy them
		TRACE_CPUPROFILER_EVENT_SCOPE(DestroyActors);
		while ((DeactivatedActorsToDestroy.Num() || ActorsToDestroy.Num()) && 
			   (HasToDestroyAllActorsOnServerSide || FPlatformTime::Seconds() <= TimeSliceEnd))
		{
			AActor* ActorToDestroy = DeactivatedActorsToDestroy.Num() ? DeactivatedActorsToDestroy.Pop(/*bAllowShrinking*/false) : ActorsToDestroy.Pop(/*bAllowShrinking*/false);
			if (!ReleaseActorToPool(ActorToDestroy))
			{
				// Couldn't release actor back to pool, so destroy it
				World->DestroyActor(ActorToDestroy);
				--NumActorSpawned;
			}
		}
	}

	if(ActorsToDestroy.Num())
	{
		// Try release to pool remaining actors or deactivate them
		TRACE_CPUPROFILER_EVENT_SCOPE(DeactivateActors);
		for (AActor* ActorToDestroy : ActorsToDestroy)
		{
			if (!ReleaseActorToPool(ActorToDestroy))
			{
				// Couldn't release actor back to pool, do simple deactivate instead
				ActorToDestroy->SetActorEnableCollision(false);
				ActorToDestroy->SetActorHiddenInGame(true);
				ActorToDestroy->SetActorTickEnabled(false);
				DeactivatedActorsToDestroy.Add(ActorToDestroy);
			}
		}
		ActorsToDestroy.Reset();
	}
}

void UMassActorSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMassSimulationSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (const UWorld* World = GetWorld())
	{
		UMassSimulationSubsystem* SimSystem = UWorld::GetSubsystem<UMassSimulationSubsystem>(World);
		check(SimSystem);
		SimSystem->GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).AddUObject(this, &UMassActorSpawnerSubsystem::OnPrePhysicsPhaseStarted);
		SimSystem->GetOnProcessingPhaseFinished(EMassProcessingPhase::PrePhysics).AddUObject(this, &UMassActorSpawnerSubsystem::OnPrePhysicsPhaseFinished);
	}
}

void UMassActorSpawnerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (const UWorld* World = GetWorld())
	{
		if (const UMassSimulationSubsystem* SimSystem = UWorld::GetSubsystem<UMassSimulationSubsystem>(World))
		{
			SimSystem->GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).RemoveAll(this);
			SimSystem->GetOnProcessingPhaseFinished(EMassProcessingPhase::PrePhysics).RemoveAll(this);
		}
	}
}

void UMassActorSpawnerSubsystem::OnPrePhysicsPhaseStarted(const float DeltaSeconds)
{
	// Spawn actors before processors run so they can operate on deterministic actor state for the frame
	// 
	// Note: MassRepresentationProcessor relies on actor spawns being processed before it runs so it can confirm the
	// spawn and clean up the previous representation  
	ProcessPendingSpawningRequest(GET_MASSSIMULATION_CONFIG_VALUE(DesiredActorSpawningTimeSlicePerTick));

	CSV_CUSTOM_STAT(MassActors, NumSpawned, NumActorSpawned, ECsvCustomStatOp::Accumulate);
	CSV_CUSTOM_STAT(MassActors, NumPooled, NumActorPooled, ECsvCustomStatOp::Accumulate);
}

void UMassActorSpawnerSubsystem::OnPrePhysicsPhaseFinished(const float DeltaSeconds)
{
	// Destroy any actors queued for destruction this frame and hide any we didn't get to within the max processing time
	// 
	// Note: MassRepresentationProcessor relies on actor destruction processing after it runs so it can clean up
	// unwanted actor representations that it has replaced for this frame. It also relies on this running before physics
	// so unwanted representations don't interfere with new physics enabled actors 
	ProcessPendingDestruction(GET_MASSSIMULATION_CONFIG_VALUE(DesiredActorDestructionTimeSlicePerTick));
}

void UMassActorSpawnerSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMassActorSpawnerSubsystem* MASS = Cast<UMassActorSpawnerSubsystem>(InThis);
	if (MASS)
	{
		for (auto It = MASS->PooledActors.CreateIterator(); It; ++It)
		{
			TArray<AActor*>& Value = It.Value();

			Collector.AddReferencedObjects<AActor>(Value);
		}
	}

	Super::AddReferencedObjects(InThis, Collector);
}

void UMassActorSpawnerSubsystem::EnableActorPooling() 
{ 
	bActorPoolingEnabled = true; 
}

void UMassActorSpawnerSubsystem::DisableActorPooling() 
{
	bActorPoolingEnabled = false;

	ReleaseAllResources();

}

void UMassActorSpawnerSubsystem::ReleaseAllResources()
{
	if (UWorld* World = GetWorld())
	{
		for (auto It = PooledActors.CreateIterator(); It; ++It)
		{
			TArray<AActor*>& ActorArray = It.Value();
			for (int i = 0; i < ActorArray.Num(); i++)
			{
				World->DestroyActor(ActorArray[i]);
			}
			NumActorSpawned -= ActorArray.Num();
		}
	}
	PooledActors.Empty();

	NumActorPooled = 0;
	CSV_CUSTOM_STAT(MassActors, NumSpawned, NumActorSpawned, ECsvCustomStatOp::Accumulate);
	CSV_CUSTOM_STAT(MassActors, NumPooled, NumActorPooled, ECsvCustomStatOp::Accumulate);
}
