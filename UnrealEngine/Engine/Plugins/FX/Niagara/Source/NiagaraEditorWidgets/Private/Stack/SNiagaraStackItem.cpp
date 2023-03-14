// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackItem.h"

#include "EditorFontGlyphs.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "Styling/AppStyle.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "ViewModels/Stack/NiagaraStackClipboardUtilities.h"
#include "Styling/StyleColors.h"
#include "Stack/SNiagaraStackInheritanceIcon.h"

#define LOCTEXT_NAMESPACE "NiagaraStackItem"

void SNiagaraStackItem::Construct(const FArguments& InArgs, UNiagaraStackItem& InItem, UNiagaraStackViewModel* InStackViewModel)
{
	Item = &InItem;
	StackEntryItem = Item;
	StackViewModel = InStackViewModel;

	TSharedRef<SHorizontalBox> RowBox = SNew(SHorizontalBox);

	// Icon Brush
	if (Item->GetSupportedIconMode() == UNiagaraStackEntry::EIconMode::Brush)
	{
		RowBox->AddSlot()
		.Padding(2, 0, 3, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image_UObject(Item, &UNiagaraStackItem::GetIconBrush)
		];
	}

	// Icon Text
	if (Item->GetSupportedIconMode() == UNiagaraStackEntry::EIconMode::Text)
	{
		RowBox->AddSlot()
		.Padding(2, 0, 3, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
			.Text(Item->GetIconText())
		];
	}

	// Display name
	RowBox->AddSlot()
		.Padding(2, 0, 2, 0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(DisplayNameWidget, SNiagaraStackDisplayName, InItem, *InStackViewModel)
			.NameStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ItemText")
			.EditableNameStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.EditableItemText")
		];

	// Allow derived classes to add additional widgets.
	AddCustomRowWidgets(RowBox);

	// Edit Mode Button
	if (Item->SupportsEditMode())
	{
		RowBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 2, 0)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(this, &SNiagaraStackItem::GetEditModeButtonToolTipText)
			.ButtonStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.SimpleButton")
			.OnClicked(this, &SNiagaraStackItem::EditModeButtonClicked)
			.ContentPadding(1)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Edit"))
				.ColorAndOpacity(this, &SNiagaraStackItem::GetEditModeButtonIconColor)
			]
		];
	}

	// Reset to base button
	if (Item->SupportsResetToBase())
	{
		RowBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 2, 0)
		[
			SNew(SButton)
			.Visibility(this, &SNiagaraStackItem::GetResetToBaseButtonVisibility)
			.IsFocusable(false)
			.ToolTipText(this, &SNiagaraStackItem::GetResetToBaseButtonToolTipText)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.OnClicked(this, &SNiagaraStackItem::ResetToBaseButtonClicked)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				.ColorAndOpacity(FSlateColor(FLinearColor::Green))
			]
		];
	}

	// Inheritance Icon
	if (Item->SupportsInheritance())
	{
		RowBox->AddSlot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0, 0, 2, 0)
		[
			SNew(SNiagaraStackInheritanceIcon, Item)
		];
	}

	// Delete button
	if (Item->SupportsDelete())
	{
		RowBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 2, 0)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.IsFocusable(false)
			.ToolTipText(this, &SNiagaraStackItem::GetDeleteButtonToolTipText)
			.Visibility(this, &SNiagaraStackItem::GetDeleteButtonVisibility)
			.OnClicked(this, &SNiagaraStackItem::DeleteClicked)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	// Enabled checkbox
	if (Item->SupportsChangeEnabled())
	{
		RowBox->AddSlot()
		.Padding(0)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.IsChecked(this, &SNiagaraStackItem::CheckEnabledStatus)
			.OnCheckStateChanged(this, &SNiagaraStackItem::OnCheckStateChanged)
			.IsEnabled(this, &SNiagaraStackItem::GetEnabledCheckBoxEnabled)
		];
	}

	ChildSlot
	[
		// Allow derived classes add a container for the row widgets, e.g. a drop target.
		AddContainerForRowWidgets(RowBox)
	];
}

void SNiagaraStackItem::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (Item->GetIsRenamePending())
	{
		if (DisplayNameWidget.IsValid())
		{
			DisplayNameWidget->StartRename();
		}
		Item->SetIsRenamePending(false);
	}
	SNiagaraStackEntryWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

TSharedRef<SWidget> SNiagaraStackItem::AddContainerForRowWidgets(TSharedRef<SWidget> RowWidgets)
{
	return RowWidgets;
}

FText SNiagaraStackItem::GetEditModeButtonToolTipText() const
{
	return Item->GetEditModeIsActive() 
		? LOCTEXT("DisableEditModeToolTip", "Disable Edit Mode")
		: LOCTEXT("EnableEditModeToolTip", "Enable Edit Mode");
}

FReply SNiagaraStackItem::EditModeButtonClicked()
{
	if (Item->SupportsEditMode())
	{
		Item->SetEditModeIsActive(!Item->GetEditModeIsActive());
	}
	return FReply::Handled();
}

FSlateColor SNiagaraStackItem::GetEditModeButtonIconColor() const
{
	return Item->GetEditModeIsActive()
		? FStyleColors::AccentYellow
		: FSlateColor::UseSubduedForeground();
}

EVisibility SNiagaraStackItem::GetResetToBaseButtonVisibility() const
{
	FText Unused;
	return Item->TestCanResetToBaseWithMessage(Unused) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNiagaraStackItem::GetResetToBaseButtonToolTipText() const
{
	FText CanResetToBaseMessage;
	Item->TestCanResetToBaseWithMessage(CanResetToBaseMessage);
	return CanResetToBaseMessage;
}

FReply SNiagaraStackItem::ResetToBaseButtonClicked()
{
	Item->ResetToBase();
	return FReply::Handled();
}

FText SNiagaraStackItem::GetDeleteButtonToolTipText() const
{
	FText CanDeleteMessage;
	Item->TestCanDeleteWithMessage(CanDeleteMessage);
	return CanDeleteMessage;
}

EVisibility SNiagaraStackItem::GetDeleteButtonVisibility() const
{
	FText Unused;
	return Item->TestCanDeleteWithMessage(Unused) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SNiagaraStackItem::DeleteClicked()
{
	TArray<UNiagaraStackEntry*> EntriesToDelete;
	EntriesToDelete.Add(Item);
	FNiagaraStackClipboardUtilities::DeleteSelection(EntriesToDelete);
	return FReply::Handled();
}

void SNiagaraStackItem::OnCheckStateChanged(ECheckBoxState InCheckState)
{
	Item->SetIsEnabled(InCheckState == ECheckBoxState::Checked);
}

ECheckBoxState SNiagaraStackItem::CheckEnabledStatus() const
{
	return Item->GetIsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SNiagaraStackItem::GetEnabledCheckBoxEnabled() const
{
	return Item->GetOwnerIsEnabled();
}

#undef LOCTEXT_NAMESPACE