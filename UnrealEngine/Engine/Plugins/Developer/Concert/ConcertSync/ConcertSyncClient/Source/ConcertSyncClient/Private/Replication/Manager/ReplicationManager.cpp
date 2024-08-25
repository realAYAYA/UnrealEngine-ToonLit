// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManager.h"

#include "IConcertSession.h"
#include "ReplicationManagerState_Disconnected.h"
#include "ReplicationManagerUtils.h"

#include "Replication/Messages/Handshake.h"

namespace UE::ConcertSyncClient::Replication
{
	FReplicationManager::FReplicationManager(TSharedRef<IConcertClientSession> InLiveSession, IConcertClientReplicationBridge* InBridge)
		: Session(MoveTemp(InLiveSession))
		, Bridge(InBridge)
	{}

	FReplicationManager::~FReplicationManager()
	{}

	void FReplicationManager::StartAcceptingJoinRequests()
	{
		checkSlow(!CurrentState.IsValid());
		CurrentState = MakeShared<FReplicationManagerState_Disconnected>(Session, Bridge, *this);
	}

	TFuture<FJoinReplicatedSessionResult> FReplicationManager::JoinReplicationSession(FJoinReplicatedSessionArgs Args)
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->JoinReplicationSession(MoveTemp(Args))
			: MakeFulfilledPromise<FJoinReplicatedSessionResult>(EJoinReplicationErrorCode::Cancelled).GetFuture();
	}

	void FReplicationManager::LeaveReplicationSession()
	{
		if (ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point.")))
		{
			CurrentState->LeaveReplicationSession();
		}
	}

	bool FReplicationManager::CanJoin()
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			&& CurrentState->CanJoin(); 
	}

	bool FReplicationManager::IsConnectedToReplicationSession()
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			&& CurrentState->IsConnectedToReplicationSession(); 
	}

	IConcertClientReplicationManager::EStreamEnumerationResult FReplicationManager::ForEachRegisteredStream(
		TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback
		) const
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->ForEachRegisteredStream(Callback)
			: EStreamEnumerationResult::NoRegisteredStreams;
	}

	TFuture<FConcertReplication_ChangeAuthority_Response> FReplicationManager::RequestAuthorityChange(FConcertReplication_ChangeAuthority_Request Args)
	{
		if (ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point.")))
		{
			return CurrentState->RequestAuthorityChange(Args);
		}
		
		return RejectAll(MoveTemp(Args));
	}

	TFuture<FConcertReplication_QueryReplicationInfo_Response> FReplicationManager::QueryClientInfo(FConcertReplication_QueryReplicationInfo_Request Args)
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->QueryClientInfo(MoveTemp(Args))
			: MakeFulfilledPromise<FConcertReplication_QueryReplicationInfo_Response>().GetFuture();
	}

	TFuture<FConcertReplication_ChangeStream_Response> FReplicationManager::ChangeStream(FConcertReplication_ChangeStream_Request Args)
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->ChangeStream(MoveTemp(Args))
			: MakeFulfilledPromise<FConcertReplication_ChangeStream_Response>().GetFuture(); 
	}

	IConcertClientReplicationManager::EAuthorityEnumerationResult FReplicationManager::ForEachClientOwnedObject(
		TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)> Callback) const
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->ForEachClientOwnedObject(Callback)
			: EAuthorityEnumerationResult::NoAuthorityAvailable;
	}

	TSet<FGuid> FReplicationManager::GetClientOwnedStreamsForObject(const FSoftObjectPath& ObjectPath) const
	{
		return ensureMsgf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."))
			? CurrentState->GetClientOwnedStreamsForObject(ObjectPath)
			: TSet<FGuid>{}; 
	}

	IConcertClientReplicationManager::FOnPreStreamsChanged& FReplicationManager::OnPreStreamsChanged()
	{
		// Check() to avoid returning some dummy static variable
		checkf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."));
		return CurrentState->OnPreStreamsChanged();
	}

	IConcertClientReplicationManager::FOnPostStreamsChanged& FReplicationManager::OnPostStreamsChanged()
	{
		// Check() to avoid returning some dummy static variable
		checkf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."));
		return CurrentState->OnPostStreamsChanged();
	}

	IConcertClientReplicationManager::FOnPreAuthorityChanged& FReplicationManager::OnPreAuthorityChanged()
	{
		// Check() to avoid returning some dummy static variable
		checkf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."));
		return CurrentState->OnPreAuthorityChanged();
	}

	IConcertClientReplicationManager::FOnPostAuthorityChanged& FReplicationManager::OnPostAuthorityChanged()
	{
		// Check() to avoid returning some dummy static variable
		checkf(CurrentState, TEXT("StartAcceptingJoinRequests should have been called at this point."));
		return CurrentState->OnPostAuthorityChanged();
	}

	void FReplicationManager::OnChangeState(TSharedRef<FReplicationManagerState> NewState)
	{
		CurrentState = MoveTemp(NewState);
	}
}

