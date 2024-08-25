// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AuthorityManager.h"
#include "ConcertMessages.h"
#include "ConcertReplicationClient.h"
#include "Processing/ServerObjectReplicationReceiver.h"
#include "Replication/IConcertServerReplicationManager.h"
#include "Replication/Messages/Handshake.h"

#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"

class IConcertClientReplicationBridge;
class IConcertServerSession;

enum class EConcertQueryClientStreamFlags : uint8;

struct FConcertReplication_ChangeStream_Response;
struct FConcertReplication_QueryReplicationInfo_Response;
struct FConcertReplication_QueryReplicationInfo_Request;
struct FConcertAuthorityClientInfo;

namespace UE::ConcertSyncCore
{
	class IObjectReplicationFormat;
	class FObjectReplicationCache;
}

namespace UE::ConcertSyncServer::Replication
{
	class FAuthorityManager;

	/**
	 * Manages all server-side systems relevant to the Replication features.
	 * 
	 * Primarily responds to client requests to join (handshake) and leave replication delegating the result of the
	 * operation to relevant systems. 
	 */
	class FConcertServerReplicationManager
		: public IConcertServerReplicationManager
		, public IAuthorityManagerGetters
		, public FNoncopyable
	{
	public:

		explicit FConcertServerReplicationManager(TSharedRef<IConcertServerSession> InLiveSession);
		virtual ~FConcertServerReplicationManager() override;

		const FAuthorityManager& GetAuthorityManager() const { return AuthorityManager.Get(); }

		//~ Begin IAuthorityManagerGetters Interface
		virtual void ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback) const override;
		virtual void ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const override;
		//~ End IAuthorityManagerGetters Interface

	private:
		
		/** Session instance this manager was created for. */
		TSharedRef<IConcertServerSession> Session;
		
		/** Responsible for analysing received replication data. */
		TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat;

		/** Responds to client requests to changing authority and can be asked whether an object change is valid to take place. */
		TSharedRef<FAuthorityManager> AuthorityManager;
		
		/** Received replication events are put into the ReplicationCache. The cache is used to relay data to clients latently. */
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReplicationCache;
		/** Receives replication events from all endpoints. */
		FServerObjectReplicationReceiver ReplicationDataReceiver;

		/**
		 * Clients that have requested to join replication. Maps client ID to replication info.
		 * Clients are stored in a unique ptr to avoid dealing with reallocations when the map resizes.
		 */
		TMap<FGuid, TUniquePtr<FConcertReplicationClient>> Clients;

		// Joining
		EConcertSessionResponseCode HandleJoinReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_Join_Request& Request, FConcertReplication_Join_Response& Response);
		EConcertSessionResponseCode InternalHandleJoinReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_Join_Request& Request, FConcertReplication_Join_Response& Response);

		// Querying
		EConcertSessionResponseCode HandleQueryReplicationInfoRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_QueryReplicationInfo_Request& Request, FConcertReplication_QueryReplicationInfo_Response& Response);
		/** Gets all registered streams and optionally removes the properties. */
		static TArray<FConcertBaseStreamInfo> BuildClientStreamInfo(const FConcertReplicationClient& Client, EConcertQueryClientStreamFlags QueryFlags);
		/** Maps the client's streams to the objects in that stream the client has taken authority over. */
		TArray<FConcertAuthorityClientInfo> BuildClientAuthorityInfo(const FConcertReplicationClient& Client) const;

		// Changing streams
		EConcertSessionResponseCode HandleChangeStreamRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_ChangeStream_Request& Request, FConcertReplication_ChangeStream_Response& Response);

		// Leaving
		void HandleLeaveReplicationSessionRequest(const FConcertSessionContext& ConcertSessionContext, const FConcertReplication_LeaveEvent& EventData);
		void OnConnectionChanged(IConcertServerSession& ConcertServerSession, EConcertClientStatus ConcertClientStatus, const FConcertSessionClientInfo& ConcertSessionClientInfo);

		/**
		 * Ticks all clients which causes clients to process pending data and send it to the corresponding endpoints.
		 * 
		 * There is an internal time budget to ensure ticks do not starve the server's other tick tasks.
		 * This is configured in an .ini. TODO: Add config
		 */
		void Tick(IConcertServerSession& InSession, float InDeltaTime);
		
		/** Callback to clients for obtaining an object's frequency settings. */
		FConcertObjectReplicationSettings GetObjectFrequencySettings(const FConcertReplicatedObjectId& Object) const;
	};
}