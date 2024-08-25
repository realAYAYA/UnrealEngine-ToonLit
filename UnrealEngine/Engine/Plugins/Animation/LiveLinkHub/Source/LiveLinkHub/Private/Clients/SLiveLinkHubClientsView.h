// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Async/Async.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Misc/Guid.h"
#include "LiveLinkClient.h"
#include "LiveLinkHubClientsModel.h"
#include "LiveLinkHubUEClientInfo.h"
#include "Session/LiveLinkHubSessionManager.h"
#include "SPositiveActionButton.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"


struct FGuid;
class FLiveLinkHub;

#define LOCTEXT_NAMESPACE "LiveLinkHub.ClientsView"

DECLARE_DELEGATE_OneParam(FOnClientSelected, FLiveLinkHubClientId/*ClientIdentifier*/);
DECLARE_DELEGATE_OneParam(FOnDiscoveredClientPicked, FLiveLinkHubClientId/*ClientIdentifier*/);
DECLARE_DELEGATE_OneParam(FOnRemoveClientFromSession, FLiveLinkHubClientId/*ClientIdentifier*/);


static const FName NameColumnId = "Name";
static const FName StatusColumnId = "Status";
static const FName EnabledIconColumnId = "EnabledIcon";

/** Tree view item that represents either a client or a livelink subject. */
struct FClientTreeViewItem
{
	FClientTreeViewItem(FLiveLinkHubClientId InClientId, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
		: ClientId(MoveTemp(InClientId))
		, ClientsModel(MoveTemp(InClientsModel))
	{
	}

	virtual ~FClientTreeViewItem() = default;

	/** Get the subject key for this tree item (Invalid key for client rows). */
	virtual const FLiveLinkSubjectKey& GetSubjectKey() const
	{
		static const FLiveLinkSubjectKey InvalidSubjectKey;
		return InvalidSubjectKey;
	}

	/**
	 * For clients, returns if it should receive any livelink data from the hub (except for heartbeat messages).
	 * For subjects, returns if the hub transmit this subject's data to the client.
	 */
	virtual bool IsEnabled() const = 0;

	/** Whether the row should be in read only (ie. if the source is disconnected) */
	virtual bool IsReadOnly() const = 0;

	/**
	 * Set whether this item should be transmitted to the client. 
	 * @See IsEnabled()
	 */
	virtual void SetEnabled(bool bInEnabled) = 0;

	/** Get status text for the row. */
	virtual FText GetStatusText() const = 0;
	
public:
	/** This item's children, in the case of client rows, these represent the livelink subjects. */
	TArray<TSharedPtr<FClientTreeViewItem>> Children;
	/** Name of the tree item (client or subject name). */
	FText Name;
	/** Identifier of the unreal client for this item. */
	FLiveLinkHubClientId ClientId;
	/** ClientsModel used to retrieve information about clients/subjects. */
	TWeakPtr<ILiveLinkHubClientsModel> ClientsModel;
};

/** Holds a client row's data. */
struct FClientTreeViewClientItem : public FClientTreeViewItem
{
	FClientTreeViewClientItem(FLiveLinkHubClientId InClientId, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
		: FClientTreeViewItem(MoveTemp(InClientId), MoveTemp(InClientsModel))
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientModelPtr = ClientsModel.Pin())
		{
			Name = ClientModelPtr->GetClientDisplayName(ClientId);
		}
	}

	virtual bool IsEnabled() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return ClientsModelPtr->IsClientEnabled(ClientId);
		}
		return false;
	}

	virtual bool IsReadOnly() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return !ClientsModelPtr->IsClientConnected(ClientId);
		}

		return true;
	}

	virtual void SetEnabled(bool bInEnabled) override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			ClientsModelPtr->SetClientEnabled(ClientId, bInEnabled);
		}
	}

	virtual FText GetStatusText() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return ClientsModelPtr->GetClientStatus(ClientId);
		}

		return LOCTEXT("InvalidStatus", "Disconnected");
	}
	//~ End FClientTreeViewItem interface
};

/** Holds a subject row's data. */
struct FClientTreeViewSubjectItem : public FClientTreeViewItem
{
	FClientTreeViewSubjectItem(FLiveLinkHubClientId InClientId, FLiveLinkSubjectKey InLiveLinkSubjectKey, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
		: FClientTreeViewItem(MoveTemp(InClientId), MoveTemp(InClientsModel))
		, LiveLinkSubjectKey(MoveTemp(InLiveLinkSubjectKey))
	{
		const FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		Name = FText::Format(LOCTEXT("SubjectName", "{0} - {1}"), FText::FromName(LiveLinkSubjectKey.SubjectName), LiveLinkClient.GetSourceType(LiveLinkSubjectKey.Source));
	}

	//~ Begin FClientTreeViewItem interface
	virtual const FLiveLinkSubjectKey& GetSubjectKey() const override
	{
		return LiveLinkSubjectKey;
	}

	virtual bool IsEnabled() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return ClientsModelPtr->IsSubjectEnabled(ClientId, LiveLinkSubjectKey);
		}
		return false;
	}

	virtual bool IsReadOnly() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return !ClientsModelPtr->IsClientEnabled(ClientId) || !ClientsModelPtr->IsClientConnected(ClientId);
		}

		return false;
	}

	virtual void SetEnabled(bool bInEnabled) override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			ClientsModelPtr->SetSubjectEnabled(ClientId, LiveLinkSubjectKey, bInEnabled);
		}
	}

	virtual FText GetStatusText() const override
	{
		return FText::GetEmpty();
	}
	//~ End FClientTreeViewItem interface

	/** Unique key for this item's subject. */
	FLiveLinkSubjectKey LiveLinkSubjectKey;
};

/** Holds a client row's data. */
class SLiveLinkHubClientsRow : public SMultiColumnTableRow<TSharedPtr<FClientTreeViewItem>>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubClientsRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FClientTreeViewItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		TreeItem = InArgs._Item;

		SMultiColumnTableRow<TSharedPtr<FClientTreeViewItem>>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			InOwnerTableView
		);
	}

	//~ Begin SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == NameColumnId)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew(SExpanderArrow, SharedThis(this)).IndentAmount(12)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.ToolTipText(TreeItem->Name)
					.Text(TreeItem->Name)
				];
		}
		else if (ColumnName == StatusColumnId)
		{
			return SNew(STextBlock)
				.Text(this, &SLiveLinkHubClientsRow::GetStatusText);
		}
		else if (ColumnName == EnabledIconColumnId)
		{
			return SNew(SCheckBox)
				.IsChecked(MakeAttributeSP(this, &SLiveLinkHubClientsRow::IsItemEnabled))
				.IsEnabled(this, &SLiveLinkHubClientsRow::IsCheckboxEnabled)
				.OnCheckStateChanged(this, &SLiveLinkHubClientsRow::OnEnabledCheckboxChange);
		}

		return SNullWidget::NullWidget;
	}
	//~ End SMultiColumnTableRow interface

private:
	/** Return whether the enable checkbox should be clickable. */
	bool IsCheckboxEnabled() const
	{
		return !TreeItem->IsReadOnly();
	}

	/** Return whether the enabled checkbox is checked. */
	ECheckBoxState IsItemEnabled() const
	{
		return TreeItem->IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Handler called when the enabled checkbox is clicked. */
	void OnEnabledCheckboxChange(ECheckBoxState State) const
	{
		TreeItem->SetEnabled(State == ECheckBoxState::Checked);
	}

	/** Get the status text from the tree item. */
	FText GetStatusText() const
	{
		return TreeItem->GetStatusText();
	}

private:
	/** The data represented by this row (Either an unreal client or a livelink subject). */
	TSharedPtr<FClientTreeViewItem> TreeItem;
};

/**
 * Provides the UI that displays the UE clients connected to the hub. 
 */
class SLiveLinkHubClientsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubClientsView) {}
	SLATE_EVENT(FOnClientSelected, OnClientSelected)
	SLATE_EVENT(FOnDiscoveredClientPicked, OnDiscoveredClientPicked)
	SLATE_EVENT(FOnRemoveClientFromSession, OnRemoveClientFromSession)
	SLATE_END_ARGS()

	using FClientTreeItemPtr = TSharedPtr<FClientTreeViewItem>;

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
	{
		OnClientSelectedDelegate = InArgs._OnClientSelected;
		OnDiscoveredClientPickedDelegate = InArgs._OnDiscoveredClientPicked;
		OnRemoveClientFromSessionDelegate = InArgs._OnRemoveClientFromSession;

		ClientsModel = MoveTemp(InClientsModel);
		 
		ClientsModel->OnClientEvent().AddSP(this, &SLiveLinkHubClientsView::OnClientEvent);

		const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		LiveLinkHubModule.GetSessionManager()->OnClientAddedToSession().AddSP(this, &SLiveLinkHubClientsView::OnClientAddedToSession);
		LiveLinkHubModule.GetSessionManager()->OnClientRemovedFromSession().AddSP(this, &SLiveLinkHubClientsView::OnClientRemovedFromSession);
		LiveLinkHubModule.GetSessionManager()->OnActiveSessionChanged().AddSP(this, &SLiveLinkHubClientsView::OnActiveSessionChanged);

		FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient.OnLiveLinkSubjectAdded().AddSP(this, &SLiveLinkHubClientsView::OnSubjectAdded_AnyThread);
		LiveLinkClient.OnLiveLinkSubjectRemoved().AddSP(this, &SLiveLinkHubClientsView::OnSubjectRemoved_AnyThread);

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(10.0, 7.0))
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 5.f, 0.f, 5.f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FSlateIcon("LiveLinkStyle", "LiveLinkHub.Clients.Icon").GetIcon())
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding(FMargin(4.0, 2.0))
				[
					SNew(STextBlock)
					.Font( DEFAULT_FONT( "Regular", 14 ) )
					.Text(LOCTEXT("ClientsHeaderText", "Clients"))
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SPositiveActionButton)
                	.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
                	.Text(LOCTEXT("AddClientLabel", "Add Client"))
                	.ToolTipText(LOCTEXT("AddClient_Tooltip", "Connect to an unreal editor instance."))
					.MenuContent()
					[
						SNew(SBox)
						.HeightOverride(200.0)
						.WidthOverride(200.0)
						[
							SAssignNew(DiscoveredClientsListView, SListView<TSharedPtr<FLiveLinkHubClientId>>)
							.ListItemsSource(&DiscoveredClients)
							.ItemHeight(20.0f)
							.OnSelectionChanged(this, &SLiveLinkHubClientsView::OnDiscoveredClientPicked)
							.OnGenerateRow(this, &SLiveLinkHubClientsView::OnGenerateDiscoveredClientsRow)
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TreeView, STreeView<FClientTreeItemPtr>)
				.TreeItemsSource(&Clients)
				.ItemHeight(20.0f)
				.OnSelectionChanged(this, &SLiveLinkHubClientsView::OnSelectionChanged)
				.OnGenerateRow(this, &SLiveLinkHubClientsView::OnGenerateClientRow)
				.OnContextMenuOpening(this, &SLiveLinkHubClientsView::OnContextMenuOpening)
				.OnGetChildren(this, &SLiveLinkHubClientsView::OnGetChildren)
				.OnKeyDownHandler(this, &SLiveLinkHubClientsView::OnKeyDownHandler)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(NameColumnId)
					.FillWidth(0.75f)
					.DefaultLabel(LOCTEXT("ItemName", "Name"))
					+ SHeaderRow::Column(StatusColumnId)
					.DefaultLabel(LOCTEXT("Status", "Status"))
					.FillWidth(0.25f)
					+ SHeaderRow::Column(EnabledIconColumnId)
					.ManualWidth(20.f)
					.DefaultLabel(LOCTEXT("EnabledIconEmpty", ""))
				)
			]
		];

		Reinitialize();
	}
	//~ End SWidget interface

	virtual ~SLiveLinkHubClientsView() override
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkClient.OnLiveLinkSubjectRemoved().RemoveAll(this);
			LiveLinkClient.OnLiveLinkSubjectAdded().RemoveAll(this);
		}

		if (FLiveLinkHubModule* LiveLinkHubModule = FModuleManager::Get().GetModulePtr<FLiveLinkHubModule>("LiveLinkHub"))
		{
			if (TSharedPtr<ILiveLinkHubSessionManager> SessionManager = LiveLinkHubModule->GetSessionManager())
			{
				SessionManager->OnActiveSessionChanged().RemoveAll(this);
				SessionManager->OnClientRemovedFromSession().RemoveAll(this);
				SessionManager->OnClientAddedToSession().RemoveAll(this);
			}
		}

		if (ClientsModel)
		{
			ClientsModel->OnClientEvent().RemoveAll(this);
		}
	}

	/** Refresh the data and widgets. */
	void Reinitialize()
	{
		PopulateClients();
		TreeView->RequestTreeRefresh();
		if (DiscoveredClientsListView)
		{
			DiscoveredClientsListView->RequestListRefresh();
		}
	}

	/** Get the currently selected client, or an invalid optional when nothing is selected. */
	TOptional<FLiveLinkHubClientId> GetSelectedClient() const
	{
		TOptional<FLiveLinkHubClientId> ClientId;

		TArray<FClientTreeItemPtr> SelectedClients = TreeView->GetSelectedItems();
		if (SelectedClients.Num())
		{
			ClientId = SelectedClients[0]->ClientId;
		}

		return ClientId;
	}

private:
	/** Handler used to generate a widget for a given client row. */
	TSharedRef<ITableRow> OnGenerateDiscoveredClientsRow(TSharedPtr<FLiveLinkHubClientId> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		FText ClientName = LOCTEXT("InvalidClient", "Invalid Client");
		if (ClientsModel && Item)
		{
			ClientName = ClientsModel->GetClientDisplayName(*Item);
		}

		return SNew(STableRow<TSharedPtr<FLiveLinkHubClientId>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(ClientName)
		];
	}

	/** Handler used to generate a widget for a given client row. */
	TSharedRef<ITableRow> OnGenerateClientRow(FClientTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SLiveLinkHubClientsRow, OwnerTable)
			.Item(Item);
	}

	/** Handler used to create the context menu widget. */
	TSharedPtr<SWidget> OnContextMenuOpening() const
	{
		constexpr bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder( CloseAfterSelection, nullptr);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Remove", "Remove selected client"),
			LOCTEXT("RemoveClientTooltip", "Stop transmitting LiveLink data to this client."),
			FSlateIcon("LiveLinkStyle", "LiveLinkClient.Common.RemoveSource"),
			FUIAction(
				FExecuteAction::CreateRaw(this, &SLiveLinkHubClientsView::RemoveSelectedClient),
				FCanExecuteAction::CreateStatic( [](){ return true; } )
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveAll", "Remove all clients"),
			LOCTEXT("RemoveAllClientTooltip", "Stop transmitting LiveLink data to all discovered clients."),
			FSlateIcon("LiveLinkStyle", "LiveLinkClient.Common.RemoveSource"),
			FUIAction(
				FExecuteAction::CreateRaw(this, &SLiveLinkHubClientsView::RemoveAllClients),
				FCanExecuteAction::CreateStatic([]() { return true; })
			)
		);


		return MenuBuilder.MakeWidget();
	}

	/** Handler called when selection changes in the list view. */
	void OnSelectionChanged(FClientTreeItemPtr InItem, const ESelectInfo::Type InSelectInfoType) const
	{
		if (InItem)
		{
			OnClientSelectedDelegate.ExecuteIfBound(InItem->ClientId);
		}
	}

	/** Handler called to fetch a tree row's children. */
	void OnGetChildren(FClientTreeItemPtr Item, TArray<FClientTreeItemPtr>& OutChildren)
	{
		OutChildren.Append(Item->Children);
	}

	/** Method to handle deleting clients when the delete key is pressed. */
    FReply OnKeyDownHandler(const FGeometry&, const FKeyEvent& InKeyEvent) const
    {
		if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
    	{
			if (TOptional<FLiveLinkHubClientId> Id = GetSelectedClient())
			{
				OnRemoveClientFromSessionDelegate.ExecuteIfBound(*Id);
			}
    		return FReply::Handled();
    	}

    	return FReply::Unhandled();
    }

	/** Remove the selected client from the list. This will stop all livelink messages from being transmitted to it. */
	void RemoveSelectedClient() const
	{
		if (TOptional<FLiveLinkHubClientId> Id = GetSelectedClient())
		{
			OnRemoveClientFromSessionDelegate.ExecuteIfBound(*Id);
		}
	}

	/** Remove all clients from the list. This will stop all livelink messages from being transmitted to them. */
	void RemoveAllClients() const
	{
		TArray<FLiveLinkHubClientId> ClientIds;
		ClientIds.Reserve(Clients.Num());

		Algo::TransformIf(Clients, ClientIds, [](FClientTreeItemPtr ClientPtr) { return !!ClientPtr; }, [](FClientTreeItemPtr ClientPtr) { return ClientPtr->ClientId; });
		
		Algo::ForEach(ClientIds, [this](const FLiveLinkHubClientId& ClientId)
		{
			OnRemoveClientFromSessionDelegate.ExecuteIfBound(ClientId);
		});
	}


	/** Handler called when a client is picked in the Add Client menu. */
	void OnDiscoveredClientPicked(TSharedPtr<FLiveLinkHubClientId> InItem, const ESelectInfo::Type InSelectInfoType)
	{
		if (InItem && TreeView)
		{
			OnDiscoveredClientPickedDelegate.ExecuteIfBound(*InItem);
			FSlateApplication::Get().SetUserFocus(0, TreeView);
		}
	}

	/** Handler called when the client list has changed. */
	void OnClientEvent(FLiveLinkHubClientId ClientId, ILiveLinkHubClientsModel::EClientEventType EventType)
	{
		switch (EventType)
		{
		case ILiveLinkHubClientsModel::EClientEventType::Discovered:
		{
			if (!DiscoveredClients.ContainsByPredicate([ClientId](const TSharedPtr<FLiveLinkHubClientId>& InClient) { return *InClient == ClientId; }))
			{
				DiscoveredClients.Add(MakeShared<FLiveLinkHubClientId>(MoveTemp(ClientId)));

				TWeakPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> WeakList = DiscoveredClientsListView;
				AsyncTask(ENamedThreads::GameThread, [WeakList]() 
				{
					if (TSharedPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> List = WeakList.Pin())
					{
						List->RequestListRefresh();
					}
				});
			}
			break;
		}
		case ILiveLinkHubClientsModel::EClientEventType::Disconnected:
		{
			int32 ClientIndex = DiscoveredClients.IndexOfByPredicate([ClientId](const TSharedPtr<FLiveLinkHubClientId>& InClient) { return *InClient == ClientId; });
			if (ClientIndex != INDEX_NONE)
			{
				DiscoveredClients.RemoveAt(ClientIndex);

				TWeakPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> WeakList = DiscoveredClientsListView;
				AsyncTask(ENamedThreads::GameThread, [WeakList]()
					{
						if (TSharedPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> List = WeakList.Pin())
						{
							List->RequestListRefresh();
						}
					});
			}
			break;
		}
		case ILiveLinkHubClientsModel::EClientEventType::Reestablished:
		{

			if (FLiveLinkHubModule* LiveLinkHubModule = FModuleManager::Get().GetModulePtr<FLiveLinkHubModule>("LiveLinkHub"))
			{
				if (TSharedPtr<ILiveLinkHubSessionManager> SessionManager = LiveLinkHubModule->GetSessionManager())
				{
					if (SessionManager->GetCurrentSession()->IsClientInSession(ClientId))
					{
						// Client is in session, add it to the clients, remove from discovered list
						int32 DiscoveredClientIndex = DiscoveredClients.IndexOfByPredicate([ClientId](const TSharedPtr<FLiveLinkHubClientId>& InClient) { return *InClient == ClientId; });
						if (DiscoveredClientIndex != INDEX_NONE)
						{
							DiscoveredClients.RemoveAt(DiscoveredClientIndex);
						}

						int32 ClientIndex = Clients.IndexOfByPredicate([ClientId](const FClientTreeItemPtr& InClient) { return InClient->ClientId == ClientId; });
						if (ClientIndex == INDEX_NONE)
						{
							TSharedPtr<FClientTreeViewClientItem> ClientItem = MakeShared<FClientTreeViewClientItem>(ClientId, ClientsModel.ToSharedRef());
							ClientItem->ClientId = ClientId;
							InitializeClientItem(*ClientItem);
							Clients.Add(ClientItem);
						}
					}
					else
					{
						// Client is not in session, add it to discovered clients if not already there
						int32 ClientIndex = DiscoveredClients.IndexOfByPredicate([ClientId](const TSharedPtr<FLiveLinkHubClientId>& InClient) { return *InClient == ClientId; });
						if (ClientIndex == INDEX_NONE)
						{
							DiscoveredClients.Add(MakeShared<FLiveLinkHubClientId>(ClientId));
						}
					}
				}
			}


			TWeakPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> WeakDiscoveredList = DiscoveredClientsListView;
			TWeakPtr<STreeView<FClientTreeItemPtr>> WeakClients = TreeView;
			AsyncTask(ENamedThreads::GameThread, [WeakDiscoveredList, WeakClients]()
			{
				if (TSharedPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> List = WeakDiscoveredList.Pin())
				{
					List->RequestListRefresh();
				}

				if (TSharedPtr<STreeView<FClientTreeItemPtr>> ClientsList = WeakClients.Pin())
				{
					ClientsList->RequestTreeRefresh();
				}
			});
			break;
		}
		case ILiveLinkHubClientsModel::EClientEventType::Modified:
		{
			break;
		}
		default:
		{
			checkNoEntry();
		}
		}
	}

	/** Handler called when a client is added to a session. */
	void OnClientAddedToSession(FLiveLinkHubClientId ClientId)
	{
		int32 ClientIndex = DiscoveredClients.IndexOfByPredicate([ClientId](const TSharedPtr<FLiveLinkHubClientId>& InClient) { return *InClient == ClientId; });
		if (ClientIndex != INDEX_NONE)
		{
			DiscoveredClients.RemoveAt(ClientIndex);
		}

		TSharedPtr<FClientTreeViewClientItem> ClientItem = MakeShared<FClientTreeViewClientItem>(ClientId, ClientsModel.ToSharedRef());
		ClientItem->ClientId = ClientId;
		InitializeClientItem(*ClientItem);
		Clients.Add(ClientItem);

		TreeView->RequestTreeRefresh();
		DiscoveredClientsListView->RequestListRefresh();
	}

	/** Handler called when a client is removed from a session. */
	void OnClientRemovedFromSession(FLiveLinkHubClientId ClientId)
	{
		int32 ClientIndex = Clients.IndexOfByPredicate([ClientId](const FClientTreeItemPtr& InClient) { return InClient->ClientId == ClientId; });
		if (ClientIndex != INDEX_NONE)
		{
			Clients.RemoveAt(ClientIndex);
		}

		if (ClientsModel->IsClientConnected(ClientId))
		{
			DiscoveredClients.Add(MakeShared<FLiveLinkHubClientId>(ClientId));
			DiscoveredClientsListView->RequestListRefresh();
		}

		TreeView->RequestTreeRefresh();
	}

	/** Refreshes the data when the current session changes. */
	void OnActiveSessionChanged(const TSharedRef<ILiveLinkHubSession>& ActiveSession)
	{
		Reinitialize();
	}

	/** Populate a client item row with its data and children. */
	void InitializeClientItem(FClientTreeViewClientItem& ClientItem)
	{
		const FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		constexpr bool bIncludeDisabledSubject = true;
		constexpr bool bIncludeVirtualSubject = true;

		TArray<FLiveLinkSubjectKey> LiveLinkSubjects = LiveLinkClient.GetSubjects(bIncludeDisabledSubject, bIncludeVirtualSubject);
		ClientItem.Children.Reserve(LiveLinkSubjects.Num());

		for (const FLiveLinkSubjectKey& SubjectKey : LiveLinkSubjects)
		{
			TSharedPtr<FClientTreeViewSubjectItem> SubjectItem = MakeShared<FClientTreeViewSubjectItem>(ClientItem.ClientId, SubjectKey, ClientsModel.ToSharedRef());
			ClientItem.Children.Add(SubjectItem);
		}
	}

	/** AnyThread handler for the SubjectAdded delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectAdded_AnyThread(FLiveLinkSubjectKey SubjectKey)
	{
		TWeakPtr<SLiveLinkHubClientsView> Self = StaticCastSharedRef<SLiveLinkHubClientsView>(AsShared());
		AsyncTask(ENamedThreads::GameThread, [Self, Key = MoveTemp(SubjectKey)]
		{
			if (TSharedPtr<SLiveLinkHubClientsView> View = Self.Pin())
			{
				View->OnSubjectAdded(Key);
			}
		});
	}

	/** Handles updating the tree view when a subject is added. */
	void OnSubjectAdded(const FLiveLinkSubjectKey& SubjectKey)
	{
		for (const FClientTreeItemPtr& Client : Clients)
		{
			TSharedPtr<FClientTreeViewSubjectItem> SubjectItem = MakeShared<FClientTreeViewSubjectItem>(Client->ClientId, SubjectKey, ClientsModel.ToSharedRef());
			SubjectItem->LiveLinkSubjectKey = SubjectKey;

			if (!Client->Children.ContainsByPredicate([&](const TSharedPtr<FClientTreeViewItem>& Child)
			{
				return Child->GetSubjectKey() == SubjectKey;
			}))
			{
				Client->Children.Add(SubjectItem);
			}
		}

		TreeView->RequestTreeRefresh();
	}

	/** AnyThread handler for the SubjectRemoved delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectRemoved_AnyThread(FLiveLinkSubjectKey SubjectKey)
	{
		TWeakPtr<SLiveLinkHubClientsView> Self = StaticCastSharedRef<SLiveLinkHubClientsView>(AsShared());
		AsyncTask(ENamedThreads::GameThread, [Self, Key = MoveTemp(SubjectKey)]
		{
			if (TSharedPtr<SLiveLinkHubClientsView> View = Self.Pin())
			{
				View->OnSubjectRemoved(Key);
			}
		});
	}

	/** Handles updating the tree view when a subject is removed. */
	void OnSubjectRemoved(const FLiveLinkSubjectKey& SubjectKey)
	{
		for (const FClientTreeItemPtr& Client : Clients)
		{
			int32 Index = Client->Children.IndexOfByPredicate([SubjectKey](const TSharedPtr<FClientTreeViewItem>& Item) { return Item->GetSubjectKey() == SubjectKey; });
			if (Index != INDEX_NONE)
			{
				Client->Children.RemoveAt(Index);
			}
		}

		TreeView->RequestTreeRefresh();
	}

	/** Build the client list. */
	void PopulateClients()
	{
		TArray<FLiveLinkHubClientId> ClientList = ClientsModel->GetSessionClients();
		Clients.Reset(ClientList.Num());

		TArray<FLiveLinkHubClientId> DiscoveredClientList = ClientsModel->GetDiscoveredClients();
		DiscoveredClients.Reset(DiscoveredClientList.Num());

		for (const FLiveLinkHubClientId& Client : DiscoveredClientList)
		{
			DiscoveredClients.Add(MakeShared<FLiveLinkHubClientId>(Client));
		}

		for (const FLiveLinkHubClientId& Client : ClientList)
		{
			TSharedPtr<FClientTreeViewClientItem> ClientItem = MakeShared<FClientTreeViewClientItem>(Client, ClientsModel.ToSharedRef());
			ClientItem->ClientId = Client;
			InitializeClientItem(*ClientItem);
			Clients.Add(ClientItem);
		}
	}

private:
	/** Delegate called when a client is selected. */
	FOnClientSelected OnClientSelectedDelegate;
	/** Delegate called when a client is selected. */
	FOnDiscoveredClientPicked OnDiscoveredClientPickedDelegate;
	/** Delegate called when a client is deleted. */
	FOnRemoveClientFromSession OnRemoveClientFromSessionDelegate;
	/** TreeView widget that displays the clients. */
	TSharedPtr<STreeView<FClientTreeItemPtr>> TreeView;
	/** ListView widget that displays clients discovered by the hub that aren't in the current session. */
	TSharedPtr<SListView<TSharedPtr<FLiveLinkHubClientId>>> DiscoveredClientsListView;
	/** List of clients in the current session. */
	TArray<FClientTreeItemPtr> Clients;
	/** Clients discovered by the hub. */
	TArray<TSharedPtr<FLiveLinkHubClientId>> DiscoveredClients;
	/** Model that holds the client data we are displaying. */
	TSharedPtr<ILiveLinkHubClientsModel> ClientsModel;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.ClientsView */
