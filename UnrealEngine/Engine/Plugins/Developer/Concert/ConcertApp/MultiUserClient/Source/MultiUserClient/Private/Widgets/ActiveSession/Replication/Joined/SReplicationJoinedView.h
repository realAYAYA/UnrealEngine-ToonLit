// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertSyncClient;
class SNotificationItem;
class SWidgetSwitcher;

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamEditor;
}

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;
	class SReplicationClientView;

	/** This widget is displayed by SReplicationRootWidget when the client has joined replication. */
	class SReplicationJoinedView : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationJoinedView)
		{}
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
			TSharedRef<FMultiUserReplicationManager> InReplicationManager,
			TSharedRef<IConcertSyncClient> InClient
			);

	private:

		/** The local client this widget is created for. */
		TSharedPtr<IConcertSyncClient> Client;
		/** Acts as the model of this view */
		TSharedPtr<FMultiUserReplicationManager> ReplicationManager;
		
		/** Selects which client is being view. */
		TSharedPtr<SWidgetSwitcher> ClientViewSwitcher;
		/** Maps a remote client to index in ClientViewSwitcher. */
		TMap<FGuid, int32> RemoteClientToWidgetSwitcherIndex;
		
		/** Notification about in progress authority change, if any. */
		TSharedPtr<SNotificationItem> AuthorityChangeNotification;
		
		// Building ClientViewSwitcher
		/** Gets the remote clients and makes sure ClientViewSwitcher has a widget for each. */
		void RefreshClientViewSwitcher();
		/** Util for adding back old client widgets after ClientViewSwitcher was cleared. */
		void RebuildClientViewSwitcherChildren(const TArray<TSharedRef<SWidget>> OldClientWidgets);

		/** Warps the combo box with a text */
		TSharedRef<SWidget> MakeClientSelectionArea();
		/** Creates a combobox with which the content of ClientViewSwitcher can be changed. */
		TSharedRef<SWidget> MakeClientSelectionComboBox();
		TOptional<FGuid> GetRemoteClientBySwitcherIndex(int32 WidgetSwitcherIndex) const;
	};
}