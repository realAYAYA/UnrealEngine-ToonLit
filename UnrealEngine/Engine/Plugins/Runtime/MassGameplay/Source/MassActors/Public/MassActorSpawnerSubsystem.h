// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStruct.h"
#include "IndexedHandle.h"
#include "Delegates/Delegate.h"
#include "MassCommonTypes.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"

#include "MassActorSpawnerSubsystem.generated.h"

// Handle for an actor spawning request
USTRUCT()
struct MASSACTORS_API FMassActorSpawnRequestHandle : public FIndexedHandleBase
{
	GENERATED_BODY()

	FMassActorSpawnRequestHandle() = default;

	/** @note passing INDEX_NONE as index will make this handle Invalid */
	FMassActorSpawnRequestHandle(const int32 InIndex, const uint32 InSerialNumber) : FIndexedHandleBase(InIndex, InSerialNumber)
	{
	}
};

// Managing class of spawning requests handles
typedef FIndexedHandleManager<FMassActorSpawnRequestHandle, true/*bOptimizeHandleReuse*/> FMassEntityHandleManager_ActorSpawnRequest;


DECLARE_DELEGATE_TwoParams(FMassActorPreSpawnDelegate, const FMassActorSpawnRequestHandle& SpawnRequestHandle, FConstStructView SpawnRequest);
enum class EMassActorSpawnRequestAction : uint8
{
	Keep, // Will leave spawning request in the queue and it will be users job to call RemoveActorSpawnRequest
	Remove, // Will remove the spawning request from the queue once the callback ends
};
DECLARE_DELEGATE_RetVal_TwoParams(EMassActorSpawnRequestAction, FMassActorPostSpawnDelegate, const FMassActorSpawnRequestHandle& SpawnRequestHandle, FConstStructView SpawnRequest);

UENUM()
enum class ESpawnRequestStatus : uint8
{
	None, // Not in the queue to be spawn
	Pending, // Still in the queue to be spawn
	Processing, // in the process of spawning the actor
	Succeeded, // Successfully spawned the actor
	Failed, // Error while spawning the actor
	RetryPending, // Failed spawn request that are being retried (lower priority)
};

/**
 * Base class for all spawn request
 */
USTRUCT()
struct MASSACTORS_API FMassActorSpawnRequest
{
	GENERATED_BODY()
public:
	/** The actual mass agent handle corresponding to the actor to spawn */
	FMassEntityHandle MassAgent;

	/** The template BP actor to spawn */
	UPROPERTY(Transient)
	TSubclassOf<AActor> Template;

	/** The location of where to spawn that actor */
	FTransform	Transform;

	/** Priority of this spawn request in comparison with the others, the lower the value is, the higher the priority is */
	float Priority = MAX_FLT;

	/** Delegate that will be called just before the spawning an actor, giving the chance to the processor to prepare it */
	FMassActorPreSpawnDelegate ActorPreSpawnDelegate;

	/** Delegate that will be called once the spawning is done */
	FMassActorPostSpawnDelegate ActorPostSpawnDelegate;

	/** The current status of the spawn request */
	ESpawnRequestStatus SpawnStatus = ESpawnRequestStatus::None;

	/** The pointer to the actor once it is spawned */
	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedActor = nullptr;

	/** Internal request serial number (used to cycle through next spawning request) */
	uint32 SerialNumber = 0;

	/** Requested world time seconds */
	float RequestedTime = 0.0f;

	void Reset()
	{
		MassAgent = FMassEntityHandle();
		Template = nullptr;
		Priority = MAX_FLT;
		ActorPreSpawnDelegate.Unbind();
		ActorPostSpawnDelegate.Unbind();
		SpawnStatus = ESpawnRequestStatus::None;
		SpawnedActor = nullptr;
		SerialNumber = 0;
		RequestedTime = 0.0f;
	}
};

/**
 * A subsystem managing spawning of actors for all mass subsystems
 */
UCLASS()
class MASSACTORS_API UMassActorSpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:

	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// USubsystem END

	/** Request an actor to spawn
	 * Note: If you do not provide a spawn delegate, the requester is responsible to remove the request by hand.
	 * It will be auto removed after the execution of the spawn delegate.
	 * @param InSpawnRequest the spawn request parameters, You can provide any type of UStruct as long at it derives from FMassActorSpawnRequest. This let you add more spawning information.
	 */
	template< typename T, typename = typename TEnableIf<TIsDerivedFrom<typename TRemoveReference<T>::Type, FMassActorSpawnRequest>::IsDerived, void>::Type >
	FMassActorSpawnRequestHandle RequestActorSpawn(const T& InSpawnRequest)
	{
		return RequestActorSpawnInternal(FConstStructView::Make(InSpawnRequest));
	}

	/** Retries a failed spawn request
	 * @param SpawnRequestHandle the spawn request handle to retry
	 */
	void RetryActorSpawnRequest(const FMassActorSpawnRequestHandle SpawnRequestHandle);

	/**
	 * Removes a spawn request
	 * The only time a spawn request cannot be removed is when its state is processing
	 * Also spawn requests are auto removed if you provided a spawn delegate after it was being executed.
	 * @param SpawnRequestHandle [IN/OUT] the spawn request handle to remove
	 * @return true if successfully removed the request
	 */
	 bool RemoveActorSpawnRequest(FMassActorSpawnRequestHandle& SpawnRequestHandle);

	/**
	 * Returns the stored spawn request from the handle, useful to update the transform
	 * @param SpawnRequestHandle the spawn request handle to get the request from
	 * @return The spawn request cast in the provided template argument
	 */
	template<typename T>
	const T& GetSpawnRequest(const FMassActorSpawnRequestHandle SpawnRequestHandle) const
	{
		check(SpawnRequestHandleManager.IsValidHandle(SpawnRequestHandle));
		check(SpawnRequests.IsValidIndex(SpawnRequestHandle.GetIndex()));
		return SpawnRequests[SpawnRequestHandle.GetIndex()].Get<T>();
	}

	/**
	 * Returns the stored spawn request from the handle, useful to update the transform
	 * @param SpawnRequestHandle the spawn request handle to get the request from
	 * @return The spawn request cast in the provided template argument
	 */
	template<typename T> 
	T& GetMutableSpawnRequest(const FMassActorSpawnRequestHandle SpawnRequestHandle)
	{
		check(SpawnRequestHandleManager.IsValidHandle(SpawnRequestHandle));
		check(SpawnRequests.IsValidIndex(SpawnRequestHandle.GetIndex()));
		return SpawnRequests[SpawnRequestHandle.GetIndex()].GetMutable<T>();
	}

	/**
	 * Destroy an actor 
	 * @param Actor to destroy
	 * @param bImmediate to do the destruction immediately, otherwise will be queued up for later
	 */
	void DestroyActor(AActor* Actor, bool bImmediate = false);
	
	void EnableActorPooling();
	void DisableActorPooling();

	void ReleaseAllResources();

protected:

	/** Called at the start of the PrePhysics mass processing phase and calls ProcessPendingSpawningRequest */ 
	void OnPrePhysicsPhaseStarted(const float DeltaSeconds);
	
	/** Called at the end of the PrePhysics mass processing phase and calls ProcessPendingDestruction */ 
	void OnPrePhysicsPhaseFinished(const float DeltaSeconds); 
	
	/** Retrieve what would be the next best spawning request to spawn, can be overridden to have different logic
	 *  Default implementation is the first valid request in the list, no interesting logic yet
	 *  @return the next best handle to spawn. */
	virtual FMassActorSpawnRequestHandle GetNextRequestToSpawn() const;

	virtual AActor* SpawnOrRetrieveFromPool(FConstStructView SpawnRequest);

	/** Actual code that will spawn the actor, overridable by subclass if need to be.
	 *  @return spawned actor if succeeded. */
	virtual AActor* SpawnActor(FConstStructView SpawnRequest) const;

	/** Go through the spawning request and spawn them until we reach the budget 
	 * @param MaxTimeSlicePerTick is the budget in seconds allowed to do spawning */
	void ProcessPendingSpawningRequest(const double MaxTimeSlicePerTick);

	/** Go through the queued actors to destroy and destroy them until we reach the budget
	 * @param MaxTimeSlicePerTick is the budget in seconds allowed to do destruction */
	void ProcessPendingDestruction(const double MaxTimeSlicePerTick);

	/** Try releasing this actor to pool if possible 
	 * @param Actor to release to the bool
	 * @return true if the actor was actually released to the pool */
	bool ReleaseActorToPool(AActor* Actor);

	/** Internal generic request actor spawn to make sure the request derives from FMassActorSpawnRequest 
	 *  @param SpawnRequest the spawn request parameters, We are allowing any type of spawn request, let's store it internally as a FInstancedStruct. This parameter is the FStructView over provide user struct */
	FMassActorSpawnRequestHandle RequestActorSpawnInternal(const FConstStructView SpawnRequest);

protected:

	UPROPERTY()
	TArray<FInstancedStruct> SpawnRequests;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> ActorsToDestroy;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> DeactivatedActorsToDestroy;

	bool bActorPoolingEnabled = true;

	TMap<TSubclassOf<AActor>, TArray<AActor*>> PooledActors;

	FMassEntityHandleManager_ActorSpawnRequest SpawnRequestHandleManager;

	std::atomic<uint32> RequestSerialNumberCounter;

	mutable int32 NumActorSpawned = 0;
	mutable int32 NumActorPooled = 0;
};
