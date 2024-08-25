// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationSender.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/Processing/IReplicationDataSource.h"

#include "Algo/Accumulate.h"
#include "HAL/IConsoleManager.h"

namespace UE::ConcertSyncCore
{
	static TAutoConsoleVariable<bool> CVarLogSentObjects(TEXT("Concert.Replication.LogSentObjects"), false, TEXT("Enable Concert logging for sent replicated objects."));
	
	FObjectReplicationSender::FObjectReplicationSender(
		const FGuid& TargetEndpointId,
		TSharedRef<IConcertSession> Session,
		TSharedRef<IReplicationDataSource> DataSource
		)
		: FObjectReplicationProcessor(MoveTemp(DataSource))
		, TargetEndpointId(TargetEndpointId)
		, Session(MoveTemp(Session))
	{}

	void FObjectReplicationSender::ProcessObjects(const FProcessObjectsParams& Params)
	{
		FObjectReplicationProcessor::ProcessObjects(Params);

		if (!EventToSend.Streams.IsEmpty())
		{
			const int32 NumObjects = Algo::TransformAccumulate(EventToSend.Streams, [](const FConcertReplication_StreamReplicationEvent& Event){ return Event.ReplicatedObjects.Num(); }, 0);
			UE_CLOG(CVarLogSentObjects.GetValueOnGameThread(), LogConcert, Log, TEXT("Sending %d streams with %d objects to %s"),
				EventToSend.Streams.Num(),
				NumObjects,
				*TargetEndpointId.ToString()
				);
			
			Session->SendCustomEvent(EventToSend, TargetEndpointId,
				// Replication is always unreliable - if it fails to deliver we'll send updated data soon again
				// TODO: In regular intervals send CRC values to detect that a change is missing
				EConcertMessageFlags::None
			);
			EventToSend.Streams.Empty(
				// It's not unreasonable to expect the next pass to have a similar number of objects so keep it around to avoid re-allocating all the time
				EventToSend.Streams.Num()
				);
		}
	}

	void FObjectReplicationSender::ProcessObject(const FObjectProcessArgs& Args)
	{
		const FSoftObjectPath& ReplicatedObject = Args.ObjectInfo.Object;
		auto CaptureData = [this, &Args, &ReplicatedObject]<typename TPayloadRefType>(TPayloadRefType&& Payload)
		{
			const int32 PreexistingIndex = EventToSend.Streams.IndexOfByPredicate([&Args](const FConcertReplication_StreamReplicationEvent& StreamData){ return StreamData.StreamId == Args.ObjectInfo.StreamId; });
			FConcertReplication_StreamReplicationEvent& StreamData = EventToSend.Streams.IsValidIndex(PreexistingIndex)
				? EventToSend.Streams[PreexistingIndex]
				: EventToSend.Streams[EventToSend.Streams.Emplace(Args.ObjectInfo.StreamId)];
			StreamData.ReplicatedObjects.Add({
				ReplicatedObject,
				// Take advantage of move semantics if it is possible - this depends on how our data source internally obtains its payloads
				Forward<TPayloadRefType>(Payload)
			});
		};
		GetDataSource().ExtractReplicationDataForObject(Args.ObjectInfo, CaptureData, CaptureData);
	}
}
