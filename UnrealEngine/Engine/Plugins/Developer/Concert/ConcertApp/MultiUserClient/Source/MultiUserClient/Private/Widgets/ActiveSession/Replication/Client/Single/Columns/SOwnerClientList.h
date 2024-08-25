// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate { class SHorizontalClientList; }

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;
	
	/**
	 * Displays a list of authoritive clients in the SReplicationClientView columns for top-level, subobject and properties;
	 * the passed in FGetClientList determines which is displayed
	 */
	class SOwnerClientList : public SCompoundWidget
	{
	public:

		DECLARE_DELEGATE_RetVal_OneParam(TArray<FGuid>, FGetClientList, const FGlobalAuthorityCache&);
		
		SLATE_BEGIN_ARGS(SOwnerClientList)
		{}
			/** Obtains the client list to display */
			SLATE_EVENT(FGetClientList, GetClientList)
			/** Used for highlighting in the text */
			SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient, FGlobalAuthorityCache& InAuthorityCache);
		virtual ~SOwnerClientList();

	private:
		
		/** Tells us when the authority change */
		FGlobalAuthorityCache* AuthorityCache;

		/** Displays the clients */
		TSharedPtr<ConcertClientSharedSlate::SHorizontalClientList> ClientList;

		/** Used to get the client list from FGlobalAuthorityCache */
		FGetClientList GetClientListDelegate;

		void OnClientChanged(const FGuid& ChangedClientId);
		void RefreshList();
	};
}


