// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetConnection.h"

class AActor;
class FArchive;

/**
 * Struct to store an actor pointer and any internal metadata for that actor used
 * internally by a UNetDriver.
 */
struct FNetworkObjectInfo
{
	/** Pointer to the replicated actor. */
	AActor* Actor;

	/** WeakPtr to actor. This is cached here to prevent constantly constructing one when needed for (things like) keys in TMaps/TSets */
	TWeakObjectPtr<AActor> WeakActor;

	/** Next time to consider replicating the actor. Based on FPlatformTime::Seconds(). */
	double NextUpdateTime;

	/** Last absolute time in seconds since actor actually sent something during replication */
	double LastNetReplicateTime;

	/** Optimal delta between replication updates based on how frequently actor properties are actually changing */
	float OptimalNetUpdateDelta;

	/** Last time this actor was updated for replication via NextUpdateTime
	* @warning: internal net driver time, not related to WorldSettings.TimeSeconds */
	double LastNetUpdateTimestamp;

	/** List of connections that this actor is dormant on */
	TSet<TWeakObjectPtr<UNetConnection>> DormantConnections;

	/** A list of connections that this actor has recently been dormant on, but the actor doesn't have a channel open yet.
	*  These need to be differentiated from actors that the client doesn't know about, but there's no explicit list for just those actors.
	*  (this list will be very transient, with connections being moved off the DormantConnections list, onto this list, and then off once the actor has a channel again)
	*/
	TSet<TWeakObjectPtr<UNetConnection>> RecentlyDormantConnections;

	/** Is this object still pending a full net update due to clients that weren't able to replicate the actor at the time of LastNetUpdateTime */
	uint8 bPendingNetUpdate : 1;

	/** Should this object be considered for replay checkpoint writes */
	uint8 bDirtyForReplay : 1;

	/** Should channel swap roles while calling ReplicateActor */
	uint8 bSwapRolesOnReplicate : 1;

	/** Force this object to be considered relevant for at least one update */
	uint32 ForceRelevantFrame = 0;

	FNetworkObjectInfo()
		: Actor(nullptr)
		, NextUpdateTime(0.0)
		, LastNetReplicateTime(0.0)
		, OptimalNetUpdateDelta(0.0f)
		, LastNetUpdateTimestamp(0.0)
		, bPendingNetUpdate(false)
		, bDirtyForReplay(false)
		, bSwapRolesOnReplicate(false) {}

	FNetworkObjectInfo(AActor* InActor)
		: Actor(InActor)
		, WeakActor(InActor)
		, NextUpdateTime(0.0)
		, LastNetReplicateTime(0.0)
		, OptimalNetUpdateDelta(0.0f) 
		, LastNetUpdateTimestamp(0.0)
		, bPendingNetUpdate(false)
		, bDirtyForReplay(false)
		, bSwapRolesOnReplicate(false) {}

	void CountBytes(FArchive& Ar) const;
};

/**
 * KeyFuncs to allow using the actor pointer as the comparison key in a set.
 */
struct FNetworkObjectKeyFuncs : BaseKeyFuncs<TSharedPtr<FNetworkObjectInfo>, AActor*, false>
{
	/**
	 * @return The key used to index the given element.
	 */
	static KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Get()->Actor;
	}

	/**
	 * @return True if the keys match.
	 */
	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}

	/** Calculates a hash index for a key. */
	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/**
 * Stores the list of replicated actors for a given UNetDriver.
 */
class ENGINE_API FNetworkObjectList
{
public:
	typedef TSet<TSharedPtr<FNetworkObjectInfo>, FNetworkObjectKeyFuncs> FNetworkObjectSet;

	/**
	 * Adds replicated actors in World to the internal set of replicated actors.
	 * Used when a net driver is initialized after some actors may have already
	 * been added to the world.
	 *
	 * @param World The world from which actors are added.
	 * @param NetDriver The net driver to which this object list belongs.
	 */
	void AddInitialObjects(UWorld* const World, UNetDriver* NetDriver);

	/**
	 * Attempts to find the Actor's FNetworkObjectInfo.
	 * If no info is found, then the Actor will be added to the list, and will assumed to be active.
	 *
	 * If the Actor is dormant when this is called, it is the responsibility of the caller to call
	 * MarkDormant immediately.
	 *
	 * If info cannot be found or created, nullptr will be returned.
	 */
	TSharedPtr<FNetworkObjectInfo>* FindOrAdd(AActor* const Actor, UNetDriver* NetDriver, bool* OutWasAdded=nullptr);

	/**
	 * Attempts to find the Actor's FNetworkObjectInfo.
	 *
	 * If info is not found (or the Actor is in an invalid state) an invalid TSharedPtr is returned.
	 */
	TSharedPtr<FNetworkObjectInfo> Find(AActor* const Actor);
	const TSharedPtr<FNetworkObjectInfo> Find(const AActor* const Actor) const
	{
		return const_cast<FNetworkObjectList*>(this)->Find(const_cast<AActor* const>(Actor));
	}

	/** Removes actor from the internal list, and any cleanup that is necessary (i.e. resetting dormancy state) */
	void Remove(AActor* const Actor);

	/** Marks this object as dormant for the passed in connection */
	void MarkDormant(AActor* const Actor, UNetConnection* const Connection, const int32 NumConnections, UNetDriver* NetDriver);

	/** Marks this object as active for the passed in connection */
	bool MarkActive(AActor* const Actor, UNetConnection* const Connection, UNetDriver* NetDriver);

	/** Marks this object dirty for replays using delta checkpoints */
	void MarkDirtyForReplay(AActor* const Actor);

	/** Removes the recently dormant status from the passed in connection */
	void ClearRecentlyDormantConnection(AActor* const Actor, UNetConnection* const Connection, UNetDriver* NetDriver);

	/** 
	 *	Does the necessary house keeping when a new connection is added 
	 *	When a new connection is added, we must add all objects back to the active list so the new connection will process it
	 *	Once the objects is dormant on that connection, it will then be removed from the active list again
	*/
	void HandleConnectionAdded();

	/** Clears all state related to dormancy */
	void ResetDormancyState();

	/** Returns a const reference to the entire set of tracked actors. */
	const FNetworkObjectSet& GetAllObjects() const { return AllNetworkObjects; }

	/** Returns a const reference to the active set of tracked actors. */
	const FNetworkObjectSet& GetActiveObjects() const { return ActiveNetworkObjects; }

	/** Returns a const reference to the entire set of dormant actors. */
	const FNetworkObjectSet& GetDormantObjectsOnAllConnections() const { return ObjectsDormantOnAllConnections; }

	int32 GetNumDormantActorsForConnection( UNetConnection* const Connection ) const;

	/** Force this actor to be relevant for at least one update */
	void ForceActorRelevantNextUpdate(AActor* const Actor, UNetDriver* NetDriver);
		
	void Reset();

	void CountBytes(FArchive& Ar) const;

	/** Marks any actors in the given package/level active if they were fully dormant or dormant for the passed in connection */
	void FlushDormantActors(UNetConnection* const Connection, const FName& PackageName);

private:
	bool MarkActiveInternal(const TSharedPtr<FNetworkObjectInfo>& ObjectInfo, UNetConnection* const Connection, UNetDriver* NetDriver);

	FNetworkObjectSet AllNetworkObjects;
	FNetworkObjectSet ActiveNetworkObjects;
	FNetworkObjectSet ObjectsDormantOnAllConnections;

	TMap<TWeakObjectPtr<UNetConnection>, int32 > NumDormantObjectsPerConnection;

	TMap<FName, FNetworkObjectSet> FullyDormantObjectsByLevel;
	TMap<TObjectKey<UNetConnection>, TMap<FName, FNetworkObjectSet>> DormantObjectsPerConnection;
};
