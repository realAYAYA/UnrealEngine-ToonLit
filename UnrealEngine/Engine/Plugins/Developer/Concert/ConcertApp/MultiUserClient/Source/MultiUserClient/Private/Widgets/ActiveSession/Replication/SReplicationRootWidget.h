// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SSplitter.h"

class IConcertSyncClient;

namespace UE::MultiUserClient
{
	class FMultiUserReplicationManager;
	
	enum class EMultiUserReplicationConnectionState : uint8;

	/** Root widget for replication in Multi-User session. */
	class SReplicationRootWidget : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationRootWidget)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager, TSharedRef<IConcertSyncClient> InClient);

	private:

		/** The client this widget was created for. */
		TSharedPtr<IConcertSyncClient> Client;
		/** Manages the business logic which we represent. */
		TSharedPtr<FMultiUserReplicationManager> ReplicationManager;
		
		/** Called when joining replication session state changes. */
		void OnReplicationConnectionStateChanged(EMultiUserReplicationConnectionState);

		void ShowWidget_Connecting();
		void ShowWidget_Connected();
		void ShowWidget_Disconnected();
	};
}

