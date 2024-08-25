// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurveStyle.h"
#include "EaseCurveTool/AvaEaseCurveSubsystem.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePreview.h"
#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "Framework/SlateDelegates.h"
#include "Internationalization/Text.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SAvaEaseCurvePreset"

void SAvaEaseCurvePreset::Construct(const FArguments& InArgs)
{
	DisplayRate = InArgs._DisplayRate;
	OnPresetChanged = InArgs._OnPresetChanged;
	OnQuickPresetChanged = InArgs._OnQuickPresetChanged;
	OnGetNewPresetTangents = InArgs._OnGetNewPresetTangents;

	ChildSlot
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]()
				{
					return bIsCreatingNewPreset ? 1 : 0;
				})
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Expose(ComboBoxSlot)
				[
					ConstructPresetComboBox()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton"))
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("AddNewPresetToolTip", "Save the current ease curve as a new preset"))
					.IsEnabled_Lambda([this]()
						{
							return !SelectedItem.IsValid();
						})
					.OnClicked(this, &SAvaEaseCurvePreset::OnCreateNewPresetClick)
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(FAvaEaseCurveStyle::Get().GetFloat(TEXT("ToolButton.ImageSize"))))
						.Image(FAppStyle::GetBrush(TEXT("Icons.Plus")))
					]
				]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SAssignNew(NewPresetNameTextBox, SEditableTextBox)
					.OnKeyDownHandler(this, &SAvaEaseCurvePreset::OnNewPresetKeyDownHandler)
					.OnTextCommitted(this, &SAvaEaseCurvePreset::OnNewPresetTextCommitted)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton"))
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("CancelNewPresetToolTip", "Cancels the current new ease curve preset operation"))
					.IsEnabled_Lambda([this]()
						{
							return !SelectedItem.IsValid();
						})
					.OnClicked(this, &SAvaEaseCurvePreset::OnCancelNewPresetClick)
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(FAvaEaseCurveStyle::Get().GetFloat(TEXT("ToolButton.ImageSize"))))
						.Image(FAppStyle::GetBrush(TEXT("Icons.X")))
					]
				]
			]
		];
}

TSharedRef<SWidget> SAvaEaseCurvePreset::ConstructPresetComboBox()
{
	return SNew(SComboButton)
		.OnGetMenuContent(this, &SAvaEaseCurvePreset::GeneratePresetDropdown)
		.OnMenuOpenChanged_Lambda([this](const bool bInOpening)
			{
				bIsInEditMode.Set(false);
			})
		.ButtonContent()
		[
			GenerateSelectedRowWidget()
		];
}

TSharedRef<SWidget> SAvaEaseCurvePreset::GeneratePresetDropdown()
{
	static const FVector2D ButtonImageSize = FVector2D(FAvaEaseCurveStyle::Get().GetFloat(TEXT("ToolButton.ImageSize")));

	TSharedRef<SWidget> OutWidget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHintLabel", "Search"))
				.OnTextChanged(this, &SAvaEaseCurvePreset::OnSearchTextChanged)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton"))
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("ReloadJsonPresetsToolTip", "Reload ease curve presets from Json files"))
				.OnClicked(this, &SAvaEaseCurvePreset::ReloadJsonPresets)
				[
					SNew(SImage)
					.DesiredSizeOverride(ButtonImageSize)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Refresh")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton"))
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("ExploreJsonPresetsFolderToolTip", "Opens the folder location for the Json ease curve presets"))
				.OnClicked(this, &SAvaEaseCurvePreset::ExploreJsonPresetsFolder)
				[
					SNew(SImage)
					.DesiredSizeOverride(ButtonImageSize)
					.Image(FAppStyle::GetBrush(TEXT("Icons.FolderOpen")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.f, 0.f, 3.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAvaEaseCurveStyle::Get(), TEXT("ToolToggleButton"))
				.Padding(4.f)
				.ToolTipText(LOCTEXT("ToggleEditModeToolTip", "Enable editing of ease curve presets and categories"))
				.IsChecked_Lambda([this]()
					{
						return bIsInEditMode.Get(false) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
				.OnCheckStateChanged(this, &SAvaEaseCurvePreset::ToggleEditMode)
				[
					SNew(SImage)
					.DesiredSizeOverride(ButtonImageSize)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Edit")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAvaEaseCurveStyle::Get(), TEXT("ToolButton"))
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("CreateCategoryToolTip", "Creates a new empty category"))
				.Visibility_Lambda([this]()
					{
						return bIsInEditMode.Get(false) ? EVisibility::Visible : EVisibility::Collapsed;
					})
				.OnClicked(this, &SAvaEaseCurvePreset::CreateNewCategory)
				[
					SNew(SImage)
					.DesiredSizeOverride(ButtonImageSize)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Plus")))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(3.f, 0.f, 3.f, 3.f)
		[
			SNew(SBox)
			.MaxDesiredHeight(960.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(GroupWidgetsParent, SBox)
				]
			]
		];

	RegenerateGroupWrapBox();

	return OutWidget;
}

void SAvaEaseCurvePreset::UpdateGroupsContent()
{
	const UAvaEaseCurveSubsystem& EaseCurveSubsystem = UAvaEaseCurveSubsystem::Get();
	const TArray<FString>& EaseCurveCategories = EaseCurveSubsystem.GetEaseCurveCategories();

	auto GenerateNoPresetsWidget = [](const FText& InText)
		{
			return SNew(SBox)
				.WidthOverride(300.f)
				.HeightOverride(200.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), TEXT("HintText"))
					.Text(InText)
				];
		};

	if (EaseCurveCategories.Num() == 0)
	{
		GroupWidgetsParent->SetContent(GenerateNoPresetsWidget(LOCTEXT("NoPresetsLabel", "No ease curve presets")));
		return;
	}

	if (!SearchText.IsEmpty())
	{
		int32 TotalVisiblePresets = 0;
		for (const TSharedPtr<SAvaEaseCurvePresetGroup>& GroupWidget : GroupWidgets)
		{
			TotalVisiblePresets += GroupWidget->GetVisiblePresetCount();
		}
		if (TotalVisiblePresets == 0)
		{
			GroupWidgetsParent->SetContent(GenerateNoPresetsWidget(LOCTEXT("NoPresetsFoundLabel", "No ease curve presets found")));
			return;
		}
	}

	GroupWidgetsParent->SetContent(GroupWrapBox.ToSharedRef());
}

void SAvaEaseCurvePreset::RegenerateGroupWrapBox()
{
	const UAvaEaseCurveSubsystem& EaseCurveSubsystem = UAvaEaseCurveSubsystem::Get();
	const TArray<FString>& EaseCurveCategories = EaseCurveSubsystem.GetEaseCurveCategories();
	const int32 CurvePresetCount = EaseCurveCategories.Num();

	GroupWidgets.Empty(CurvePresetCount);

	GroupWrapBox = SNew(SUniformWrapPanel)
		.HAlign(HAlign_Center)
		.SlotPadding(FMargin(2.f, 1.f))
		.EvenRowDistribution(true)
		.NumColumnsOverride_Lambda([]()
			{
				const int32 EaseCurveCategoryCount = UAvaEaseCurveSubsystem::Get().GetEaseCurveCategories().Num();
				return FMath::Min(5, EaseCurveCategoryCount);
			});

	for (const FString& Category : EaseCurveCategories)
	{
		TSharedRef<SAvaEaseCurvePresetGroup> NewGroupWidget = SNew(SAvaEaseCurvePresetGroup)
			.CategoryName(Category)
			.Presets(EaseCurveSubsystem.GetEaseCurvePresets(Category))
			.SelectedPreset(SelectedItem)
			.IsEditMode(bIsInEditMode)
			.DisplayRate(DisplayRate.Get())
			.OnCategoryDelete(this, &SAvaEaseCurvePreset::HandleCategoryDelete)
			.OnCategoryRename(this, &SAvaEaseCurvePreset::HandleCategoryRename)
			.OnPresetDelete(this, &SAvaEaseCurvePreset::HandlePresetDelete)
			.OnPresetRename(this, &SAvaEaseCurvePreset::HandlePresetRename)
			.OnBeginPresetMove(this, &SAvaEaseCurvePreset::HandleBeginPresetMove)
			.OnEndPresetMove(this, &SAvaEaseCurvePreset::HandleEndPresetMove)
			.OnPresetClick(this, &SAvaEaseCurvePreset::HandlePresetClick);

		GroupWidgets.Add(NewGroupWidget);

		GroupWrapBox->AddSlot()
			.HAlign(HAlign_Left)
			[
				NewGroupWidget
			];
	}

	UpdateGroupsContent();
}

TSharedRef<SWidget> SAvaEaseCurvePreset::GenerateSelectedRowWidget() const
{
	if (!SelectedItem.IsValid())
	{
		return SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::FromString("Select Preset..."));
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0.f, 2.f, 5.f, 2.f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FStyleColors::White25)
			[
				SNew(SAvaEaseCurvePreview)
				.PreviewSize(12.f)
				.CustomToolTip(true)
				.DisplayRate(DisplayRate.Get())
				.Tangents_Lambda([this]()
					{
						return SelectedItem.IsValid() ? SelectedItem->Tangents : FAvaEaseCurveTangents();
					})
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FStyleColors::Foreground)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.Text_Lambda([this]()
					{
						return SelectedItem.IsValid() ? FText::FromString(SelectedItem->Name) : FText();
					})
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FStyleColors::White25)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.Text_Lambda([this]()
					{
						return SelectedItem.IsValid() ? FText::FromString(SelectedItem->Category) : FText();
					})
			]

		];
}

FReply SAvaEaseCurvePreset::OnCreateNewPresetClick()
{
	bIsCreatingNewPreset = true;

	FSlateApplication::Get().SetAllUserFocus(NewPresetNameTextBox);

	return FReply::Handled();
}

FReply SAvaEaseCurvePreset::OnCancelNewPresetClick()
{
	bIsCreatingNewPreset = false;
	NewPresetNameTextBox->SetText(FText());

	return FReply::Handled();
}

FReply SAvaEaseCurvePreset::OnDeletePresetClick()
{
	if (SelectedItem.IsValid())
	{
		if (UAvaEaseCurveSubsystem* EaseCurveSubsystem = GEditor->GetEditorSubsystem<UAvaEaseCurveSubsystem>())
		{
			EaseCurveSubsystem->RemovePreset(*SelectedItem);
		}

		ClearSelection();
	}

	return FReply::Handled();
}

FReply SAvaEaseCurvePreset::OnNewPresetKeyDownHandler(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		bIsCreatingNewPreset = false;
		NewPresetNameTextBox->SetText(FText());

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAvaEaseCurvePreset::OnNewPresetTextCommitted(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter && !InNewText.IsEmpty())
	{
		FAvaEaseCurveTangents NewTangents;
		if (OnGetNewPresetTangents.IsBound() && OnGetNewPresetTangents.Execute(NewTangents))
		{
			UAvaEaseCurveSubsystem& EaseCurveSubsystem = UAvaEaseCurveSubsystem::Get();

			SelectedItem = EaseCurveSubsystem.AddPreset(InNewText.ToString(), NewTangents);

			ComboBoxSlot->AttachWidget(ConstructPresetComboBox());
		}
	}

	bIsCreatingNewPreset = false;
	NewPresetNameTextBox->SetText(FText());
}

void SAvaEaseCurvePreset::OnSearchTextChanged(const FText& InSearchText)
{
	SearchText = InSearchText;

	for (const TSharedPtr<SAvaEaseCurvePresetGroup>& GroupWidget : GroupWidgets)
	{
		GroupWidget->SetSearchText(SearchText);
	}

	UpdateGroupsContent();
}

void SAvaEaseCurvePreset::ClearSelection()
{
	SelectedItem.Reset();
	ComboBoxSlot->AttachWidget(ConstructPresetComboBox());
}

bool SAvaEaseCurvePreset::GetSelectedItem(FAvaEaseCurvePreset& OutPreset) const
{
	if (!SelectedItem.IsValid())
	{
		return false;
	}

	OutPreset = *SelectedItem;

	return true;
}

bool SAvaEaseCurvePreset::SetSelectedItem(const FString& InName)
{
	UAvaEaseCurveSubsystem& EaseCurveSubsystem = UAvaEaseCurveSubsystem::Get();

	const TSharedPtr<FAvaEaseCurvePreset>& FoundItem = EaseCurveSubsystem.FindPreset(InName);
	if (!FoundItem.IsValid())
	{
		return false;
	}

	SelectedItem = FoundItem;
	ComboBoxSlot->AttachWidget(ConstructPresetComboBox());

	return true;
}

bool SAvaEaseCurvePreset::SetSelectedItem(const FAvaEaseCurvePreset& InPreset)
{
	return SetSelectedItem(InPreset.Name);
}

bool SAvaEaseCurvePreset::SetSelectedItem(const FAvaEaseCurveTangents& InTangents)
{
	UAvaEaseCurveSubsystem& EaseCurveSubsystem = UAvaEaseCurveSubsystem::Get();

	const TSharedPtr<FAvaEaseCurvePreset>& FoundItem = EaseCurveSubsystem.FindPresetByTangents(InTangents);
	if (!FoundItem.IsValid())
	{
		return false;
	}

	SelectedItem = FoundItem;
	ComboBoxSlot->AttachWidget(ConstructPresetComboBox());

	return true;
}

FReply SAvaEaseCurvePreset::ReloadJsonPresets()
{
	UAvaEaseCurveSubsystem::Get().ReloadPresetsFromJson();

	RegenerateGroupWrapBox();

	return FReply::Handled();
}

FReply SAvaEaseCurvePreset::ExploreJsonPresetsFolder()
{
	UAvaEaseCurveSubsystem::Get().ExploreJsonPresetsFolder();

	return FReply::Handled();
}

FReply SAvaEaseCurvePreset::CreateNewCategory()
{
	UAvaEaseCurveSubsystem::Get().AddNewPresetCategory();

	ReloadJsonPresets();

	return FReply::Handled();
}

void SAvaEaseCurvePreset::ToggleEditMode(const ECheckBoxState bInNewState)
{
	bIsInEditMode.Set(bInNewState == ECheckBoxState::Checked ? true : false);

	RegenerateGroupWrapBox();
}

bool SAvaEaseCurvePreset::HandleCategoryDelete(const FString& InCategoryName)
{
	if (!UAvaEaseCurveSubsystem::Get().RemovePresetCategory(InCategoryName))
	{
		return false;
	}

	ReloadJsonPresets();

	return true;
}

bool SAvaEaseCurvePreset::HandleCategoryRename(const FString& InCategoryName, const FString& InNewName)
{
	return UAvaEaseCurveSubsystem::Get().RenamePresetCategory(InCategoryName, InNewName);
}

bool SAvaEaseCurvePreset::HandlePresetDelete(const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
{
	return UAvaEaseCurveSubsystem::Get().RemovePreset(*InPreset);
}

bool SAvaEaseCurvePreset::HandlePresetRename(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewName)
{
	return UAvaEaseCurveSubsystem::Get().RenamePreset(InPreset->Category, InPreset->Name, InNewName);
}

bool SAvaEaseCurvePreset::HandleBeginPresetMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName)
{
	for (const TSharedPtr<SAvaEaseCurvePresetGroup>& GroupWidget : GroupWidgets)
	{
		if (!GroupWidget->GetCategoryName().Equals(InNewCategoryName))
		{
			GroupWidget->NotifyCanDrop(true);
		}
	}

	return true;
}

bool SAvaEaseCurvePreset::HandleEndPresetMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName)
{
	for (const TSharedPtr<SAvaEaseCurvePresetGroup>& GroupWidget : GroupWidgets)
	{
		if (!GroupWidget->GetCategoryName().Equals(InNewCategoryName))
		{
			GroupWidget->NotifyCanDrop(false);
		}
	}

	if (!InPreset.IsValid() || InPreset->Category.Equals(InNewCategoryName))
	{
		return false;
	}

	if (!UAvaEaseCurveSubsystem::Get().ChangePresetCategory(InPreset, InNewCategoryName))
	{
		return false;
	}

	ReloadJsonPresets();

	return true;
}

bool SAvaEaseCurvePreset::HandlePresetClick(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FModifierKeysState& InModifierKeys)
{
	if (InModifierKeys.IsShiftDown())
	{
		UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetMutableDefault<UAvaEaseCurveToolSettings>();
		EaseCurveToolSettings->SetQuickEaseTangents(InPreset->Tangents.ToJson());
		EaseCurveToolSettings->SaveConfig();

		OnQuickPresetChanged.ExecuteIfBound(InPreset);

		return true;
	}

	SetSelectedItem(*InPreset);

	OnPresetChanged.ExecuteIfBound(InPreset);

	return true;
}

#undef LOCTEXT_NAMESPACE
