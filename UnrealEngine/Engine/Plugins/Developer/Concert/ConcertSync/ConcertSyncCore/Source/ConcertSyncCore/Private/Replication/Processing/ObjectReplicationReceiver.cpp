// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationReceiver.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/Messages/ObjectReplication.h"
#include "Replication/Processing/ObjectReplicationCache.h"

#include "HAL/IConsoleManager.h"

namespace UE::ConcertSyncCore
{
	static TAutoConsoleVariable<bool> CVarLogReceivedObjects(TEXT("Concert.Replication.LogReceivedObjects"), false, TEXT("Enable Concert logging for received replicated objects."));
	
	FObjectReplicationReceiver::FObjectReplicationReceiver(TSharedRef<IConcertSession> Session, TSharedRef<FObjectReplicationCache> ReplicationCache)
		: Session(MoveTemp(Session))
		, ReplicationCache(MoveTemp(ReplicationCache))
	{
		Session->RegisterCustomEventHandler<FConcertReplication_BatchReplicationEvent>(this, &FObjectReplicationReceiver::HandleBatchReplicationEvent);
	}

	FObjectReplicationReceiver::~FObjectReplicationReceiver()
	{
		Session->UnregisterCustomEventHandler<FConcertReplication_BatchReplicationEvent>(this);
	}

	void FObjectReplicationReceiver::HandleBatchReplicationEvent(const FConcertSessionContext& SessionContext, const FConcertReplication_BatchReplicationEvent& Event)
	{
		// Fyi: an object may have multiple changes in a batch replication event: each stream can modify different properties as long as they do not overlap.
		int32 NumObjects = 0;
		int32 NumRejectedObjectChanges = 0;
		int32 NumCacheUsages = 0;
		int32 NumOfAcceptedObjectChanges = 0;
		
		for (const FConcertReplication_StreamReplicationEvent& StreamEvent : Event.Streams)
		{
			const int32 ObjectsInStream = StreamEvent.ReplicatedObjects.Num();
			NumObjects += ObjectsInStream;
			
			for (const FConcertReplication_ObjectReplicationEvent& ObjectEvent : StreamEvent.ReplicatedObjects)
			{
				if (ShouldAcceptObject(SessionContext, StreamEvent, ObjectEvent))
				{
					const int32 NumAccepted = ReplicationCache->StoreUntilConsumed(SessionContext.SourceEndpointId, StreamEvent.StreamId, ObjectEvent);
					NumCacheUsages += NumAccepted;
					NumOfAcceptedObjectChanges += NumAccepted == 0 ? 0 : 1;
				}
				else
				{
					++NumRejectedObjectChanges;
				}
			}
		}

		if (CVarLogReceivedObjects.GetValueOnGameThread())
		{
			UE_LOG(LogConcert, Log, TEXT("Received %d streams with %d object changes from endpoint %s. Cached %d object changes with a total of %d cache usages."),
				Event.Streams.Num(),
				NumObjects,
				*SessionContext.SourceEndpointId.ToString(),
				NumOfAcceptedObjectChanges,
				NumCacheUsages
			);
			UE_CLOG(NumRejectedObjectChanges > 0, LogConcert, Warning, TEXT("Rejected %d object changes."), NumRejectedObjectChanges);
		}
	}
}
