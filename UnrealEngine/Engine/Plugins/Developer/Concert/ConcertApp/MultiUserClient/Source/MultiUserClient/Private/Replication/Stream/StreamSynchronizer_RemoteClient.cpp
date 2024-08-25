// Copyright Epic Games, Inc. All Rights Reserved.

#include "StreamSynchronizer_RemoteClient.h"

#include "ConcertLogGlobal.h"
#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Util/RegularQueryService.h"

namespace UE::MultiUserClient
{
	constexpr float QueryTimeInterval = 1.f;
	
	FStreamSynchronizer_RemoteClient::FStreamSynchronizer_RemoteClient(const FGuid& RemoteEndpointId, FRegularQueryService& InQueryService)
		: QueryService(InQueryService)
		, QueryStreamHandle(
			QueryService.RegisterStreamQuery(
				RemoteEndpointId,
				FStreamQueryDelegate::CreateRaw(this, &FStreamSynchronizer_RemoteClient::HandleStreamQuery)
				)
			)
	{}

	FStreamSynchronizer_RemoteClient::~FStreamSynchronizer_RemoteClient()
	{
		QueryService.UnregisterStreamQuery(QueryStreamHandle);
	}

	FGuid FStreamSynchronizer_RemoteClient::GetStreamId() const
	{
		return UMultiUserReplicationClientPreset::MultiUserStreamID;
	}

	void FStreamSynchronizer_RemoteClient::HandleStreamQuery(const TArray<FConcertBaseStreamInfo>& Streams)
	{
		// MU clients use UMultiUserReplicationClientPreset::MultiUserStreamID for streams.
		// That handles the (unlikely) case in which some external logic had added streams to the same client which we must differentiate
		const FConcertBaseStreamInfo* MultiUserDescription = Streams.FindByPredicate([](const FConcertBaseStreamInfo& Description)
		{
			return Description.Identifier == UMultiUserReplicationClientPreset::MultiUserStreamID;
		});
		if (MultiUserDescription
			// Avoid unnecessary Broadcast()s
			&& LastKnownServerState != *MultiUserDescription)
		{
			LastKnownServerState = *MultiUserDescription;
			OnServerStateChangedDelegate.Broadcast();
		}
		// If the remote client removed the last object, the stream is implicitly deleted.
		else if (!MultiUserDescription && !LastKnownServerState.ReplicationMap.ReplicatedObjects.IsEmpty())
		{
			LastKnownServerState = {};
			OnServerStateChangedDelegate.Broadcast();
		}
	}
}
