// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationBar.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SSuggestionTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

// Category of list view item to choose an icon
enum class ELocationSource
{
	History,
	Suggestion
};

// Type for list view control
struct FLocationItem
{
	FString VirtualPath;
	ELocationSource Source;
};

class SLocationListView : public SListView<TSharedPtr<FLocationItem>>
{
public:
	SLATE_BEGIN_ARGS(SLocationListView)
		{}
		SLATE_EVENT(FOnGenerateRow, OnGenerateRow)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnKeyDown, OnKeyDownHandler)
		SLATE_EVENT(FOnKeyChar, OnKeyCharHandler)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnKeyCharHandler = InArgs._OnKeyCharHandler;
		using SSuper = SListView<TSharedPtr<FLocationItem>>;
		SSuper::Construct(SSuper::FArguments()
			.ListItemsSource(&Items)
			.ItemHeight(18.0f)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(InArgs._OnGenerateRow)
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			.OnKeyDownHandler(InArgs._OnKeyDownHandler)
		);
	}

	// Forward typing events to the navigation bar so that focus can be moved back to the editable text box 
	FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
	{
		if (OnKeyCharHandler.IsBound())
		{
			return OnKeyCharHandler.Execute(MyGeometry, InCharacterEvent);
		}
		return FReply::Unhandled();
	}
	
	void SetItems(TArray<FString> InItems, ELocationSource InSource)
	{
		RequestListRefresh();
		ClearSelection();
		Items.Reset(InItems.Num());
		for (FString& Item : InItems)
		{
			Items.Emplace(MakeShared<FLocationItem>(FLocationItem{MoveTemp(Item), InSource}));
		}
	}
	
	FReply NavigateToFirst()
	{
		if (Items.Num() > 0)
		{
			SetSelection(Items[0], ESelectInfo::OnNavigation);
			return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::SetDirectly);
		}
		return FReply::Handled();
	}
	FReply NavigateToLast()
	{
		if (Items.Num() > 0)
		{
			SetSelection(Items.Last(), ESelectInfo::OnNavigation);
			return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::SetDirectly);
		}
		return FReply::Handled();
	}
	
protected:
	FOnKeyChar OnKeyCharHandler;
	TArray<TSharedPtr<FLocationItem>> Items;
};

void SNavigationBar::Construct(const FArguments& InArgs)
{
	OnGetComboOptions = InArgs._GetComboOptions;
	OnCompletePrefix = InArgs._OnCompletePrefix;
	OnNavigateToPath = InArgs._OnNavigateToPath;
	OnCanEditPathAsText = InArgs._OnCanEditPathAsText;

	ComboBoxStyle = InArgs._ComboBoxStyle;
	TextBoxStyle = InArgs._TextBoxStyle;
	TableRowStyle = InArgs._ItemStyle;
	bShowMenuBackground = false;
	
	const ISlateStyle& StyleSet = FAppStyle::Get();
	SuggestionIcon = StyleSet.GetBrush("Icons.Search");
	HistoryIcon = StyleSet.GetBrush("Icons.Recent");

	SComboButton::Construct(SComboButton::FArguments()
		.ComboButtonStyle(&ComboBoxStyle->ComboButtonStyle)
		.ButtonStyle(&ComboBoxStyle->ComboButtonStyle.ButtonStyle)
		.ContentPadding(ComboBoxStyle->ContentPadding)
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)
				// Invisible button under visible controls to handle clicking in blank space
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SButton)
					.OnClicked(this, &SNavigationBar::HandleBlankSpaceClicked)	
					.ButtonStyle( FAppStyle::Get(), "NoBorder" )
				]
				// Breadcrumb trail aligned to left in combo box
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(BreadcrumbBar, SBreadcrumbTrail<FString>)
					.Visibility(this, &SNavigationBar::GetNonEditVisibility)
					.TextStyle(InArgs._BreadcrumbTextStyle)
					.ButtonStyle(InArgs._BreadcrumbButtonStyle)
					.ButtonContentPadding(InArgs._BreadcrumbButtonContentPadding)
					.DelimiterImage(InArgs._BreadcrumbDelimiterImage)
					.OnCrumbClicked(InArgs._OnPathClicked)
					.HasCrumbMenuContent(InArgs._HasPathMenuContent)
					.GetCrumbMenuContent(InArgs._GetPathMenuContent)
				]
				// Editable text box taking up all space
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SAssignNew( EditableText, SEditableText )
					.Visibility(this, &SNavigationBar::GetEditTextVisibility)
					.OnTextCommitted(this, &SNavigationBar::HandleTextCommitted)
					.OnTextChanged(this, &SNavigationBar::HandleTextChanged)
					.SelectAllTextWhenFocused(false)
					.ClearKeyboardFocusOnCommit(true)
					.OnKeyDownHandler(this, &SNavigationBar::HandleEditableTextKeyDown)
				]
			]
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(StyleSet.GetMargin("Menu.Heading.Padding"))
			.AutoHeight()
			[
				// This matches multibox heading control, it could be refactored to use that (or a shared control)
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(this, &SNavigationBar::GetPopupHeading)
					.TextStyle(&StyleSet, "Menu.Heading")
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(14.f, 0.f, 0.f, 0.f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
					.Thickness(1.0f)
					.SeparatorImage(StyleSet.GetBrush("Menu.Separator"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(300.0f)
			.Padding(2.0f,2.0f)
			[
				SAssignNew(ComboListView, SLocationListView)
				.OnGenerateRow(this, &SNavigationBar::HandleGenerateComboRow)
				.OnSelectionChanged(this, &SNavigationBar::HandleComboSelectionChanged)
				.OnKeyDownHandler(this, &SNavigationBar::HandleComboKeyDown)
				.OnKeyCharHandler(this, &SNavigationBar::HandleComboKeyChar)
			]
		]
	);

	ComboListView->SetBackgroundBrush(FStyleDefaults::GetNoBrush());
	EditableText->SetTextBlockStyle(&TextBoxStyle->TextStyle);
	SetMenuContentWidgetToFocus(ComboListView);
}

void SNavigationBar::ClearPaths()
{
	BreadcrumbBar->ClearCrumbs();
}

void SNavigationBar::PushPath(const FText& ElementText, const FString& FullPath)
{
	BreadcrumbBar->PushCrumb(ElementText, FullPath);	
}

bool SNavigationBar::SupportsKeyboardFocus() const 
{
	return bIsFocusable;
}

void SNavigationBar::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) 
{
	// Stop editing and hide dropdown when focusing away from either the navigation bar or the popup
	TSharedPtr<SWindow> Popup = PopupWindowPtr.Pin();
	if(PreviousFocusPath.ContainsWidget(this) || PreviousFocusPath.ContainsWidget(Popup.Get()))
	{
		if (!NewWidgetPath.ContainsWidget(this) && !NewWidgetPath.ContainsWidget(Popup.Get()))
		{
			if (CompletionTimerHandle.IsValid())
			{
				UnRegisterActiveTimer(CompletionTimerHandle.ToSharedRef());	
				CompletionTimerHandle.Reset();
			}

			EditTextVisibility = EVisibility::Hidden;
			SetIsOpen(false, false);
		}
	}
}

FReply SNavigationBar::OnButtonClicked()
{
	GenerateHistoryOptions();
	return SComboButton::OnButtonClicked();
}

void SNavigationBar::GenerateHistoryOptions()
{
	PopupHeading = LOCTEXT("NavigationBar.HistoryHeader", "HISTORY");
	TArray<FString> Options;
	if (OnGetComboOptions.IsBound())
	{
		Options = OnGetComboOptions.Execute();
	}
	ComboListView->SetItems(MoveTemp(Options), ELocationSource::History);
}

void SNavigationBar::GenerateCompletionOptions(const FString& Prefix)
{
	PopupHeading = LOCTEXT("NavigationBar.SuggestionsHeader", "SUGGESTIONS"); 
	TArray<FString> Options;
	if (OnCompletePrefix.IsBound())
	{
		Options = OnCompletePrefix.Execute(Prefix);
	}
	ComboListView->SetItems(MoveTemp(Options), ELocationSource::Suggestion);
}

FReply SNavigationBar::HandleBlankSpaceClicked()
{
	StartEditingPath();
	return FReply::Handled();
}

FReply SNavigationBar::HandleComboKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharEvent)
{
	// Add the typed character to the currently suggested suggestion and return focus to the editable text box
	TArray<TSharedPtr<FLocationItem>> SelectedItems = ComboListView->GetSelectedItems();
	if (SelectedItems.Num() != 0)
	{
		FString NewText = SelectedItems[0]->VirtualPath;
		NewText += InCharEvent.GetCharacter();
		EditableText->SetText(FText::FromString(NewText));

		return FReply::Handled().SetUserFocus(EditableText.ToSharedRef());
	}
	return FReply::Unhandled();	
}

FReply SNavigationBar::HandleComboKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		// Select (and navigate to) the first selected item on hitting enter
		TArray<TSharedPtr<FLocationItem>> SelectedItems = ComboListView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			HandleComboSelectionChanged(SelectedItems[0], ESelectInfo::OnKeyPress);

			// Set focus back to navigation box 
			FSlateApplication::Get().ForEachUser([this](FSlateUser& User) 
			{
				TSharedRef<SWidget> ThisRef = this->AsShared();
				if (User.IsWidgetInFocusPath(this->ComboListView))
				{
					User.SetFocus(ThisRef);
				}
			});

			return FReply::Handled();
		}
	}
	else if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Close text box and popup when pressing escape
		EditTextVisibility = EVisibility::Hidden;
		SetIsOpen(false, false);
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		// Put current selected item into editable text and move focus back there
		TArray<TSharedPtr<FLocationItem>> SelectedItems = ComboListView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			FString NewText = SelectedItems[0]->VirtualPath + TEXT("/"); // TODO: Ideally only add slash if this prefix has children itself 
			EditableText->SetText(FText::FromString(NewText));
			return FReply::Handled().SetUserFocus(EditableText.ToSharedRef(), EFocusCause::SetDirectly);
		}
		return FReply::Handled(); // Swallow event to avoid confusion with items being selected or not 
	}

	return FReply::Unhandled();
}

FReply SNavigationBar::HandleEditableTextKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (EditableText->HasKeyboardFocus() && KeyEvent.GetKey() == EKeys::Escape)
	{
		// Stop editing and discard when user presses escape 
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Cleared);
	}

	if (EditableText->HasKeyboardFocus() && KeyEvent.GetKey() == EKeys::Tab)
	{
		// Swallow tab key to prevent moving focus away because Tab is used by the popup if it's focused
		return FReply::Handled();
	}

	bool bUp = KeyEvent.GetKey() == EKeys::Up;
	bool bDown = KeyEvent.GetKey() == EKeys::Down;
	
	if (bUp || bDown)
	{
		if (!IsOpen())
		{
			// Open popup if completions haven't been generated from timer after text edit yet
			GenerateCompletionOptions(EditableText->GetText().ToString());
			SetIsOpen(true, false);
			return FReply::Handled();
		}
		else 
		{
			// Switch focus from text box to first or last element of popup to start navigation
			if (bUp)
			{
				return ComboListView->NavigateToLast();
			}
			else 
			{
				return ComboListView->NavigateToFirst();
			}
		}
	}

	return FReply::Unhandled();
}

void SNavigationBar::StartEditingPath()
{
	EditTextVisibility = EVisibility::Visible;
	FText Text = FText::GetEmpty();
	if (BreadcrumbBar->HasCrumbs())
	{
		FString Path = BreadcrumbBar->PeekCrumb();
		if (OnCanEditPathAsText.IsBound() && OnCanEditPathAsText.Execute(Path))
		{
			Text = FText::FromString(Path);
		}
	}
	EditableText->SetText(Text);
	FSlateApplication::Get().SetKeyboardFocus(EditableText.ToSharedRef(), EFocusCause::SetDirectly);
}

void SNavigationBar::HandleTextChanged(const FText& NewText)
{
	if (EditableText->GetVisibility().IsVisible() && !bNoSuggestionsFromTextChange)
	{
		if (CompletionTimerHandle.IsValid())
		{
			UnRegisterActiveTimer(CompletionTimerHandle.ToSharedRef());	
		}
		// Generate completion suggestions shortly after user stops typing
		CompletionTimerHandle = RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SNavigationBar::HandleUpdateCompletionOptions));
	}
}

	
void SNavigationBar::HandleTextCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	switch (CommitType)
	{
		case ETextCommit::Default:
		case ETextCommit::OnCleared:
		case ETextCommit::OnUserMovedFocus:
			return; // Discard changes and don't navigate
	}

	// Stop editing and navigate to new path
	EditTextVisibility = EVisibility::Hidden;
	SetIsOpen(false, false);
	OnNavigateToPath.ExecuteIfBound(InText.ToString());
}

EActiveTimerReturnType SNavigationBar::HandleUpdateCompletionOptions(double InCurrentTime, float InDeltaTime)
{
	GenerateCompletionOptions(EditableText->GetText().ToString());
	SetIsOpen(true, false);
	CompletionTimerHandle = nullptr;
	return EActiveTimerReturnType::Stop; // Never run more than once unless text changes again 
}

TSharedRef<ITableRow> SNavigationBar::HandleGenerateComboRow(TSharedPtr<FLocationItem> ForItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FLocationItem>>, OwnerTable)
		.Padding(ComboBoxStyle->MenuRowPadding)
		.Style(TableRowStyle)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(GetImageForItem(*ForItem))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ForItem->VirtualPath))
			]
		];
}

void SNavigationBar::HandleComboSelectionChanged(TSharedPtr<FLocationItem> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (!SelectedItem.IsValid())
	{
		return;
	}

	switch(SelectInfo)
	{
		case ESelectInfo::Direct:
		case ESelectInfo::OnKeyPress:
		case ESelectInfo::OnMouseClick:
			// Stop editing/hide popup and navigate to chosen path
			EditTextVisibility = EVisibility::Hidden;
			SetIsOpen(false);
			OnNavigateToPath.ExecuteIfBound(SelectedItem->VirtualPath);
			break;
		case ESelectInfo::OnNavigation:
			// Mirror chosen item into text box without moving focus back or triggering new suggestions
			TGuardValue<bool> BlockSuggestions{bNoSuggestionsFromTextChange, true};
			EditableText->SetText(FText::FromString(SelectedItem->VirtualPath));
			break;
	}
}

FText SNavigationBar::GetPopupHeading() const 
{
	return PopupHeading;
}

const FSlateBrush* SNavigationBar::GetImageForItem(const FLocationItem& ForItem) const
{
	switch (ForItem.Source)
	{
		case ELocationSource::History:
			return HistoryIcon;
		case ELocationSource::Suggestion:
		default:
			return SuggestionIcon;
	}
}

EVisibility SNavigationBar::GetEditTextVisibility() const
{
	return EditTextVisibility;
}

EVisibility SNavigationBar::GetNonEditVisibility() const
{
	if (EditTextVisibility.IsVisible())
	{
		return EVisibility::Hidden;
	}
	else
	{
		return EVisibility::Visible;
	}
}

#undef LOCTEXT_NAMESPACE