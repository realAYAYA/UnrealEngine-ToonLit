// Copyright Epic Games, Inc. All Rights Reserved.

#include "ServerObjectReplicationReceiver.h"

#include "IConcertSessionHandler.h"
#include "Replication/AuthorityManager.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Messages/ObjectReplication.h"

namespace UE::ConcertSyncServer::Replication
{
	FServerObjectReplicationReceiver::FServerObjectReplicationReceiver(
		TSharedRef<FAuthorityManager> AuthorityManager,
		TSharedRef<IConcertSession> Session,
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache
		)
		: FObjectReplicationReceiver(MoveTemp(Session), MoveTemp(ReplicationCache))
		, AuthorityManager(MoveTemp(AuthorityManager))
	{}

	bool FServerObjectReplicationReceiver::ShouldAcceptObject(
		const FConcertSessionContext& SessionContext,
		const FConcertReplication_StreamReplicationEvent& StreamEvent,
		const FConcertReplication_ObjectReplicationEvent& ObjectEvent
		) const
	{
		const FConcertReplicatedObjectId ReplicatedObjectInfo { { StreamEvent.StreamId, ObjectEvent.ReplicatedObject }, SessionContext.SourceEndpointId };
		return AuthorityManager->HasAuthorityToChange(ReplicatedObjectInfo);
	}
}
