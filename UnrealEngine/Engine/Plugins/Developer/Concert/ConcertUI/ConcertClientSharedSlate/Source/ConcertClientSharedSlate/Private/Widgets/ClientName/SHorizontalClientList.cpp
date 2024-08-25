// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ClientName/SHorizontalClientList.h"

#include "IConcertClient.h"
#include "Widgets/ClientName/SClientName.h"

#include "Widgets/ClientName/SLocalClientName.h"
#include "Widgets/ClientName/SRemoteClientName.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHorizontalClientList"

namespace UE::ConcertClientSharedSlate
{
	namespace HorizontalClientList
	{
		TArray<FConcertSessionClientInfo> GetSortedClients(
			const IConcertClient& LocalConcertClient,
			const TConstArrayView<FGuid>& Clients,
			const SHorizontalClientList::FSortPredicate& SortPredicate
			)
		{
			TArray<FConcertSessionClientInfo> ClientsToDisplay;
			
			const TSharedPtr<IConcertClientSession> ClientSession = LocalConcertClient.GetCurrentSession();
			if (!ensureMsgf(ClientSession, TEXT("This widget does not work if the local client is not in a session!")))
			{
				return ClientsToDisplay;
			}
		
			// Prefetch the client info to avoid many FindSessionClient during Sort()
			for (const FGuid& Client : Clients)
			{
				FConcertSessionClientInfo Info;
				if (ClientSession->FindSessionClient(Client, Info))
				{
					ClientsToDisplay.Add(Info);
				}
				else if (Client == ClientSession->GetSessionClientEndpointId())
				{
					ClientsToDisplay.Add({Client, ClientSession->GetLocalClientInfo()});
				}
			}

			ClientsToDisplay.Sort([&SortPredicate](const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right)
			{
				return SortPredicate.Execute(Left, Right);
			});
			
			return ClientsToDisplay;
		}
	}
	
	bool SHorizontalClientList::SortLocalClientFirstThenAlphabetical(const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right, TSharedRef<IConcertClient> Client)
	{
		// If one of the compare clients is local, always return that the local client is smaller.
		const auto IsLocalClient = [&Client](const FConcertSessionClientInfo& Info)
		{
			return Client->GetCurrentSession() && Client->GetCurrentSession()->GetSessionClientEndpointId() == Info.ClientEndpointId;
		};
		const bool bLeftIsLocalClient = IsLocalClient(Left);
		const bool bRightIsLocalClient = IsLocalClient(Right);
		return bLeftIsLocalClient
			|| (!bRightIsLocalClient && Left.ClientInfo.DisplayName < Right.ClientInfo.DisplayName);
	}

	TOptional<FString> SHorizontalClientList::GetDisplayString(const IConcertClient& LocalConcertClient, const TConstArrayView<FGuid>& Clients, const FSortPredicate& SortPredicate)
	{
		const TSharedPtr<IConcertClientSession> ClientSession = LocalConcertClient.GetCurrentSession();
		const TArray<FConcertSessionClientInfo> ClientsToDisplay = HorizontalClientList::GetSortedClients(LocalConcertClient, Clients, SortPredicate);
		if (!ClientSession || ClientsToDisplay.IsEmpty())
		{
			return {};
		}
		
		return FString::JoinBy(ClientsToDisplay, TEXT(", "), [&ClientSession](const FConcertSessionClientInfo& ClientInfo)
			{
				// GetSortedClients should return empty if GetSortedClients is invalid
				const bool bIsLocalClient = ensure(ClientSession) && ClientInfo.ClientEndpointId == ClientSession->GetSessionClientEndpointId();
				return SClientName::GetDisplayText(ClientInfo.ClientInfo, bIsLocalClient).ToString();
			});
	}

	void SHorizontalClientList::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient)
	{
		LocalConcertClient = MoveTemp(InClient);
		SortPredicateDelegate = InArgs._SortPredicate.IsBound()
			? InArgs._SortPredicate
			: FSortPredicate::CreateStatic(&SHorizontalClientList::SortLocalClientFirstThenAlphabetical, InClient);
		HighlightTextAttribute = InArgs._HighlightText;
		NameFont = InArgs._Font;
		
		ChildSlot
		[
			SAssignNew(WidgetSwitcher, SWidgetSwitcher)
			.WidgetIndex(0)
			+SWidgetSwitcher::Slot()
			[
				InArgs._EmptyListSlot.Widget
			]
			+SWidgetSwitcher::Slot()
			[
				SAssignNew(ScrollBox, SScrollBox)
				.Orientation(Orient_Horizontal)
			]
		];
	}

	void SHorizontalClientList::RefreshList(const TConstArrayView<FGuid>& Clients)
	{
		ScrollBox->ClearChildren();

		if (Clients.IsEmpty())
		{
			WidgetSwitcher->SetActiveWidgetIndex(0);
			return;
		}
		WidgetSwitcher->SetActiveWidgetIndex(1);

		const TSharedPtr<IConcertClientSession> ClientSession = LocalConcertClient->GetCurrentSession();
		const TArray<FConcertSessionClientInfo> ClientsToDisplay = HorizontalClientList::GetSortedClients(*LocalConcertClient, Clients, SortPredicateDelegate);
		
		bool bIsFirst = true;
		for (const FConcertSessionClientInfo& Info : ClientsToDisplay)
		{
			if (!bIsFirst)
			{
				ScrollBox->AddSlot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Comma", ", "))
					.Font(NameFont)
				];
			}

			// GetSortedClients should return empty if GetSortedClients is invalid
			if (ensure(ClientSession) && Info.ClientEndpointId == ClientSession->GetSessionClientEndpointId())
			{
				ScrollBox->AddSlot()
				[
					SNew(SLocalClientName, LocalConcertClient.ToSharedRef())
					.HighlightText(HighlightTextAttribute)
					.Font(NameFont)
				];
			}
			else
			{
				ScrollBox->AddSlot()
				[
					SNew(SRemoteClientName, LocalConcertClient.ToSharedRef())
					.ClientEndpointId(Info.ClientEndpointId)
					.HighlightText(HighlightTextAttribute)
					.Font(NameFont)
				];
			}

			bIsFirst = false;
		}
	}
}

#undef LOCTEXT_NAMESPACE