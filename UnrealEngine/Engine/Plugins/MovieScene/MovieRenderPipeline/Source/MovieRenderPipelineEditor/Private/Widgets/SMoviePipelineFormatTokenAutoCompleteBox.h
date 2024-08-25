// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyHandle.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Views/SListView.h"

/** This is a specific auto-complete widget made for the MoviePipeline that isn't very flexible. It's
* similar to SSuggestionTextBox, but SSuggestionTextBox doesn't handle more than one word/suggestions
* mid string. This widget is hardcoded to look for '{' characters (for {format_tokens}) and then auto
* completes them from a list and fixes up the {} braces.
*/
class SMoviePipelineFormatTokenAutoCompleteBox : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMoviePipelineFormatTokenAutoCompleteBox){}

	SLATE_ARGUMENT(FText, InitialText)
	SLATE_ATTRIBUTE(TArray<FString>, Suggestions)
	/** Called whenever the text is changed programmatically or interactively by the user. */
	SLATE_EVENT(FOnTextChanged, OnTextChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;
	// End of SWidget interface

	void SetText(const FText& InText);

	void OnItemClicked(TSharedPtr<FString>);

	static void FindAutoCompletableTextAtPos(const FString& InWholeString, int32 InCursorPos, FString& OutStr, bool& bShowAutoComplete);

	void ReplaceRelevantTextWithSuggestion(const FString& InSuggestionText) const;

	void HandleTextBoxTextChanged(const FText& InText);

	void FilterVisibleSuggestions(const FString& StrToMatch, const bool bForceShowAll);

	void CloseMenuAndReset();

	void SetActiveSuggestionIndex(int32 InIndex);

	TSharedRef<ITableRow> HandleSuggestionListViewGenerateRow(TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable) const;

private:
	TSharedPtr<SListView<TSharedPtr<FString>>> SuggestionListView;
	TSharedPtr<SMultiLineEditableTextBox> TextBox;
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<SVerticalBox> VerticalBox;
	// Holds a delegate that is executed when the text has changed.
	FOnTextChanged OnTextChanged;

	// The pool of suggestions to show
	TArray<FString> AllSuggestions;
	// The currently filtered suggestion list
	TArray<TSharedPtr<FString>> Suggestions;
	int32 CurrentSuggestionIndex = -1;
};
