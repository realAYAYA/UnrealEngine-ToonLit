// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"

class SConsoleVariablesEditorMainPanel;
class SEditableTextBox;

class SConsoleVariablesEditorCustomConsoleInputBox : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SConsoleVariablesEditorCustomConsoleInputBox)
	{}

	SLATE_END_ARGS()
	
	struct FSuggestions
	{
		FSuggestions()
			: SelectedSuggestion(INDEX_NONE)
		{
		}

		void Reset()
		{
			SelectedSuggestion = INDEX_NONE;
			SuggestionsList.Reset();
			SuggestionsHighlight = FText::GetEmpty();
		}

		bool HasSuggestions() const
		{
			return SuggestionsList.Num() > 0;
		}

		bool HasSelectedSuggestion() const
		{
			return SuggestionsList.IsValidIndex(SelectedSuggestion);
		}

		void StepSelectedSuggestion(const int32 Step)
		{
			SelectedSuggestion += Step;
			if (SelectedSuggestion < 0)
			{
				SelectedSuggestion = SuggestionsList.Num() - 1;
			}
			else if (SelectedSuggestion >= SuggestionsList.Num())
			{
				SelectedSuggestion = 0;
			}
		}

		TSharedPtr<FString> GetSelectedSuggestion() const
		{
			return SuggestionsList.IsValidIndex(SelectedSuggestion) ? SuggestionsList[SelectedSuggestion] : nullptr;
		}

		/** INDEX_NONE if not set, otherwise index into SuggestionsList */
		int32 SelectedSuggestion;

		/** All log messages stored in this widget for the list view */
		TArray<TSharedPtr<FString>> SuggestionsList;

		/** Highlight text to use for the suggestions list */
		FText SuggestionsHighlight;
	};

	void Construct(const FArguments& InArgs, TWeakPtr<SConsoleVariablesEditorMainPanel> InMainPanelWidget);

	virtual ~SConsoleVariablesEditorCustomConsoleInputBox() override;

	//~ Begin SWidget Interface
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;
	//~ End SWidget Interface

	bool TakeKeyboardFocus() const;

	void OnTextChanged(const FText& InText);

	FReply OnKeyCharHandler(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) const;

	FReply OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& KeyPressed);
	
	void SetSuggestions(TArray<FString>& Elements, FText Highlight);

	void MarkActiveSuggestion();

	void ClearSuggestions();
	
	void CommitInput();

private:

	TWeakPtr<SConsoleVariablesEditorMainPanel> MainPanelWidget;
	
	/** A reference to the actual text box inside ConsoleInput */
	TSharedPtr<SEditableTextBox> InputText;

	/** history / auto completion elements */
	TSharedPtr<SMenuAnchor> SuggestionBox;

	/** The list view for showing all log messages. Should be replaced by a full text editor */
	TSharedPtr<SListView<TSharedPtr<FString>>> SuggestionListView;

	/** Active list of suggestions */
	FSuggestions Suggestions;

	/** to prevent recursive calls in UI callback */
	bool bIgnoreUIUpdate = false;
};
