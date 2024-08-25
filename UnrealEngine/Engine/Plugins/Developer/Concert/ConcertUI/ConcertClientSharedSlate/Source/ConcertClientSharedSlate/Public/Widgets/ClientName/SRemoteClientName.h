// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Displays the name of a client.
	 * 
	 * The name will look like "Client Name".
	 * @see SLocalClientName
	 *
	 * If the client disconnects, the last known info is used.
	 * If the client info is unknown, the widget will display an empty FConcertClientInfo;
	 */
	class CONCERTCLIENTSHAREDSLATE_API SRemoteClientName : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SRemoteClientName)
			: _Font(FAppStyle::Get().GetFontStyle("BoldFont"))
		{}
			SLATE_ATTRIBUTE(FGuid, ClientEndpointId)
			/** Used for highlighting in the text */
			SLATE_ATTRIBUTE(FText, HighlightText)
			/** The font to use for the name */
			SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient);

	private:

		/** The displayed local client. */
		TSharedPtr<IConcertClient> Client;
		/** The endpoint ID of the client to display. */
		TAttribute<FGuid> ClientEndpointIdAttribute;

		/**
		 * Cached so that the info remains known when the client disconnects.
		 * Must be mutable because TAttribute::CreateSP requires GetClientInfo to be const.
		 */
		mutable FConcertSessionClientInfo LastKnownClientInfo;

		/** Gets the display info. */
		const FConcertClientInfo* GetClientInfo() const;
	};
}
