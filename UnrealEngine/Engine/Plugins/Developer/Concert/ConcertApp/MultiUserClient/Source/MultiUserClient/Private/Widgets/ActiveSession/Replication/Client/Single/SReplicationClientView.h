// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

struct FConcertPropertyChain;
class IConcertClient;
class FMenuBuilder;

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamEditor;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FReplicationClient;
	class FReplicationClientManager;
	
	/** Displays the contents of a client. */
	class SReplicationClientView : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationClientView)
		{}
			/** The client to depict. Should always return something valid. If the client is destroyed, so should this widget be. */
			SLATE_ATTRIBUTE(FReplicationClient*, GetReplicationClient)
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<IConcertClient>& InClient, FReplicationClientManager& InClientManager);

	private:
		
		/** Used to rebuild Content */
		TSharedPtr<IConcertClient> ConcertClient;
		FReplicationClientManager* ClientManager;
		
		/** The editor view of the replication content. */
		TSharedPtr<ConcertSharedSlate::IReplicationStreamEditor> EditorView;
		
		/** The client to depict. Should always return true. If the client is destroyed, so should this widget be. */
		TAttribute<FReplicationClient*> GetReplicationClientAttribute;

		TSharedRef<SWidget> CreateContent(FReplicationClient& InReplicationClient);

		/** Adds additional entries to the context menu for the object tree view. */
		void ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const;
		/** Adds additional properties, e.g. struct child properties. */
		void ExtendPropertiesToAdd(const FSoftObjectPath& Object, TArray<FConcertPropertyChain>& InOutPropertiesToAdd) const;
		
		/** Called when any of the streams change. */
		void OnModelChanged() const;

		/** Lists all objects being replicated */
		void EnumerateReplicatedObjects(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const;
	};
}

