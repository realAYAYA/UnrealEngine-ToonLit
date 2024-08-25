// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerReplicationManager.h"
#include "Replication/IConcertServerReplicationManager.h"

class IConcertServerSession;

/**
 * Exposes functions that are required for testing.
 * 
 * These functions are technically exported but conceptually not part of the public interface
 * and should only be used for the purpose of automated testing.
 */
namespace UE::ConcertSyncServer::TestInterface
{
	CONCERTSYNCSERVER_API TSharedRef<Replication::IConcertServerReplicationManager> CreateServerReplicationManager(
		TSharedRef<IConcertServerSession> InLiveSession
		)
	{
		return MakeShared<Replication::FConcertServerReplicationManager>(MoveTemp(InLiveSession));
	}
}
