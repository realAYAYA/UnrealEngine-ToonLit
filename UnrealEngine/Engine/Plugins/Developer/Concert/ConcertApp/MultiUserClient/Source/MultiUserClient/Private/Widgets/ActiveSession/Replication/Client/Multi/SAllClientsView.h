// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllClientsSelectionModel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::MultiUserClient
{
	class FReplicationClient;
	class FReplicationClientManager;

	/** Leverages SMultiClientView to display all replication clients. */
	class SAllClientsView : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SAllClientsView)
			{}
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager);

	private:

		/** Used to get all the replication clients and listen for client changes. */
		FReplicationClientManager* ClientManager = nullptr;

		/** Keeps the SMultiClientView updated of any changes to clients (e.g. disconnects, etc.) */
		TUniquePtr<FAllClientsSelectionModel> AllClientsModel;

		/** Gets all the clients to display */
		TSet<const FReplicationClient*> GetAllClients() const;
	};
}
