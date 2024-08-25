// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientUtils.h"

#include "ConcertMessageData.h"
#include "IConcertClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Widgets/ClientName/SClientName.h"

namespace UE::MultiUserClient::ClientUtils
{
	FString GetClientDisplayName(const IConcertClient& InLocalClientInstance, const FGuid& InClientEndpointId)
	{
		const TSharedPtr<IConcertClientSession> Session = InLocalClientInstance.GetCurrentSession();
		return ensure(Session) ? GetClientDisplayName(*Session, InClientEndpointId) : FString{};
	}
	
	FString GetClientDisplayName(const IConcertClientSession& InSession, const FGuid& InClientEndpointId)
	{
		const bool bIsLocalClient = InSession.GetSessionClientEndpointId() == InClientEndpointId;
		if (bIsLocalClient)
		{
			return ConcertClientSharedSlate::SClientName::GetDisplayText(InSession.GetLocalClientInfo(), bIsLocalClient).ToString();
		}

		FConcertSessionClientInfo ClientInfo;
		if (InSession.FindSessionClient(InClientEndpointId, ClientInfo))
		{
			return ConcertClientSharedSlate::SClientName::GetDisplayText(ClientInfo.ClientInfo, bIsLocalClient).ToString();
		}

		ensureMsgf(false, TEXT("Bad args"));
		return InClientEndpointId.ToString(EGuidFormats::DigitsWithHyphens);
	}
	
	bool GetClientDisplayInfo(const IConcertClient& InLocalClientInstance, const FGuid& InClientEndpointId, FConcertClientInfo& OutClientInfo)
	{
		const TSharedPtr<IConcertClientSession> Session = InLocalClientInstance.GetCurrentSession();
		return GetClientDisplayInfo(*Session, InClientEndpointId, OutClientInfo);
	}
	
	bool GetClientDisplayInfo(const IConcertClientSession& InSession, const FGuid& InClientEndpointId, FConcertClientInfo& OutClientInfo)
	{
		const bool bIsLocalClient = InSession.GetSessionClientEndpointId() == InClientEndpointId;
		if (bIsLocalClient)
		{
			OutClientInfo = InSession.GetLocalClientInfo();
			return true;
		}

		FConcertSessionClientInfo ClientInfo;
		if (InSession.FindSessionClient(InClientEndpointId, ClientInfo))
		{
			OutClientInfo = MoveTemp(ClientInfo.ClientInfo);
			return true;
		}

		return false;
	}
	
	TArray<const FReplicationClient*> GetSortedClientList(const IConcertClient& InLocalClientInstance, const FReplicationClientManager& InReplicationManager)
	{
		return GetSortedClientList(*InLocalClientInstance.GetCurrentSession(), InReplicationManager);
	}

	TArray<const FReplicationClient*> GetSortedClientList(const IConcertClientSession& InSession, const FReplicationClientManager& InReplicationManager)
	{
		TArray<const FReplicationClient*> Result;
		TMap<const FReplicationClient*, FConcertClientInfo> ClientToDisplayInfo;
		for (const TNonNullPtr<const FRemoteReplicationClient> RemoteClient : InReplicationManager.GetRemoteClients())
		{
			FConcertClientInfo Info;
			if (GetClientDisplayInfo(InSession, RemoteClient->GetEndpointId(), Info))
			{
				Result.Add(RemoteClient);
				ClientToDisplayInfo.Add(RemoteClient, MoveTemp(Info));
			}
		}

		Result.Sort([&ClientToDisplayInfo](const FReplicationClient& Left, const FReplicationClient& Right)
		{
			const FConcertClientInfo& LeftInfo = ClientToDisplayInfo[&Left];
			const FConcertClientInfo& RightInfo = ClientToDisplayInfo[&Right];
			const FText LeftDisplayName = ConcertClientSharedSlate::SClientName::GetDisplayText(LeftInfo, false);
			const FText RightDisplayName = ConcertClientSharedSlate::SClientName::GetDisplayText(RightInfo, false);
			return LeftDisplayName.ToString() <= RightDisplayName.ToString();
		});
		
		Result.Insert(&InReplicationManager.GetLocalClient(), 0);
		return Result;
	}
}
