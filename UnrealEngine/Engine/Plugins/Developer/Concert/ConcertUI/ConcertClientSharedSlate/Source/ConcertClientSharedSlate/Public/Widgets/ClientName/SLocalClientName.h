// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Displays the name of a client.
	 * 
	 * The name will look like "Client Name (me)".
	 * @see SRemoteClientName
	 */
	class CONCERTCLIENTSHAREDSLATE_API SLocalClientName : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SLocalClientName)
			: _Font(FAppStyle::Get().GetFontStyle("BoldFont"))
		{}
			/** Used for highlighting in the text */
			SLATE_ATTRIBUTE(FText, HighlightText)
			/** The font to use for the name */
			SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient);
	};
}
