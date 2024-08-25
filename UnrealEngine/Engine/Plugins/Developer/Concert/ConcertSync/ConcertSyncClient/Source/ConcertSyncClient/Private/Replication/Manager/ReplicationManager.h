// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/Messages/Handshake.h"
#include "Templates/SharedPointer.h"

class IConcertClientReplicationBridge;
class IConcertClientSession;

namespace UE::ConcertSyncClient::Replication
{
	class FReplicationManagerState;
	
	class FReplicationManager : public IConcertClientReplicationManager
	{
		friend FReplicationManagerState; 
	public:
		
		FReplicationManager(TSharedRef<IConcertClientSession> InLiveSession, IConcertClientReplicationBridge* InBridge);
		virtual ~FReplicationManager() override;

		/** Starts accepting join requests. Must be called separately from constructor because of TSharedFromThis asserting if SharedThis is called in constructor. */
		void StartAcceptingJoinRequests();

		//~ Begin IConcertClientReplicationManager Interface
		virtual TFuture<FJoinReplicatedSessionResult> JoinReplicationSession(FJoinReplicatedSessionArgs Args) override;
		virtual void LeaveReplicationSession() override;
		virtual bool CanJoin() override;
		virtual bool IsConnectedToReplicationSession() override;
		virtual EStreamEnumerationResult ForEachRegisteredStream(TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback) const override;
		virtual TFuture<FConcertReplication_ChangeAuthority_Response> RequestAuthorityChange(FConcertReplication_ChangeAuthority_Request Args) override;
		virtual TFuture<FConcertReplication_QueryReplicationInfo_Response> QueryClientInfo(FConcertReplication_QueryReplicationInfo_Request Args) override;
		virtual TFuture<FConcertReplication_ChangeStream_Response> ChangeStream(FConcertReplication_ChangeStream_Request Args) override;
		virtual EAuthorityEnumerationResult ForEachClientOwnedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)>) const override;
		virtual TSet<FGuid> GetClientOwnedStreamsForObject(const FSoftObjectPath& ObjectPath) const override;
		virtual FOnPreStreamsChanged& OnPreStreamsChanged() override;
		virtual FOnPostStreamsChanged& OnPostStreamsChanged() override;
		virtual FOnPreAuthorityChanged& OnPreAuthorityChanged() override;
		virtual FOnPostAuthorityChanged& OnPostAuthorityChanged() override;
		//~ End IConcertClientReplicationManager Interface

	private:
		
		/** Session instance this manager was created for. */
		TSharedRef<IConcertClientSession> Session;
		/** The replication bridge is responsible for applying received data and generating data to send. */
		IConcertClientReplicationBridge* Bridge;

		/** The current state this manager is in, e.g. waiting for connection request, connecting, connected, etc. */
		TSharedPtr<FReplicationManagerState> CurrentState;
		
		/** Called by FReplicationManagerState to change the state. */
		void OnChangeState(TSharedRef<FReplicationManagerState> NewState);
	};
}
