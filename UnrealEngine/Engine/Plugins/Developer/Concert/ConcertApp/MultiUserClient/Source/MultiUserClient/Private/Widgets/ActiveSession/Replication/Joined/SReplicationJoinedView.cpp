// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationJoinedView.h"

#include "IConcertSyncClient.h"
#include "Replication/MultiUserReplicationManager.h"
#include "SSelectClientViewComboButton.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/SAllClientsView.h"
#include "Widgets/ActiveSession/Replication/Client/Single/SReplicationClientView.h"

#include "Replication/Client/RemoteReplicationClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicationJoinedWidget"

namespace UE::MultiUserClient
{
	void SReplicationJoinedView::Construct(
		const FArguments& InArgs,
		TSharedRef<FMultiUserReplicationManager> InReplicationManager,
		TSharedRef<IConcertSyncClient> InClient
		)
	{
		Client = MoveTemp(InClient);
		ReplicationManager = MoveTemp(InReplicationManager);

		ChildSlot
		[
			SAssignNew(ClientViewSwitcher, SWidgetSwitcher)
			// These must be aligned with EClientViewType
			+SWidgetSwitcher::Slot()
			[
				SNew(SAllClientsView, InClient->GetConcertClient(), *ReplicationManager->GetClientManager())
				.ViewSelectionArea()
				[
					MakeClientSelectionArea()
				]
			]
			+SWidgetSwitcher::Slot()
			[
				SNew(SReplicationClientView, InClient->GetConcertClient(), *ReplicationManager->GetClientManager())
				.GetReplicationClient_Lambda([this](){ return &ReplicationManager->GetClientManager()->GetLocalClient(); })
				.ViewSelectionArea()
				[
					MakeClientSelectionArea()
				]
			]
		];
		
		RefreshClientViewSwitcher();
		ReplicationManager->GetClientManager()->OnRemoteClientsChanged().AddSP(this, &SReplicationJoinedView::RefreshClientViewSwitcher);
	}

	void SReplicationJoinedView::RefreshClientViewSwitcher()
	{
		const int32 ActiveViewIndex = ClientViewSwitcher->GetActiveWidgetIndex();
		const FGuid DisplayedClientId = ActiveViewIndex == 0
			? Client->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId()
			: [this, &ActiveViewIndex]() 
			{
				for (const TPair<FGuid, int32>& Pair : RemoteClientToWidgetSwitcherIndex)
				{
					if (Pair.Value == ActiveViewIndex)
					{
						return Pair.Key;
					}
				}
				return FGuid{};
			}();
		
		// Retain the old SWidgets so their transient state, like highlights, are retained when the remote clients change.
		TArray<TSharedRef<SWidget>> OldClientWidgets;
		OldClientWidgets.Reserve(ClientViewSwitcher->GetNumWidgets());
		while (ClientViewSwitcher->GetNumWidgets() > 0)
		{
			const TSharedRef<SWidget> Widget = ClientViewSwitcher->GetWidget(0).ToSharedRef();
			OldClientWidgets.Add(Widget);
			ClientViewSwitcher->RemoveSlot(Widget);
		}
		checkf(OldClientWidgets.Num() >= 2, TEXT("Was supposed to contain all clients and local client widgets"));

		// All clients
		ClientViewSwitcher->AddSlot()
			[
				OldClientWidgets[0]
			];
		// Local client
		ClientViewSwitcher->AddSlot()
			[
				OldClientWidgets[1]
			];
		RebuildClientViewSwitcherChildren(OldClientWidgets);

		// Need to change view if the displayed client was removed
		const bool bDisplayedClientWasRemoved = RemoteClientToWidgetSwitcherIndex.Contains(DisplayedClientId); 
		if (bDisplayedClientWasRemoved)
		{
			// Will show all clients
			ClientViewSwitcher->SetActiveWidgetIndex(0);
		}
		
		// ~OldClientWidgets now disposes of the widgets for which no client exists anymore
	}

	void SReplicationJoinedView::RebuildClientViewSwitcherChildren(const TArray<TSharedRef<SWidget>> OldClientWidgets)
	{
		TMap<FGuid, int32> OldRemoteClientToWidgetSwitcherIndex = MoveTemp(RemoteClientToWidgetSwitcherIndex);
        for (const TNonNullPtr<FRemoteReplicationClient>& RemoteClient : ReplicationManager->GetClientManager()->GetRemoteClients())
        {
        	const FGuid& EndpointId = RemoteClient->GetEndpointId();
        	
        	const int32* ExistingClientWidgetIndex = OldRemoteClientToWidgetSwitcherIndex.Find(EndpointId);
        	if (ExistingClientWidgetIndex)
        	{
        		ClientViewSwitcher->AddSlot()
        		[
        			OldClientWidgets[*ExistingClientWidgetIndex]
        		];
        	}
        	else
        	{
        		ClientViewSwitcher->AddSlot()
        		[
        			SNew(SReplicationClientView, Client->GetConcertClient(), *ReplicationManager->GetClientManager())
        			.GetReplicationClient_Lambda([this, EndpointId = RemoteClient->GetEndpointId()]()
        			{
        				// It is unsafe to simply capture RemoteClient because the containing TArray may reallocate its location
        				return ReplicationManager->GetClientManager()->FindRemoteClient(EndpointId);
        			})
        			.ViewSelectionArea()
        			[
        				MakeClientSelectionArea()
        			]
        		];
        	}

        	const int32 NewIndex = ClientViewSwitcher->GetNumWidgets() - 1;
        	RemoteClientToWidgetSwitcherIndex.Add(EndpointId, NewIndex);
        }
	}

	TSharedRef<SWidget> SReplicationJoinedView::MakeClientSelectionArea()
	{
		return SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("ClientView.ToolTip", "Select the client(s) of which you want to see the registered objects & properties."))

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ClientView.Label", "Client View"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f)
			[
				SNew(SBox)
				.MinDesiredWidth(200.f)
				[
					MakeClientSelectionComboBox()
				]
			];
	}

	TSharedRef<SWidget> SReplicationJoinedView::MakeClientSelectionComboBox()
	{
		return SNew(SSelectClientViewComboButton)
		
			// Returning client info
			.Client(Client->GetConcertClient())
			.SelectableClients_Lambda([this]()
			{
				const TArray<TNonNullPtr<FRemoteReplicationClient>> RemoteClients = ReplicationManager->GetClientManager()->GetRemoteClients();
				TArray<FGuid> Result;
				Algo::Transform(RemoteClients, Result, [](const TNonNullPtr<FRemoteReplicationClient>& InClient){ return InClient->GetEndpointId(); });

				const TSharedPtr<IConcertClientSession> CurrentSession = Client->GetConcertClient()->GetCurrentSession();
				Result.Sort([&CurrentSession](const FGuid& Left, const FGuid& Right)
				{
					FConcertSessionClientInfo LeftInfo;
					FConcertSessionClientInfo RightInfo;
					CurrentSession->FindSessionClient(Left, LeftInfo);
					CurrentSession->FindSessionClient(Right, RightInfo);
					return LeftInfo.ClientInfo.DisplayName <= RightInfo.ClientInfo.DisplayName;
				});
				Result.Insert(CurrentSession->GetSessionClientEndpointId(), 0);
				return Result;
			})
			.CurrentSelection_Lambda([this]()
			{
				const TOptional<FGuid> RemoteClient = GetRemoteClientBySwitcherIndex(ClientViewSwitcher->GetActiveWidgetIndex());
				return RemoteClient
					? *RemoteClient
					: Client->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId();
			})
			.CurrentDisplayMode_Lambda([this]()
			{
				switch(ClientViewSwitcher->GetActiveWidgetIndex())
				{
				case 0: return EClientViewType::AllClients;
				case 1: return EClientViewType::LocalClient;
				default: return EClientViewType::RemoteClient;
				}
			})
			.OnSelectClient_Lambda([this](const FGuid& InClientId)
			{
				const int32* RemoteClientIndex = RemoteClientToWidgetSwitcherIndex.Find(InClientId);
				// If it's not a remote client, it's the local client
				const int32 ActiveWidgetIndex = RemoteClientIndex ? *RemoteClientIndex : 1;
				ClientViewSwitcher->SetActiveWidgetIndex(ActiveWidgetIndex);
			})
			.OnSelectAllClients_Lambda([this]()
			{
				ClientViewSwitcher->SetActiveWidgetIndex(0);
			})
		;
	}

	TOptional<FGuid> SReplicationJoinedView::GetRemoteClientBySwitcherIndex(int32 WidgetSwitcherIndex) const
	{
		for (const TPair<FGuid, int32>& Pair : RemoteClientToWidgetSwitcherIndex)
		{
			if (Pair.Value == WidgetSwitcherIndex)
			{
				return { Pair.Key };
			}
		}
		return {};
	}
}

#undef LOCTEXT_NAMESPACE