// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClientManager.h"

#include "IConcertSyncClient.h"
#include "LocalReplicationClient.h"
#include "RemoteReplicationClient.h"
#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/Stream/StreamSynchronizer_LocalClient.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::MultiUserClient
{
	FReplicationClientManager::FReplicationClientManager(
		const TSharedRef<IConcertSyncClient>& InClient,
		const TSharedRef<IConcertClientSession>& InSession,
		FReplicationDiscoveryContainer& InRegisteredExtenders
		)
		: SessionContent(NewObject<UMultiUserReplicationSessionPreset>(GetTransientPackage(), NAME_None, RF_Transient))
		, ConcertClient(InClient)
		, Session(InSession)
		, RegisteredExtenders(InRegisteredExtenders)
		, QueryService(InClient)
		, AuthorityCache(*this)
		, LocalClient([this, InClient]()
		{
			UMultiUserReplicationClientPreset* ClientPreset = SessionContent->AddClient();
			return FLocalReplicationClient(RegisteredExtenders, AuthorityCache, *ClientPreset, MakeUnique<FStreamSynchronizer_LocalClient>(InClient, ClientPreset->Stream->StreamId), InClient);
		}())
		, SubmissionNotifier(*this)
		, ReassignmentLogic(*this)
	{
		AuthorityCache.RegisterEvents();
		InSession->OnSessionClientChanged().AddRaw(this, &FReplicationClientManager::OnSessionClientChanged);

		for (const FGuid& ClientEndpointId : InSession->GetSessionClientEndpointIds())
		{
			CreateRemoteClient(ClientEndpointId);
		}
	}

	FReplicationClientManager::~FReplicationClientManager()
	{
		if (const TSharedPtr<IConcertClientSession> SessionPin = Session.Pin())
		{
			SessionPin->OnSessionClientChanged().RemoveAll(this);
		}
	}

	TArray<TNonNullPtr<const FRemoteReplicationClient>> FReplicationClientManager::GetRemoteClients() const
	{
		TArray<TNonNullPtr<const FRemoteReplicationClient>> Result;
		Algo::Transform(RemoteClients, Result, [](const TUniquePtr<FRemoteReplicationClient>& Client) -> TNonNullPtr<const FRemoteReplicationClient>
		{
			return Client.Get();
		});
		return Result;
	}

	TArray<TNonNullPtr<FRemoteReplicationClient>> FReplicationClientManager::GetRemoteClients()
	{
		TArray<TNonNullPtr<FRemoteReplicationClient>> Result;
		Algo::Transform(RemoteClients, Result, [](const TUniquePtr<FRemoteReplicationClient>& Client) -> TNonNullPtr<FRemoteReplicationClient>
		{
			return Client.Get();
		});
		return Result;
	}

	TArray<TNonNullPtr<const FReplicationClient>> FReplicationClientManager::GetClients(TFunctionRef<bool(const FReplicationClient& Client)> Predicate) const
	{
		TArray<TNonNullPtr<const FReplicationClient>> Result;
		ForEachClient([&Predicate, &Result](const FReplicationClient& Client)
		{
			if (Predicate(Client))
			{
				Result.Add(&Client);
			}
			return EBreakBehavior::Continue;
		});
		return Result;
	}

	const FRemoteReplicationClient* FReplicationClientManager::FindRemoteClient(const FGuid& EndpointId) const
	{
		const TUniquePtr<FRemoteReplicationClient>* Client = RemoteClients.FindByPredicate([&EndpointId](const TUniquePtr<FRemoteReplicationClient>& Client)
		{
			return Client->GetEndpointId() == EndpointId;
		});
		return Client ? Client->Get() : nullptr;
	}

	void FReplicationClientManager::ForEachClient(TFunctionRef<EBreakBehavior(const FReplicationClient&)> ProcessClient) const
	{
		if (ProcessClient(GetLocalClient()) == EBreakBehavior::Break)
		{
			return;
		}
		for (const TNonNullPtr<const FRemoteReplicationClient>& RemoteClient : GetRemoteClients())
		{
			if (ProcessClient(*RemoteClient) == EBreakBehavior::Break)
			{
				return;
			}
		}
	}

	void FReplicationClientManager::ForEachClient(TFunctionRef<EBreakBehavior(FReplicationClient&)> ProcessClient)
	{
		const FReplicationClientManager* ConstThis = this;
		ConstThis->ForEachClient([&ProcessClient](const FReplicationClient& Client)
		{
			// const_cast safe here because remote clients are never const and GetLocalClient() is not const here since this overload is non-const
			return ProcessClient(const_cast<FReplicationClient&>(Client));
		});
	}

	void FReplicationClientManager::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(SessionContent);
	}

	void FReplicationClientManager::OnSessionClientChanged(IConcertClientSession&, EConcertClientStatus NewStatus, const FConcertSessionClientInfo& ClientInfo)
	{
		const FGuid& ClientEndpointId = ClientInfo.ClientEndpointId;
		switch (NewStatus)
		{
			
		case EConcertClientStatus::Connected:
			CreateRemoteClient(ClientEndpointId);
			break;
			
		case EConcertClientStatus::Disconnected:
			{
				const int32 Index = RemoteClients.IndexOfByPredicate(
					[&ClientEndpointId](const TUniquePtr<FRemoteReplicationClient>& Client)
					{
						return Client->GetEndpointId() == ClientEndpointId;
					});
				if (!ensure(RemoteClients.IsValidIndex(Index)))
				{
					return;
				}

				{
					const TUniquePtr<FRemoteReplicationClient> Client = MoveTemp(RemoteClients[Index]);
					OnPreRemoteClientRemovedDelegate.Broadcast(*Client.Get());
					SessionContent->RemoveClient(*Client->GetClientContent());
					RemoteClients.RemoveAtSwap(Index);
				}
				// We want to broadcast after the client has been fully cleaned up
				OnRemoteClientsChangedDelegate.Broadcast();
			}
			break;
			
		case EConcertClientStatus::Updated:
			break;
		default: checkNoEntry();
		}
	}
	
	void FReplicationClientManager::CreateRemoteClient(const FGuid& ClientEndpointId, bool bBroadcastDelegate)
	{
		TUniquePtr<FRemoteReplicationClient> RemoteClientPtr = MakeUnique<FRemoteReplicationClient>(
			ClientEndpointId,
			RegisteredExtenders,
			ConcertClient->GetConcertClient(),
			AuthorityCache,
			*SessionContent->AddClient(),
			QueryService
			);
		FRemoteReplicationClient& RemoteClient = *RemoteClientPtr;
		RemoteClients.Emplace(
			MoveTemp(RemoteClientPtr)
			);

		if (bBroadcastDelegate)
		{
			OnPostRemoteClientAddedDelegate.Broadcast(RemoteClient);
			OnRemoteClientsChangedDelegate.Broadcast();
		}
	}
}
