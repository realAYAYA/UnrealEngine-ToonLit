// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManagerUtils.h"

#include "Replication/IConcertClientReplicationManager.h"

#include "Algo/Transform.h"
#include "Containers/Set.h"

namespace UE::ConcertSyncClient::Replication
{
	TFuture<FConcertReplication_ChangeAuthority_Response> RejectAll(FConcertReplication_ChangeAuthority_Request&& Args)
	{
		return MakeFulfilledPromise<FConcertReplication_ChangeAuthority_Response>(
			FConcertReplication_ChangeAuthority_Response{ EReplicationResponseErrorCode::Handled, MoveTemp(Args.TakeAuthority) }
			).GetFuture();
	}
}
