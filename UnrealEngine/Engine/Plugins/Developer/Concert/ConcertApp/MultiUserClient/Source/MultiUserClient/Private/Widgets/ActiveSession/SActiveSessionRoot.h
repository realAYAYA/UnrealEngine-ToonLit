// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FSpawnTabArgs;
class FTabManager;
class FWorkspaceItem;
class IConcertSyncClient;
class SDockTab;
class SWindow;

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;
	
	/**
	 * Displayed when the client is connected to an active session.
	 * Manages the child content in tabs.
	 */
	class SActiveSessionRoot : public SCompoundWidget
	{
	public:
		
		static const FName OverviewTabId;
		static const FName ReplicationTabId;

		SLATE_BEGIN_ARGS(SActiveSessionRoot)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, TSharedPtr<IConcertSyncClient> InConcertSyncClient, TSharedRef<FMultiUserReplicationManager> InReplicationManager);

	private:

		TSharedPtr<IConcertSyncClient> ConcertSyncClient;
		/** Holds the child content: SActiveSessionOverviewTab and SReplicationControlsTab. */
		TSharedPtr<FTabManager> TabManager;
		
		void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager, const TSharedRef<FWorkspaceItem>& AppMenuGroup, TSharedRef<FMultiUserReplicationManager> InReplicationManager);
		TSharedRef<SDockTab> SpawnTab_Overview(const FSpawnTabArgs& Args);
		TSharedRef<SDockTab> SpawnTab_ReplicationControls(const FSpawnTabArgs& Args, TSharedRef<FMultiUserReplicationManager> InReplicationManager);
	};
}


