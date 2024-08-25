// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Delegates/DelegateCombinations.h"
#include "Replication/Data/ObjectIds.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

struct FConcertReplication_ObjectReplicationEvent;
struct FSoftObjectPath;
struct FGuid;

namespace UE::ConcertSyncCore
{
	class IObjectReplicationFormat;

	/**
	 * Interface for objects that want to latently use received FConcertObjectReplicationEvent data.
	 * The data is continuously updated until it is consumed.
	 */
	class CONCERTSYNCCORE_API IReplicationCacheUser
	{
	public:

		/** @return Whether this user is interested in data from this object. */
		virtual bool WantsToAcceptObject(const FConcertReplicatedObjectId& Object) const = 0;

		/**
		 * Called when data that is interesting to this user becomes available.
		 *
		 * The user can keep hold of Data until it is used, at which point it just let's Data get out of scope.
		 * If new data is received while this user is referencing Data, Data will be combined to contain any new data.
		 */
		virtual void OnDataCached(const FConcertReplicatedObjectId& Object, TSharedRef<const FConcertReplication_ObjectReplicationEvent> Data) = 0;

		virtual ~IReplicationCacheUser() = default;
	};
	
	/**
	 * This is an intermediate place for received data to live before it is further processed.
	 *
	 * IObjectCacheUsers register with the cache and decide which data is to be received.
	 * When replication data comes in IObjectCacheUsers::WantsToAcceptObject is used to determine whether the object wants the data.
	 * If the data should be received, IObjectCacheUsers::OnDataCached is called receiving a shared ptr to the data.
	 * When the data is finally consumed latently, e.g. sent to other endpoints, the cache user resets the shared ptr.
	 * If new data comes in before a cache user consumes it, the new data and old data are combined (using IObjectReplicationFormat::CombineReplicationEvents). 
	 *
	 * This allows multiple systems to reuse replication data. For example, on the server the same data may need to be distributed to different clients but
	 * the clients send the data at different times.
	 */
	class CONCERTSYNCCORE_API FObjectReplicationCache : public TSharedFromThis<FObjectReplicationCache>
	{
	public:

		FObjectReplicationCache(TSharedRef<IObjectReplicationFormat> ReplicationFormat);

		/**
		 * Called when new data is received for an object and shares it with any IObjectCacheUser that is possibly interested in it.
		 * @param SendingEndpointId The ID of the client endpoint that sent this data
		 * @param OriginStreamId The stream from which the object was replicated
		 * @param ObjectReplicationEvent The data that was replicated
		 * @return The number of cache users that accepted this event. 
		 */
		int32 StoreUntilConsumed(const FGuid& SendingEndpointId, const FGuid& OriginStreamId, const FConcertReplication_ObjectReplicationEvent& ObjectReplicationEvent);

		/** Registers a new user, which will start receiving data for any new data received from now on. */
		void RegisterDataCacheUser(TSharedRef<IReplicationCacheUser> User);
		void UnregisterDataCacheUser(const TSharedRef<IReplicationCacheUser>& User);

	private:

		/** Used for combining events to save network bandwidth. */
		TSharedRef<IObjectReplicationFormat> ReplicationFormat;

		/** Everyone who registered for receiving data. */
		TArray<TSharedRef<IReplicationCacheUser>> CacheUsers;

		struct FObjectCache
		{
			/**
			 * Past data that is still in use by users.
			 * 
			 * StoreUntilConsumed asks all IReplicationCacheUser and if at least one is interested, creates exactly 1 TSharedRef<FConcertObjectReplicationEvent>.
			 * Every IReplicationCacheUser receives essentially a MakeShareable of the above instance: its own TSharedRef with a custom deleter that simply keeps the event instance.
			 *
			 * This mechanism allows detecting whether a IReplicationCacheUser already is using old data which needs to be combined with the new incoming data, or needs a new instance.
			 * The intention is that as soon as IReplicationCacheUser has finished using a data event, it will receive a
			 * new instance: we do not want IReplicationCacheUsers to have large histories of events being combined into them.
			 */
			TMap<TWeakPtr<IReplicationCacheUser>, TWeakPtr<FConcertReplication_ObjectReplicationEvent>> DataInUse;
		};
		/** Maps every object to the events cached for it. */
		TMap<FConcertObjectInStreamID, FObjectCache> Cache; 
	};
}