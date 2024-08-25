// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSelectClientViewComboButton.h"

#include "IConcertClient.h"
#include "Widgets/ClientName/SLocalClientName.h"
#include "Widgets/ClientName/SRemoteClientName.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SSelectClientViewComboButton"

namespace UE::MultiUserClient
{
	void SSelectClientViewComboButton::Construct(const FArguments& InArgs)
	{
		Client = InArgs._Client;
		ClientsAttribute = InArgs._SelectableClients;
		CurrentSelection = InArgs._CurrentSelection;
		CurrentDisplayMode = InArgs._CurrentDisplayMode;
		
		OnSelectClientDelegate = InArgs._OnSelectClient;
		OnSelectAllClients = InArgs._OnSelectAllClients;
		check(OnSelectClientDelegate.IsBound() && OnSelectAllClients.IsBound());
		
		ChildSlot
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &SSelectClientViewComboButton::MakeMenuContent)
			.ButtonContent()
			[
				SAssignNew(ButtonContent, SWidgetSwitcher)
				.WidgetIndex(this, &SSelectClientViewComboButton::GetActiveWidgetIndex)

				+SWidgetSwitcher::Slot()
				[
					MakeAllClientsDisplayWidget()
				]
				+SWidgetSwitcher::Slot()
				[
					SNew(ConcertClientSharedSlate::SLocalClientName, Client.ToSharedRef())
				]
				+SWidgetSwitcher::Slot()
				[
					SNew(ConcertClientSharedSlate::SRemoteClientName, Client.ToSharedRef())
					.ClientEndpointId(this, &SSelectClientViewComboButton::GetSelectedClientEndpointId)
				]
			]
		];
	}

	TSharedRef<SWidget> SSelectClientViewComboButton::MakeMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(FUIAction(
			FExecuteAction::CreateLambda([this](){ OnSelectAllClients.Execute(); }),
				FCanExecuteAction::CreateLambda([this](){ return GetActiveWidgetIndex() != static_cast<int32>(EClientViewType::AllClients); })
			),
			MakeAllClientsDisplayWidget()
			);

		for (const FGuid& ClientId : ClientsAttribute.Get())
		{
			const bool bIsLocalClient = ClientId == Client->GetCurrentSession()->GetSessionClientEndpointId();
			FUIAction UIAction(
				FExecuteAction::CreateLambda([this, ClientId](){ OnSelectClientDelegate.Execute(ClientId); }),
				FCanExecuteAction::CreateLambda([this, ClientId]()
				{
					return GetActiveWidgetIndex() == static_cast<int32>(EClientViewType::AllClients)
						|| CurrentSelection.Get() != ClientId;
				})
				);
			
			if (bIsLocalClient)
			{
				MenuBuilder.AddMenuEntry(UIAction, SNew(ConcertClientSharedSlate::SLocalClientName, Client.ToSharedRef()));
			}
			else
			{
				MenuBuilder.AddMenuEntry(UIAction, SNew(ConcertClientSharedSlate::SRemoteClientName, Client.ToSharedRef()).ClientEndpointId(ClientId));
			}
		}
		
		return MenuBuilder.MakeWidget();
	}

	TSharedRef<SWidget> SSelectClientViewComboButton::MakeAllClientsDisplayWidget() const
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AllClients", "All Clients"))
			];
	}

	int32 SSelectClientViewComboButton::GetActiveWidgetIndex() const
	{
		return static_cast<int32>(CurrentDisplayMode.Get());
	}

	FGuid SSelectClientViewComboButton::GetSelectedClientEndpointId() const
	{
		return CurrentSelection.Get();
	}
}

#undef LOCTEXT_NAMESPACE