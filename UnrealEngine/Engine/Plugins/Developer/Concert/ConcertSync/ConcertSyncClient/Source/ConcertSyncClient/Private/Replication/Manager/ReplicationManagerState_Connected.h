// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationManagerState.h"
#include "Replication/Processing/ClientReplicationDataCollector.h"
#include "Replication/Processing/ClientReplicationDataQueuer.h"
#include "Replication/Processing/ObjectReplicationSender.h"
#include "Replication/Processing/Proxy/ObjectProcessorProxy_Frequency.h"

class IConcertClientSession;

namespace UE::ConcertSyncCore
{
	class FObjectReplicationReceiver;
}

namespace UE::ConcertSyncClient::Replication
{
	class FObjectReplicationApplierProcessor;
	class FObjectReplicationSender;
	
	/**
	 * State for when the client has successfully completed a replication handshake.
	 *
	 * Every tick this state tries to
	 *	- collect data and sends it to the server
	 *	- process received data and applies it
	 */
	class FReplicationManagerState_Connected : public FReplicationManagerState
	{
	public:

		FReplicationManagerState_Connected(
			TSharedRef<IConcertClientSession> LiveSession,
			IConcertClientReplicationBridge* ReplicationBridge,
			TArray<FConcertReplicationStream> StreamDescriptions,
			FReplicationManager& Owner
			);
		virtual ~FReplicationManagerState_Connected() override;

		//~ Begin IConcertClientReplicationManager Interface
		virtual TFuture<FJoinReplicatedSessionResult> JoinReplicationSession(FJoinReplicatedSessionArgs Args) override;
		virtual void LeaveReplicationSession() override;
		virtual bool CanJoin() override { return false; }
		virtual bool IsConnectedToReplicationSession() override { return true; }
		virtual EStreamEnumerationResult ForEachRegisteredStream(TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback) const override;
		virtual TFuture<FConcertReplication_ChangeAuthority_Response> RequestAuthorityChange(FConcertReplication_ChangeAuthority_Request Args) override;
		virtual TFuture<FConcertReplication_QueryReplicationInfo_Response> QueryClientInfo(FConcertReplication_QueryReplicationInfo_Request Args) override;
		virtual TFuture<FConcertReplication_ChangeStream_Response> ChangeStream(FConcertReplication_ChangeStream_Request Args) override;
		virtual EAuthorityEnumerationResult ForEachClientOwnedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)> Callback) const override;
		virtual TSet<FGuid> GetClientOwnedStreamsForObject(const FSoftObjectPath& ObjectPath) const override;
		//~ End IConcertClientReplicationManager Interface

	private:

		/** Passed to FReplicationManagerState_Disconnected */
		const TSharedRef<IConcertClientSession> LiveSession;
		/** Passed to FReplicationManagerState_Disconnected */
		IConcertClientReplicationBridge* const ReplicationBridge;
		/** The streams this client has registered with the server. */
		TArray<FConcertReplicationStream> RegisteredStreams;
		
		/** The format this client will use for sending & receiving data. */
		const TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat;

		// Sending
		/** Used as source of replication data. */
		const TSharedRef<FClientReplicationDataCollector> ReplicationDataSource;
		
		/** Sends to remote endpoint and makes sure the objects are replicated at the specified frequency settings. */
		using FDataRelayThrottledByFrequency = ConcertSyncCore::TObjectProcessorProxy_Frequency<ConcertSyncCore::FObjectReplicationSender>;
		/** Sends data collected by ReplicationDataSource to the server. */
		FDataRelayThrottledByFrequency Sender;

		// Receiving
		/** Stores data received by Receiver until it is consumed by ReceivedReplicationQueuer. */
		const TSharedRef<ConcertSyncCore::FObjectReplicationCache> ReceivedDataCache;
		/** Receives data from remote endpoints via message bus.  */
		const TSharedRef<ConcertSyncCore::FObjectReplicationReceiver> Receiver;
		/** Queues data until is can be processed. */
		const TSharedRef<FClientReplicationDataQueuer> ReceivedReplicationQueuer;
		/** Processes data from ReceivedReplicationQueuer once we tick. */
		const TSharedRef<FObjectReplicationApplierProcessor> ReplicationApplier;

		//~ Begin FReplicationManagerState Interface
		virtual void OnEnterState() override;
		//~ End FReplicationManagerState Interface

		/**
		 * Ticks this client.
		 * 
		 * This processes:
		 *  - data that is to be sent
		 *  - data that was received
		 *
		 * The tasks have a time budget so that the frame rate remains stable.
		 * It is configured in the project settings TODO: Add config
		 */
		void Tick(IConcertClientSession& Session, float DeltaTime);
		
		/** Updates replicated objects affected by the change request. */
		void UpdateReplicatedObjectsAfterStreamChange(const FConcertReplication_ChangeStream_Request& Request, const FConcertReplication_ChangeStream_Response& Response);
		void HandleRemovingReplicatedObjects(const FConcertReplication_ChangeStream_Request& Request) const;
		void RevertRemovingReplicatedObjects(const FConcertReplication_ChangeStream_Request& Request) const;

		/**
		 * Updates the objects which should be replicated after changing authority.
		 * 
		 * @note Request is accepted as && because this function rewrites its memory when looking at rejections.
		 * Since the request was already sent to the server it is assumed the request can just contain trash after.
		 */
		void UpdateReplicatedObjectsAfterAuthorityChange(FConcertReplication_ChangeAuthority_Request&& Request, const FConcertReplication_ChangeAuthority_Response& Response) const;
		void HandleReleasingReplicatedObjects(const FConcertReplication_ChangeAuthority_Request& Request) const;
		void RevertReleasingReplicatedObjects(const FConcertReplication_ChangeAuthority_Request& Request) const;
		
		/** Callback to Sender for obtaining an object's frequency settings. */
		FConcertObjectReplicationSettings GetObjectFrequencySettings(const FConcertReplicatedObjectId& Object) const;
	};
}
