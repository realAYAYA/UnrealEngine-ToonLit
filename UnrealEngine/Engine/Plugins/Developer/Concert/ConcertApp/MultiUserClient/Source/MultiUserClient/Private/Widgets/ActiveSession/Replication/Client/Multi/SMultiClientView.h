// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class FMenuBuilder;

namespace UE::ConcertSharedSlate
{
	class IObjectHierarchyModel;
	class IMultiReplicationStreamEditor;
	class IEditableReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	class FMultiStreamModel;
	class FReplicationClient;
	class FReplicationClientManager;
	class IClientSelectionModel;

	/** Displays a selection of clients. */
	class SMultiClientView
		: public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SMultiClientView)
		{}
			/** Dedicated space for a widget with which to change the view. */
			SLATE_NAMED_SLOT(FArguments, ViewSelectionArea)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager, IClientSelectionModel& InDisplayClientsModel);
		virtual ~SMultiClientView() override;

	private:

		FReplicationClientManager* ClientManager = nullptr;
		IClientSelectionModel* SelectionModel = nullptr;
		
		/** Combines the clients */
		TSharedPtr<FMultiStreamModel> StreamModel;
		/** Displayed in the UI. */
		TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor> StreamEditor;
		/** Used by widgets in columns. */
		TSharedPtr<ConcertSharedSlate::IObjectHierarchyModel> ObjectHierarchy;

		/** Creates this widget's editor content */
		TSharedRef<SWidget> CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FReplicationClientManager& InClientManager);

		// SClientToolbar attributes
		TSet<FGuid> GetDisplayClientIds() const;
		void EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const;
		
		void RebuildClientSubscriptions();
		void CleanClientSubscriptions() const;
		void OnClientChanged() const;
		void OnHierarchyNeedsRefresh() const;
		
		/** Adds additional entries to the context menu for the object tree view. */
		void ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const;
	};
}
