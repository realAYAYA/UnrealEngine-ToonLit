// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationManager.h"

#include "ConcertLogGlobal.h"
#include "IConcertSyncClient.h"
#include "Replication/ChangeOperationTypes.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "Containers/Ticker.h"
#include "UObject/Package.h"

namespace UE::MultiUserClient
{
	FMultiUserReplicationManager::FMultiUserReplicationManager(TSharedRef<IConcertSyncClient> InClient)
		: Client(MoveTemp(InClient))
	{
		Client->GetConcertClient()->OnSessionConnectionChanged().AddRaw(
			this,
			&FMultiUserReplicationManager::OnSessionConnectionChanged
			);
	}

	FMultiUserReplicationManager::~FMultiUserReplicationManager()
	{
		Client->GetConcertClient()->OnSessionConnectionChanged().RemoveAll(this);
	}

	void FMultiUserReplicationManager::JoinReplicationSession()
	{
		IConcertClientReplicationManager* Manager = Client->GetReplicationManager();
		if (!ensure(ConnectionState == EMultiUserReplicationConnectionState::Disconnected)
			|| !ensure(Manager))
		{
			return;
		}

		ConnectionState = EMultiUserReplicationConnectionState::Connecting;
		// For now we join without any initial data - this will likely change in the future (5.5+)
		Manager->JoinReplicationSession({})
			.Next([WeakThis = AsWeak()](ConcertSyncClient::Replication::FJoinReplicatedSessionResult&& JoinSessionResult)
			{
				// The future can execute on any thread
				ExecuteOnGameThread(TEXT("JoinReplicationSession"), [WeakThis, JoinSessionResult = MoveTemp(JoinSessionResult)]()
				{
					// Shutting down engine?
					if (const TSharedPtr<FMultiUserReplicationManager> ThisPin = WeakThis.Pin())
					{
						ThisPin->HandleReplicationSessionJoined(JoinSessionResult);
					}
				});
			});
	}

	void FMultiUserReplicationManager::OnSessionConnectionChanged(
		IConcertClientSession& ConcertClientSession,
		EConcertConnectionStatus ConcertConnectionStatus
		)
	{
		switch (ConcertConnectionStatus)
		{
		case EConcertConnectionStatus::Connecting:
			break;
		case EConcertConnectionStatus::Connected:
			JoinReplicationSession();
			break;
		case EConcertConnectionStatus::Disconnecting:
			break;
		case EConcertConnectionStatus::Disconnected:
			OnLeaveSession(ConcertClientSession);
			break;
		default: ;
		}
	}

	void FMultiUserReplicationManager::OnLeaveSession(IConcertClientSession&)
	{
		// This clears the UI. The clients' IEditableReplicationStreamModels should no longer be referenced by anyone.
		SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Disconnected);
		// Keep in mind the IEditableReplicationStreamModels were referenced by the UI so call this after clearing the UI.
		ConnectedState.Reset();
	}

	void FMultiUserReplicationManager::HandleReplicationSessionJoined(const ConcertSyncClient::Replication::FJoinReplicatedSessionResult& JoinSessionResult)
	{
		const bool bSuccess = JoinSessionResult.ErrorCode == EJoinReplicationErrorCode::Success;
		if (bSuccess)
		{
			ConnectedState.Emplace(Client, DiscoveryContainer);
			SetupClientConnectionEvents();
			SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Connected);
		}
		else
		{
			SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Disconnected);
		}
	}

	void FMultiUserReplicationManager::SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState NewState)
	{
		ConnectionState = NewState;
		OnReplicationConnectionStateChangedDelegate.Broadcast(ConnectionState);
	}

	void FMultiUserReplicationManager::SetupClientConnectionEvents()
	{
		FReplicationClientManager& ClientManager = ConnectedState->ClientManager;
		ClientManager.ForEachClient([this](FReplicationClient& InClient){ SetupClientDelegates(InClient); return EBreakBehavior::Continue; });
		ClientManager.OnPostRemoteClientAdded().AddRaw(this, &FMultiUserReplicationManager::OnReplicationClientConnected);
	}

	void FMultiUserReplicationManager::OnClientStreamServerStateChanged(const FGuid EndpointId) const
	{
		UE_LOG(LogConcert, Verbose, TEXT("Client %s stream changed"), *EndpointId.ToString());
		OnStreamServerStateChangedDelegate.Broadcast(EndpointId);
	}

	void FMultiUserReplicationManager::OnClientAuthorityServerStateChanged(const FGuid EndpointId) const
	{
		
		UE_LOG(LogConcert, Verbose, TEXT("Client %s authority changed"), *EndpointId.ToString());
		OnAuthorityServerStateChangedDelegate.Broadcast(EndpointId);
	}

	void FMultiUserReplicationManager::SetupClientDelegates(FReplicationClient& InClient) const
	{
		InClient.GetStreamSynchronizer().OnServerStateChanged().AddRaw(this, &FMultiUserReplicationManager::OnClientStreamServerStateChanged, InClient.GetEndpointId());
		InClient.GetAuthoritySynchronizer().OnServerStateChanged().AddRaw(this, &FMultiUserReplicationManager::OnClientAuthorityServerStateChanged, InClient.GetEndpointId());
	}

	const FConcertObjectReplicationMap* FMultiUserReplicationManager::FindReplicationMapForClient(const FGuid& ClientId) const
	{
		if (ConnectedState && ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			const FReplicationClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient
				? &ReplicationClient->GetStreamSynchronizer().GetServerState()
				: nullptr;
		}
		return nullptr;
	}

	const FConcertStreamFrequencySettings* FMultiUserReplicationManager::FindReplicationFrequenciesForClient(const FGuid& ClientId) const
	{
		if (ConnectedState && ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			const FReplicationClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient
				? &ReplicationClient->GetStreamSynchronizer().GetFrequencySettings()
				: nullptr;
		}
		return nullptr;
	}

	bool FMultiUserReplicationManager::IsReplicatingObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const 
	{
		if (ConnectedState && ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			const FReplicationClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient && ReplicationClient->GetAuthoritySynchronizer().HasAuthorityOver(ObjectPath);
		}
		return false;
	}

	void FMultiUserReplicationManager::RegisterReplicationDiscoverer(TSharedRef<IReplicationDiscoverer> Discoverer)
	{
		if (ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			DiscoveryContainer.AddDiscoverer(Discoverer);
		}
	}

	void FMultiUserReplicationManager::RemoveReplicationDiscoverer(const TSharedRef<IReplicationDiscoverer>& Discoverer)
	{
		if (ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			DiscoveryContainer.RemoveDiscoverer(Discoverer);
		}
	}

	TSharedRef<IClientChangeOperation> FMultiUserReplicationManager::EnqueueChanges(const FGuid& ClientId, TAttribute<FChangeClientReplicationRequest> SubmissionParams)
	{
		if (!ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			return FExternalClientChangeRequestHandler::MakeFailedOperation(EChangeStreamOperationResult::NotOnGameThread, EChangeAuthorityOperationResult::NotOnGameThread);
		}
		
		if (ConnectedState)
		{
			FReplicationClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient
				? ReplicationClient->GetExternalRequestHandler().HandleRequest(MoveTemp(SubmissionParams))
				: FExternalClientChangeRequestHandler::MakeFailedOperation(EChangeStreamOperationResult::UnknownClient, EChangeAuthorityOperationResult::UnknownClient);
		}
		return FExternalClientChangeRequestHandler::MakeFailedOperation(EChangeStreamOperationResult::NotInSession, EChangeAuthorityOperationResult::NotInSession);
	}

	FMultiUserReplicationManager::FConnectedState::FConnectedState(TSharedRef<IConcertSyncClient> InClient, FReplicationDiscoveryContainer& InDiscoveryContainer)
		: ClientManager(InClient, InClient->GetConcertClient()->GetCurrentSession().ToSharedRef(), InDiscoveryContainer)
		, ChangeLevelHandler(ClientManager.GetLocalClient().GetClientEditModel().Get())
	{}
}
