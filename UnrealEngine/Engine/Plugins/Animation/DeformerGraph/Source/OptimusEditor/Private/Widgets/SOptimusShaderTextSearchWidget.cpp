// Copyright Epic Games, Inc. All Rights Reserved.
#include "SOptimusShaderTextSearchWidget.h"

#include "OptimusEditorStyle.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"

#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "OptimusShaderTextSearchWidget"

SOptimusShaderTextSearchWidget::SOptimusShaderTextSearchWidget()
{
}

SOptimusShaderTextSearchWidget::~SOptimusShaderTextSearchWidget()
{
}

void SOptimusShaderTextSearchWidget::Construct(const FArguments& InArgs)
{
	
	const FSearchBoxStyle& SearchBoxStyle = FOptimusEditorStyle::Get().GetWidgetStyle<FSearchBoxStyle>(TEXT("TextEditor.SearchBoxStyle"));
	
	SearchBox =
		SNew(SSearchBox)
			.HintText(NSLOCTEXT("SearchBox", "HelpHint", "Search For Text"))
			.Style(&SearchBoxStyle)
			.OnTextChanged(InArgs._OnTextChanged)
			.OnTextCommitted(InArgs._OnTextCommitted)
			.SearchResultData(InArgs._SearchResultData)
			.SelectAllTextWhenFocused(true)
			.DelayChangeNotificationsWhileTyping(true)
			.MinDesiredWidth(200)
			.OnSearch(InArgs._OnResultNavigationButtonClicked);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(6.f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SearchBox.ToSharedRef()
			]
		]
	];
}

void SOptimusShaderTextSearchWidget::FocusSearchBox() const
{
	FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
}

void SOptimusShaderTextSearchWidget::TriggerSearch(const FText& InNewSearchText) const
{
	FocusSearchBox();

	// multiline search is not supported, sanitize the input to be single line text
	FString SingleLineString = InNewSearchText.ToString();
	{
		SingleLineString.GetCharArray().RemoveAll([&](const TCHAR InChar) -> bool
		{
			const bool bIsCharAllowed = !FChar::IsLinebreak(InChar);
			return !bIsCharAllowed;
		});
	}
	
	FText SingleLineSearchText = FText::FromString(SingleLineString);
	
	// clear the text to trigger a fresh search
	// sometimes, the search text can be the same but starting from different place
	SearchBox->SetText(FText::GetEmpty());
	
	if (InNewSearchText.IsEmpty())
	{
		SearchBox->SetText(LastSearchedText);
	}
	else
	{
		SearchBox->SetText(SingleLineSearchText);
	}

	SearchBox->SelectAllText();
}

void SOptimusShaderTextSearchWidget::ClearSearchText()
{
	// Save the last searched text so that
	// if the next search is triggered without any selected text,
	// we can initialize the search using it, standard behavior
	// in regular text editors
	LastSearchedText = SearchBox->GetText();
	SearchBox->SetText(FText::GetEmpty());
}


#undef LOCTEXT_NAMESPACE
