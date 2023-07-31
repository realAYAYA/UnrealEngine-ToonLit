// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_WITH_IRIS

#include "Iris/ReplicationSystem/NetHandle.h"
#include "Engine/EngineTypes.h"

class AActor;
class UActorComponent;
struct FActorBeginReplicationParams;
class UActorReplicationBridge;
class UObject;
class FRepChangedPropertyTracker;
class UReplicationSystem;
class UWorld;
class UNetConnection;

namespace UE::Net
{

// Helper methods to interact with ReplicationSystem from engine code
struct FReplicationSystemUtil
{
	/** Returns the UReplicationSystem the actor is replicated by, or null if it isn't. */
	ENGINE_API static UReplicationSystem* GetReplicationSystem(const AActor* Actor);

	/** Returns the UActorReplicationBridge of the UReplicationSystem the actor is replicated by, or null if it isn't. */
	ENGINE_API static UActorReplicationBridge* GetActorReplicationBridge(const AActor* Actor);

	/** Returns the UReplicationSystem of the UNetDriver the UNetConnection belongs to. */
	ENGINE_API static UActorReplicationBridge* GetActorReplicationBridge(const UNetConnection* NetConnection);

	/** Get the NetHandle for the actor. */
	ENGINE_API static FNetHandle GetNetHandle(const AActor* Actor);

	/** Get the NetHandle for an actor component. */
	ENGINE_API static FNetHandle GetNetHandle(const UActorComponent* SubObject);

	/** 
		Begins replication an actor and all of its registered subobjects returns a NetHandle for the actor if it succeeds. 
	*/
	ENGINE_API static FNetHandle BeginReplication(AActor* Actor, const FActorBeginReplicationParams& Params);

	/** 
		Begins replication an actor and all of its registered subobjects returns a NetHandle for the actor if it succeeds. 
	*/
	ENGINE_API static FNetHandle BeginReplication(AActor* Actor);
	
	/** Stop replicating an actor. Will destroy handle for actor and registered subobjects. */
	ENGINE_API static void EndReplication(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	/** Create NetHandle for actor component and add it as a subobject to the actor handle. */
	ENGINE_API static FNetHandle BeginReplicationForActorComponent(FNetHandle ActorHandle, UActorComponent* ActorComponent);

	/** Create NetHandle for actor component and add it as a subobject to the actor. */
	ENGINE_API static FNetHandle BeginReplicationForActorComponent(const AActor* Actor, UActorComponent* ActorComponent);

	/** Stop replicating an ActorComponent and its associated SubObjects. */
	ENGINE_API static void EndReplicationForActorComponent(UActorComponent* SubObject);

	/** Create NetHandle for SubObject and add it as a subobject to the actor. */
	ENGINE_API static FNetHandle BeginReplicationForActorSubObject(const AActor* Actor, UObject* SubObject, ELifetimeCondition NetCondition);

	/** Stop replicating a SubObject ActorComponent and its associated SubObjects. */
	ENGINE_API static void EndReplicationForActorSubObject(const AActor* Actor, UObject* SubObject);

	/** Create NetHandle for SubObject and add it as a subobject to the actor but only replicated if the ActorComponent replicates. */
	ENGINE_API static FNetHandle BeginReplicationForActorComponentSubObject(UActorComponent* ActorComponent, UObject* SubObject, ELifetimeCondition Condition);

	/** Stop replicating an ActorComponent and its associated SubObjects. */
	ENGINE_API static void EndReplicationForActorComponentSubObject(UActorComponent* ActorComponent, UObject* SubObject);
	
	/** 
	 * Set subobject NetCondition for a subobject, the conditions is used to determine if the SubObject should be replicated or not.
	 * @note: As the filtering is done at the serialization level it is typically more efficient to use a separate object for connection 
	 * specific data as filtering can be done at a higher level.
	 */
	ENGINE_API static void SetActorComponentNetCondition(const UActorComponent* SubObject, ELifetimeCondition Condition);

	/** Update group memberships used by COND_Group sub object filtering for the specified SubObject */
	ENGINE_API static void UpdateSubObjectGroupMemberships(const UObject* SubObject, const UWorld* World);

	/** Update replication status for all NetGroups used by COND_Group sub object filtering for the provided PlayerController */
	ENGINE_API static void UpdateSubObjectGroupMemberships(const APlayerController* PC);

	/** Update replication status for the PlayerController to not include the specified NetGroup */
	ENGINE_API static void RemoveSubObjectGroupMembership(const APlayerController* PC, const FName NetGroup);

	/**
	 * Adds a dependent actor. A dependent actor can replicate separately or if a parent replicates.
	 * Dependent actors cannot be filtered out by dynamic filtering unless the parent is also filtered out.
	 * @note There is no guarantee that the data will end up in the same packet so it is a very loose form of dependency.
	 */
	ENGINE_API static void AddDependentActor(const AActor* Parent, AActor* Child);

	/** Remove dependent actor from parent. The dependent actor will function as a standard standalone replicated actor. */
	ENGINE_API static void RemoveDependentActor(const AActor* Parent, AActor* Child);

	/** Begin replication for all networked actors in the world. */
	ENGINE_API static void BeginReplicationForActorsInWorld(UWorld* World);

	/** Notify the ReplicationSystem of a dormancy change. */
	ENGINE_API static void NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState);

	/** Trigger replication of dirty state for actor wanting to be dormant. */
	ENGINE_API static void FlushNetDormancy(AActor* Actor, bool bWasDormInitial);

private:
	friend FRepChangedPropertyTracker;

	ENGINE_API static void SetPropertyCustomCondition(const UObject* Object, uint16 RepIndex, bool bIsActive);
};

}

#endif
