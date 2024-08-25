// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMoviePipelineFormatTokenAutoCompleteBox.h"

#include "DetailLayoutBuilder.h"
#include "Layout/WidgetPath.h"

void SMoviePipelineFormatTokenAutoCompleteBox::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew(MenuAnchor, SMenuAnchor)
		.Placement(MenuPlacement_ComboBox)
		[
			SAssignNew(TextBox, SMultiLineEditableTextBox)
			.Text(InArgs._InitialText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OnKeyDownHandler(this, &SMoviePipelineFormatTokenAutoCompleteBox::OnKeyDown)
			.OnTextChanged(this, &SMoviePipelineFormatTokenAutoCompleteBox::HandleTextBoxTextChanged)
			.SelectWordOnMouseDoubleClick(true)
			.AllowMultiLine(false)
		]
		.MenuContent
		(
			SNew(SBorder)
			.Padding(FMargin(2))
			[
				SAssignNew(VerticalBox, SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SuggestionListView, SListView<TSharedPtr<FString>>)
					.ItemHeight(18.f)
					.ListItemsSource(&Suggestions)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SMoviePipelineFormatTokenAutoCompleteBox::HandleSuggestionListViewGenerateRow)
					.OnMouseButtonClick(this, &SMoviePipelineFormatTokenAutoCompleteBox::OnItemClicked)
				]
			]
		)
	];

	// We just call it once and cache it for now as the selection code isn't tested against
	// the amount of suggestions changing.
	AllSuggestions.Append(InArgs._Suggestions.Get());
	OnTextChanged = InArgs._OnTextChanged;
}

void SMoviePipelineFormatTokenAutoCompleteBox::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	// Close the suggestion list if the text box has lost focus to anything other than the suggestion list
	if (PreviousFocusPath.ContainsWidget(TextBox.Get()) && !NewWidgetPath.ContainsWidget(SuggestionListView.Get()))
	{
		CloseMenuAndReset();
	}
	SWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);
}

FReply SMoviePipelineFormatTokenAutoCompleteBox::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (MenuAnchor->IsOpen())
	{
		if (KeyEvent.GetKey() == EKeys::Up)
		{
			// Because the pop-up dialog is below the text, 'up' actually goes to an earlier item in the list.
			int32 NewSuggestionIndex = CurrentSuggestionIndex - 1;
			if (NewSuggestionIndex < 0)
			{
				NewSuggestionIndex = Suggestions.Num() - 1;
			}

			SetActiveSuggestionIndex(NewSuggestionIndex);
			return FReply::Handled();
		}
		else if(KeyEvent.GetKey() == EKeys::Down)
		{
			int32 NewSuggestionIndex = CurrentSuggestionIndex + 1;
			if (NewSuggestionIndex > Suggestions.Num() - 1)
			{
				NewSuggestionIndex = 0;
			}

			SetActiveSuggestionIndex(NewSuggestionIndex);
			return FReply::Handled();
		}
		else if (KeyEvent.GetKey() == EKeys::Escape)
		{
			CloseMenuAndReset();
			return FReply::Handled();
		}
		else if (KeyEvent.GetKey() == EKeys::Tab || KeyEvent.GetKey() == EKeys::Enter)
		{
			if(CurrentSuggestionIndex >= 0 && CurrentSuggestionIndex <= Suggestions.Num() - 1)
			{
				// Trigger the auto-complete for the highlighted suggestion
				const FString SuggestionText = *Suggestions[CurrentSuggestionIndex];
				ReplaceRelevantTextWithSuggestion(SuggestionText);
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

void SMoviePipelineFormatTokenAutoCompleteBox::SetText(const FText& InText)
{
	// SetText always sets the cursor location to the end of the line, so we need to cache and restore the location
	const FTextLocation OriginalCursorLocation = TextBox->GetCursorLocation();
	TextBox->SetText(InText);
	TextBox->GoTo(OriginalCursorLocation);
}

void SMoviePipelineFormatTokenAutoCompleteBox::OnItemClicked(TSharedPtr<FString> Item)
{
	ReplaceRelevantTextWithSuggestion(*Item);
	CloseMenuAndReset();
}

void SMoviePipelineFormatTokenAutoCompleteBox::FindAutoCompletableTextAtPos(const FString& InWholeString, int32 InCursorPos, FString& OutStr, bool& bShowAutoComplete)
{
	OutStr = FString();
	bShowAutoComplete = false;

	// We want to find a { brace on or to the left of InCursorPos, but if we find a } we
	// stop looking, because that's a brace for another text. (+1 for ::FromEnd off by one)
	const int32 StartingBracePos = InWholeString.Find(
		TEXT("{"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, InCursorPos + 1);
	const int32 PreviousEndBracePos = InWholeString.Find(
		TEXT("}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, InCursorPos);

	if (StartingBracePos < PreviousEndBracePos)
	{
		return;
	}

	FString AutoCompleteText;

	// Now that we found a {, take the substring between it and either the next }, or the end of the string.
	const int32 NextEndBracePos = InWholeString.Find(TEXT("}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromStart, InCursorPos);
	if (StartingBracePos >= 0)
	{
		int32 Count = InWholeString.Len() - StartingBracePos;
		if (NextEndBracePos >= 0)
		{
			Count = NextEndBracePos - StartingBracePos;
		}

		AutoCompleteText = InWholeString.Mid(StartingBracePos + 1, Count - 1);
	}

	OutStr = AutoCompleteText;
	bShowAutoComplete = StartingBracePos >= 0 && OutStr.Len() == 0;
}

void SMoviePipelineFormatTokenAutoCompleteBox::ReplaceRelevantTextWithSuggestion(const FString& InSuggestionText) const
{
	FString TextBoxText = TextBox->GetText().ToString();
	int32 CursorPos = TextBoxText.Len();
	const FTextLocation CursorLoc = TextBox->GetCursorLocation();
	if (CursorLoc.IsValid())
	{
		CursorPos = FMath::Clamp(CursorLoc.GetOffset(), 0, CursorPos);
	}
		
	// Look for the { to the left of the cursor. We search StrPositionIndex from +1 here due to a bug in ::FromEnd being off by one.
	const int32 StartingBracePos = TextBoxText.Find(TEXT("{"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, CursorPos + 1);

	// Now that we found a {, take the substring between it and either the next }, or the end of the string.
	const int32 NextEndBracePos = TextBoxText.Find(TEXT("}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromStart, CursorPos);
	int32 NewCursorPos = 0;
	if (StartingBracePos >= 0)	
	{
		// +1 to keep the left { brace
		const FString Left = TextBoxText.Left(StartingBracePos+1);
		FString Right;

		if (NextEndBracePos >= 0)
		{
			Right = TextBoxText.RightChop(NextEndBracePos);
		}

		// Since the user chose the suggestion ensure there's already a } brace to close off the pair.
		if (!Right.StartsWith(TEXT("}")))
		{
			Right = FString::Printf(TEXT("}%s"), *Right);
		}

		TextBoxText = Left + InSuggestionText + Right;
		// We subtract 1 from the Right as we want to put the cursor after the automatically generated "}" token.
		NewCursorPos = TextBoxText.Len() - (Right.Len() - 1);
	}

	TextBox->SetText(FText::FromString(TextBoxText));
	TextBox->GoTo(FTextLocation(0, NewCursorPos));
}

void SMoviePipelineFormatTokenAutoCompleteBox::HandleTextBoxTextChanged(const FText& InText)
{
	OnTextChanged.ExecuteIfBound(InText);

	const FString TextAsStr = InText.ToString();
	if (TextAsStr.Len() > 0)
	{
		FString OutStr;
		bool bShowAutoComplete;

		int32 CursorPos = TextAsStr.Len();
		const FTextLocation CursorLoc = TextBox->GetCursorLocation();
		if (CursorLoc.IsValid())
		{
			CursorPos = FMath::Clamp(CursorLoc.GetOffset(), 0, CursorPos);
		}

		FindAutoCompletableTextAtPos(TextAsStr, CursorPos, OutStr, bShowAutoComplete);
		FilterVisibleSuggestions(OutStr, bShowAutoComplete);
	}	
	else
	{
		// If they have no text, suggest all possible solutions
		FilterVisibleSuggestions(FString(), false);
	}
}

void SMoviePipelineFormatTokenAutoCompleteBox::FilterVisibleSuggestions(const FString& StrToMatch, const bool bForceShowAll)
{
	Suggestions.Reset();
	for (const FString& Suggestion : AllSuggestions)
	{
		if (Suggestion.Contains(StrToMatch) || bForceShowAll)
		{
			Suggestions.Add(MakeShared<FString>(Suggestion));
		}
	}

	if (Suggestions.Num() > 0)
	{
		// We don't focus the menu (because then you can't type on the keyboard) and instead
		// keep the focus on the text field and bubble the keyboard commands to it.
		constexpr bool bIsOpen = true;
		constexpr bool bFocusMenu = false;
		MenuAnchor->SetIsOpen(bIsOpen, bFocusMenu);
		SuggestionListView->RequestScrollIntoView(Suggestions[0]);
	}
	else
	{
		CloseMenuAndReset();
	}
}

void SMoviePipelineFormatTokenAutoCompleteBox::CloseMenuAndReset()
{
	constexpr bool bIsOpen = false;
	MenuAnchor->SetIsOpen(bIsOpen);

	// Reset their index when the drawer closes so that the first item is always selected when we re-open.
	CurrentSuggestionIndex = -1;
}

void SMoviePipelineFormatTokenAutoCompleteBox::SetActiveSuggestionIndex(int32 InIndex)
{
	if (InIndex < 0 || InIndex >= Suggestions.Num())
	{
		return;
	}

	const TSharedPtr<FString> Suggestion = Suggestions[InIndex];
	SuggestionListView->SetSelection(Suggestion);
	if (!SuggestionListView->IsItemVisible(Suggestion))
	{
		SuggestionListView->RequestScrollIntoView(Suggestion);
	}
	CurrentSuggestionIndex = InIndex;
}

TSharedRef<ITableRow> SMoviePipelineFormatTokenAutoCompleteBox::HandleSuggestionListViewGenerateRow(
	TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString SuggestionText = *Text;

	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SBox)
			[
				SNew(STextBlock)
				.Text(FText::FromString(SuggestionText))
			]
		];
}