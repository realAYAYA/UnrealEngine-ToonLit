// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaEaseCurvePresetGroup.h"
#include "EaseCurveTool/AvaEaseCurveStyle.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurvePresetDragDropOp.h"
#include "Internationalization/Text.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "SAvaEaseCurvePresetGroup"

void SAvaEaseCurvePresetGroup::Construct(const FArguments& InArgs)
{
	CategoryName = InArgs._CategoryName;
	Presets = InArgs._Presets;
	SelectedPreset = InArgs._SelectedPreset;
	SearchText = InArgs._SearchText;
	bIsEditMode = InArgs._IsEditMode;
	DisplayRate = InArgs._DisplayRate;
	OnCategoryDelete = InArgs._OnCategoryDelete;
	OnCategoryRename = InArgs._OnCategoryRename;
	OnPresetDelete = InArgs._OnPresetDelete;
	OnPresetRename = InArgs._OnPresetRename;
	OnBeginPresetMove = InArgs._OnBeginPresetMove;
	OnEndPresetMove = InArgs._OnEndPresetMove;
	OnPresetClick = InArgs._OnPresetClick;

	ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(this, &SAvaEaseCurvePresetGroup::GetEditModeVisibility)
				.BorderImage(this, &SAvaEaseCurvePresetGroup::GetBorderImage)
			]
			+ SOverlay::Slot()
			.Padding(1.f)
			[
				SNew(SBox)
				.WidthOverride(140.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.f)
					[
						ConstructHeader()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SListView<TSharedPtr<FAvaEaseCurvePreset>>)
						.ListItemsSource(&VisiblePresets)
						.SelectionMode_Lambda([this]()
							{
								return IsEditMode() ? ESelectionMode::None : ESelectionMode::Single;
							})
						.ListViewStyle(&FAppStyle::Get().GetWidgetStyle<FTableViewStyle>(TEXT("PropertyTable.InViewport.ListView")))
						.OnGenerateRow(this, &SAvaEaseCurvePresetGroup::GeneratePresetWidget)
					]
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(1.f)
			[
				SNew(SButton)
				.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton.NoPad"))
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("EditModeDeleteTooltip", "Delete this category and the json file associated with it on disk"))
				.Visibility(this, &SAvaEaseCurvePresetGroup::GetEditModeVisibility)
				.OnClicked(this, &SAvaEaseCurvePresetGroup::HandleCategoryDelete)
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(10.f))
					.Image(FAppStyle::GetBrush(TEXT("Icons.Delete")))
				]
			]
		];
	
	SetSearchText(SearchText);
}

TSharedRef<SWidget> SAvaEaseCurvePresetGroup::ConstructHeader()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(3.f, 2.f)
		[
			SNew(SBox)
			.MaxDesiredWidth(180.f)
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]()
					{
						return IsEditMode() ? 1 : 0;
					})
				+ SWidgetSwitcher::Slot()
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), TEXT("Menu.Heading"))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(FText::FromString(CategoryName))
					.ToolTipText(this, &SAvaEaseCurvePresetGroup::GetPresetNameTooltipText)
				]
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(RenameCategoryNameTextBox, SEditableTextBox)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(FText::FromString(CategoryName))
					.ToolTipText(this, &SAvaEaseCurvePresetGroup::GetPresetNameTooltipText)
					.OnTextCommitted(this, &SAvaEaseCurvePresetGroup::HandleCategoryRenameCommitted)
				]
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(3.f, 0.f)
		[
			SNew(SSeparator)
			.SeparatorImage(FAppStyle::Get().GetBrush(TEXT("Menu.Separator")))
			.Thickness(1.1f)
		];
}

TSharedRef<ITableRow> SAvaEaseCurvePresetGroup::GeneratePresetWidget(const TSharedPtr<FAvaEaseCurvePreset> InPreset, const TSharedRef<STableViewBase>& InOwnerTable)
{
	TSharedRef<SAvaEaseCurvePresetGroupItem> NewPresetWidget = SNew(SAvaEaseCurvePresetGroupItem, InOwnerTable)
		.Preset(InPreset)
		.IsEditMode(bIsEditMode)
		.DisplayRate(DisplayRate)
		.IsSelected(this, &SAvaEaseCurvePresetGroup::IsSelected, InPreset)
		.OnClick(this, &SAvaEaseCurvePresetGroup::HandlePresetClick)
		.OnDelete(this, &SAvaEaseCurvePresetGroup::HandlePresetDelete)
		.OnRename(this, &SAvaEaseCurvePresetGroup::HandlePresetRename)
		.OnBeginMove(this, &SAvaEaseCurvePresetGroup::HandlePresetBeginMove)
		.OnEndMove(this, &SAvaEaseCurvePresetGroup::HandlePresetEndMove);
	PresetWidgetsMap.Add(InPreset, NewPresetWidget);

	return NewPresetWidget;
}

void SAvaEaseCurvePresetGroup::SetSearchText(const FText& InText)
{
	SearchText = InText;

	VisiblePresets.Empty();

	const FString SearchString = SearchText.ToString();

	for (TSharedPtr<FAvaEaseCurvePreset> Preset : Presets)
	{
		if (Preset.IsValid() && (Preset->Name.Contains(SearchString) || Preset->Category.Contains(SearchString)))
		{
			VisiblePresets.Add(Preset);
		}
	}

	const bool bIsVisible = SearchText.IsEmpty() || VisiblePresets.Num() > 0;
	SetVisibility(bIsVisible ? EVisibility::Visible : EVisibility::Collapsed);
}

int32 SAvaEaseCurvePresetGroup::GetVisiblePresetCount() const
{
	return VisiblePresets.Num();
}

bool SAvaEaseCurvePresetGroup::IsEditMode() const
{
	return bIsEditMode.Get(false);
}

EVisibility SAvaEaseCurvePresetGroup::GetEditModeVisibility() const
{
	return IsEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SAvaEaseCurvePresetGroup::GetBorderImage() const
{
	FName BrushName;

	if (bCanBeDroppedOn)
	{
		BrushName = bIsOverDifferentCategory ? TEXT("EditMode.Background.Over") : TEXT("EditMode.Background.Highlight");
	}
	else
	{
		BrushName = TEXT("EditMode.Background");
	}

	return FAvaEaseCurveStyle::Get().GetBrush(BrushName);
}

FText SAvaEaseCurvePresetGroup::GetPresetNameTooltipText() const
{
	return FText::Format(LOCTEXT("CategoryTooltipText", "Category: {0}"), FText::FromString(CategoryName));
}

void SAvaEaseCurvePresetGroup::HandleCategoryRenameCommitted(const FText& InNewText, ETextCommit::Type InCommitType)
{
	const FString NewCategoryName = InNewText.ToString();

	if (!NewCategoryName.IsEmpty() && !NewCategoryName.Equals(CategoryName)
		&& OnCategoryRename.IsBound() && OnCategoryRename.Execute(CategoryName, NewCategoryName))
	{
		CategoryName = NewCategoryName;
	}
	else
	{
		RenameCategoryNameTextBox->SetText(FText::FromString(CategoryName));
	}
}

FReply SAvaEaseCurvePresetGroup::HandleCategoryDelete() const
{
	if (OnCategoryDelete.IsBound())
	{
		OnCategoryDelete.Execute(CategoryName);
	}

	return FReply::Handled();
}

bool SAvaEaseCurvePresetGroup::HandlePresetDelete(const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
{
	if (OnPresetDelete.IsBound() && OnPresetDelete.Execute(InPreset))
	{
		Presets.Remove(InPreset);
		VisiblePresets.Remove(InPreset);

		return true;
	}

	return false;
}

bool SAvaEaseCurvePresetGroup::HandlePresetRename(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewName)
{
	if (OnPresetRename.IsBound() && OnPresetRename.Execute(InPreset, InNewName))
	{
		InPreset->Name = InNewName;

		return true;
	}
	else
	{
		PresetWidgetsMap[InPreset]->SetPreset(InPreset);

		return false;
	}
}

bool SAvaEaseCurvePresetGroup::HandlePresetBeginMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName) const
{
	if (OnBeginPresetMove.IsBound())
	{
		return OnBeginPresetMove.Execute(InPreset, InNewCategoryName);
	}

	return false;
}

bool SAvaEaseCurvePresetGroup::HandlePresetEndMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName) const
{
	if (OnEndPresetMove.IsBound() && OnEndPresetMove.Execute(InPreset, InNewCategoryName))
	{
		InPreset->Category = InNewCategoryName;

		return true;
	}

	return false;
}

bool SAvaEaseCurvePresetGroup::HandlePresetClick(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FModifierKeysState& InModifierKeys) const
{
	if (OnPresetClick.IsBound())
	{
		OnPresetClick.Execute(InPreset, InModifierKeys);

		return true;
	}

	return false;
}

void SAvaEaseCurvePresetGroup::OnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	SCompoundWidget::OnDragEnter(InGeometry, InDragDropEvent);

	const TSharedPtr<FAvaEaseCurvePresetDragDropOperation> Operation =
		InDragDropEvent.GetOperationAs<FAvaEaseCurvePresetDragDropOperation>();

	if (Operation.IsValid())
	{
		bIsOverDifferentCategory = !CategoryName.Equals(Operation->GetPreset()->Category);
		bCanBeDroppedOn = bIsOverDifferentCategory;

		Operation->AddHoveredGroup(SharedThis(this));
	}
}

void SAvaEaseCurvePresetGroup::OnDragLeave(const FDragDropEvent& InDragDropEvent)
{
	SCompoundWidget::OnDragLeave(InDragDropEvent);

	const TSharedPtr<FAvaEaseCurvePresetDragDropOperation> Operation =
		InDragDropEvent.GetOperationAs<FAvaEaseCurvePresetDragDropOperation>();

	if (Operation.IsValid())
	{
		bCanBeDroppedOn = !CategoryName.Equals(Operation->GetPreset()->Category);
		bIsOverDifferentCategory = false;
	}
}

FReply SAvaEaseCurvePresetGroup::OnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	SCompoundWidget::OnDragOver(InGeometry, InDragDropEvent);

	const TSharedPtr<FAvaEaseCurvePresetDragDropOperation> Operation =
		InDragDropEvent.GetOperationAs<FAvaEaseCurvePresetDragDropOperation>();

	if (Operation.IsValid() && Operation->GetPreset().IsValid())
	{
		bIsOverDifferentCategory = !CategoryName.Equals(Operation->GetPreset()->Category);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAvaEaseCurvePresetGroup::OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
{
	SCompoundWidget::OnDrop(InGeometry, InDragDropEvent);

	ResetDragBorder();

	const TSharedPtr<FAvaEaseCurvePresetDragDropOperation> Operation =
		InDragDropEvent.GetOperationAs<FAvaEaseCurvePresetDragDropOperation>();

	if (Operation.IsValid())
	{
		if (OnEndPresetMove.IsBound() && OnEndPresetMove.Execute(Operation->GetPreset(), CategoryName))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SAvaEaseCurvePresetGroup::ResetDragBorder()
{
	bCanBeDroppedOn = false;
	bIsOverDifferentCategory = false;
}

bool SAvaEaseCurvePresetGroup::IsSelected(const TSharedPtr<FAvaEaseCurvePreset> InPreset) const
{
	if (SelectedPreset.IsSet() && SelectedPreset.Get().IsValid())
	{
		return *InPreset == *SelectedPreset.Get();
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
