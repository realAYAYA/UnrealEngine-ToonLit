// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAllClientsView.h"

#include "AllClientsSelectionModel.h"
#include "SMultiClientView.h"
#include "Replication/Client/ReplicationClientManager.h"

#include "Algo/Transform.h"

namespace UE::MultiUserClient
{
	void SAllClientsView::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager)
	{
		ClientManager = &InClientManager;
		AllClientsModel = MakeUnique<FAllClientsSelectionModel>(InClientManager);
		
		ChildSlot
		[
			SNew(SMultiClientView, InConcertClient, InClientManager, *AllClientsModel)
			.ViewSelectionArea() [ InArgs._ViewSelectionArea.Widget ]
		];
	}

	TSet<const FReplicationClient*> SAllClientsView::GetAllClients() const
	{
		TSet<const FReplicationClient*> Result;
		Algo::Transform(ClientManager->GetRemoteClients(), Result, [](const TNonNullPtr<FRemoteReplicationClient>& Client)
		{
			return Client.Get();
		});
		Result.Add(&ClientManager->GetLocalClient());
		return Result;
	}
}
