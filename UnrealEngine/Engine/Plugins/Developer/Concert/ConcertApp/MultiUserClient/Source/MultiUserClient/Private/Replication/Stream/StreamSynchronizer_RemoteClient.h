// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientStreamSynchronizer.h"
#include "Replication/Data/ReplicationStream.h"

#include "Containers/Array.h"
#include "Templates/UnrealTemplate.h"

namespace UE::MultiUserClient
{
	class FRegularQueryService;
	
	/** Tracks the state of a remote client by querying the client's state in regular intervals. */
	class FStreamSynchronizer_RemoteClient : public IClientStreamSynchronizer, public FNoncopyable
	{
	public:
		
		FStreamSynchronizer_RemoteClient(const FGuid& RemoteEndpointId, FRegularQueryService& InQueryService);
		virtual ~FStreamSynchronizer_RemoteClient() override;

		//~ Begin IClientStreamSynchronizer Interface
		virtual FGuid GetStreamId() const override;
		virtual const FConcertObjectReplicationMap& GetServerState() const override { return LastKnownServerState.ReplicationMap; }
		virtual const FConcertStreamFrequencySettings& GetFrequencySettings() const override { return LastKnownServerState.FrequencySettings; }
		virtual FOnServerStateChanged& OnServerStateChanged() override { return OnServerStateChangedDelegate; }
		//~ End IClientStreamSynchronizer Interface

	private:

		/** Queries the server in regular intervals. This services outlives our object. */
		FRegularQueryService& QueryService;
		/** Used to unregister HandleStreamQuery upon destruction. */
		const FDelegateHandle QueryStreamHandle; 

		/** Represents what the local client thinks the replication map on the server currently looks like. */
		FConcertBaseStreamInfo LastKnownServerState;
		
		/** Event executed when the result of GetServerState has been synched. */
		FOnServerStateChanged OnServerStateChangedDelegate;

		/** Called in regular intervals with the contents of the remote client. */
		void HandleStreamQuery(const TArray<FConcertBaseStreamInfo>& Streams);
	};
}

