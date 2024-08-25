// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EClientViewType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class SWidgetSwitcher;

namespace UE::MultiUserClient
{
	class FRemoteReplicationClient;
	
	/**
	 * Wraps a SComboButton that is used to select which SReplicationClientView should be displayed in a SWidgetSwitcher.
	 */
	class SSelectClientViewComboButton : public SCompoundWidget
	{
	public:
		
		DECLARE_DELEGATE_OneParam(FSelectClient, const FGuid&);
		DECLARE_DELEGATE(FSelectAllClients);
		
		SLATE_BEGIN_ARGS(SSelectClientViewComboButton)
		{}
			/** Uses to query local and remote client display info. */
			SLATE_ARGUMENT(TSharedPtr<IConcertClient>, Client)
		
			/** Remote clients that can be selected from in the order that they should be displayed. */
			SLATE_ATTRIBUTE(TArray<FGuid>, SelectableClients)
			/** The client that is currently selected. Only valid if EButtonContent == LocalClient or RemoteClient. */
			SLATE_ATTRIBUTE(FGuid, CurrentSelection)
			/** Determines what is being displayed in the main view right now. */
			SLATE_ATTRIBUTE(EClientViewType, CurrentDisplayMode)

			/** Called when a client is selected */
			SLATE_EVENT(FSelectClient, OnSelectClient)
			/** Called when all clients are supposed to be displayed */
			SLATE_EVENT(FSelectAllClients, OnSelectAllClients)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		/** Uses to query local and remote client display info. */
		TSharedPtr<IConcertClient> Client;
		
		/** Remote clients that can be selected from */
		TAttribute<TArray<FGuid>> ClientsAttribute;
		/** The current client selection */
		TAttribute<FGuid> CurrentSelection;
		/** Determines what is being displayed in the main view right now. */
		TAttribute<EClientViewType> CurrentDisplayMode;
		
		/** Called when a client is selected */
		FSelectClient OnSelectClientDelegate;
		/** Called when all clients are supposed to be displayed */
		FSelectAllClients OnSelectAllClients;

		/** Content for the button */
		TSharedPtr<SWidgetSwitcher> ButtonContent;
		
		TSharedRef<SWidget> MakeMenuContent();
		TSharedRef<SWidget> MakeAllClientsDisplayWidget() const;
		
		int32 GetActiveWidgetIndex() const;
		FGuid GetSelectedClientEndpointId() const;
	};
}

