// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/AppStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class SScrollBox;
class SWidgetSwitcher;

struct FConcertSessionClientInfo;
struct FGuid;

namespace UE::ConcertClientSharedSlate
{
	/** Aligns client widgets from left to right. If there is not enough space, a horizontal scroll bar cuts of the list. */
	class CONCERTCLIENTSHAREDSLATE_API SHorizontalClientList : public SCompoundWidget
	{
	public:

		DECLARE_DELEGATE_RetVal_TwoParams(bool, FSortPredicate, const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& FConcertSessionClientInfo);
		static bool SortLocalClientFirstThenAlphabetical(const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right, TSharedRef<IConcertClient> Client);

		/** @return The display string a SHorizontalClientList would display with the given state. Returns unset optional if EmptyListSlot would be shown. */
		static TOptional<FString> GetDisplayString(const IConcertClient& LocalConcertClient, const TConstArrayView<FGuid>& Clients, const FSortPredicate& SortPredicate);
		
		SLATE_BEGIN_ARGS(SHorizontalClientList)
			: _Font(FAppStyle::Get().GetFontStyle("NormalFont"))
		{}
			/** Defaults to placing the local client first (if contained) and sorting alphabetically otherwise. */
			SLATE_EVENT(FSortPredicate, SortPredicate)
			/** Used for highlighting in the text */
			SLATE_ATTRIBUTE(FText, HighlightText)
			/** The widget to display when the list is empty */
			SLATE_NAMED_SLOT(FArguments, EmptyListSlot)
			/** The font to use for the names */
			SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient);

		/** Refreshes the list. */
		void RefreshList(const TConstArrayView<FGuid>& Clients);

	private:

		/** Clients used to look up client names by ID. */
		TSharedPtr<IConcertClient> LocalConcertClient;

		/** Sorts the client list */
		FSortPredicate SortPredicateDelegate;
		/** Used for highlighting in the text */
		TAttribute<FText> HighlightTextAttribute;
		/** The font to use for the names */
		FSlateFontInfo NameFont;

		/** Displays the ScrollBox when there are clients and the EmptyListSlot otherwise. */
		TSharedPtr<SWidgetSwitcher> WidgetSwitcher;
		/** Contains the children. */
		TSharedPtr<SScrollBox> ScrollBox;
	};
}