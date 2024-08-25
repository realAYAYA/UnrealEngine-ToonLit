// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IConcertClientSession;
class IConcertServerSession;
class IConcertClientReplicationBridge;
class IConcertClientReplicationManager;

namespace UE::ConcertSyncServer::Replication
{
	class IConcertServerReplicationManager;
}

namespace UE::ConcertSyncClient::TestInterface
{
	extern CONCERTSYNCCLIENT_API TSharedRef<IConcertClientReplicationManager> CreateClientReplicationManager(
		TSharedRef<IConcertClientSession> InLiveSession,
		IConcertClientReplicationBridge* InBridge
		);

	extern CONCERTSYNCCLIENT_API TSharedRef<IConcertClientReplicationBridge> CreateClientReplicationBridge();
}

namespace UE::ConcertSyncServer::TestInterface
{
	extern CONCERTSYNCSERVER_API TSharedRef<Replication::IConcertServerReplicationManager> CreateServerReplicationManager(
		TSharedRef<IConcertServerSession> InLiveSession
		);
}