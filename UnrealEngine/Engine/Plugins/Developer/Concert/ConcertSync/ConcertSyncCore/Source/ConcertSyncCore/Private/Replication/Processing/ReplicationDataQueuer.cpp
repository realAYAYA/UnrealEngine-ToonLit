// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ReplicationDataQueuer.h"

#include "Replication/Messages/ObjectReplication.h"

namespace UE::ConcertSyncCore
{
	FReplicationDataQueuer::~FReplicationDataQueuer()
	{
		if (ensure(ReplicationCache))
		{
			ReplicationCache->UnregisterDataCacheUser(AsShared());
		}
	}

	void FReplicationDataQueuer::ForEachPendingObject(TFunctionRef<void(const FConcertReplicatedObjectId&)> ProcessItemFunc) const
	{
		for (auto It = PendingEvents.CreateConstIterator(); It; ++It)
		{
			ProcessItemFunc(It->Key);
		}
	}

	int32 FReplicationDataQueuer::NumObjects() const
	{
		return PendingEvents.Num();
	}

	bool FReplicationDataQueuer::ExtractReplicationDataForObject(
		const FConcertReplicatedObjectId& Object,
		TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable,
		TFunctionRef<void(FConcertSessionSerializedPayload&& Payload)> ProcessMoveable
		)
	{
		TSharedPtr<const FConcertReplication_ObjectReplicationEvent> Event;
		const bool bSuccess = PendingEvents.RemoveAndCopyValue(Object, Event);
		if (!ensureMsgf(bSuccess, TEXT("ExtractReplicationDataForObject for an item that was not returned by ForEachPendingObject")))
		{
			return false;
		}

		// Event may be shared by other FReplicationDataQueuers since it originates from the replication cache, so sadly no move.
		ProcessCopyable(Event->SerializedPayload);
		return true;
	}

	void FReplicationDataQueuer::OnDataCached(const FConcertReplicatedObjectId& Object, TSharedRef<const FConcertReplication_ObjectReplicationEvent> Data)
	{
		PendingEvents.Add(Object, MoveTemp(Data));
	}
	
	void FReplicationDataQueuer::BindToCache(TSharedRef<FObjectReplicationCache> InReplicationCache)
	{
		if (ensure(!ReplicationCache))
		{
			ReplicationCache = MoveTemp(InReplicationCache);
			ReplicationCache->RegisterDataCacheUser(AsShared());
		}
	}
}
