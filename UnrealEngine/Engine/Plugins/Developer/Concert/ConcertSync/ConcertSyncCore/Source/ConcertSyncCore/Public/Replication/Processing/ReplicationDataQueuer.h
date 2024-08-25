// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationDataSource.h"
#include "ObjectReplicationCache.h"

namespace UE::ConcertSyncCore
{
	/**
	 * Holds on to events received from remote endpoints and exposes them as a source.
	 * 
	 * Received events are received from a FObjectReplicationCache, which makes sure that event data is shared effectively
	 * if you created multiple FReplicationDataQueuer based on the same cache. This is relevant server side, where a
	 * FReplicationDataQueuer is created for each client.
	 * 
	 * Child classes need to implement IReplicationCacheUser::WantsToAcceptObject.
	 * TODO: Remove object data if the object is is meant for becomes unavailable.
	 */
	class CONCERTSYNCCORE_API FReplicationDataQueuer
		: public IReplicationDataSource
		, public IReplicationCacheUser
		, public TSharedFromThis<FReplicationDataQueuer>
	{
	public:

		virtual ~FReplicationDataQueuer() override;

		//~ Begin IReplicationDataSource Interface
		virtual void ForEachPendingObject(TFunctionRef<void(const FConcertReplicatedObjectId&)> ProcessItemFunc) const override;
		virtual int32 NumObjects() const override;
		virtual bool ExtractReplicationDataForObject(const FConcertReplicatedObjectId& Object, TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable, TFunctionRef<void(FConcertSessionSerializedPayload&& Payload)> ProcessMoveable) override;
		//~ End IReplicationDataSource Interface
		
		//~ Begin IReplicationCacheUser Interface
		virtual void OnDataCached(const FConcertReplicatedObjectId& Object, TSharedRef<const FConcertReplication_ObjectReplicationEvent> Data) override;
		//~ End IReplicationCacheUser Interface

	protected:

		/** Called by subclass factory functions. */
		void BindToCache(TSharedRef<FObjectReplicationCache> InReplicationCache);
		
	private:

		/** Stores events as they are received. */
		TMap<FConcertReplicatedObjectId, TSharedPtr<const FConcertReplication_ObjectReplicationEvent>> PendingEvents;

		/** Provides us with replication events and shares them effectively. */
		TSharedPtr<FObjectReplicationCache> ReplicationCache;
	};
}
