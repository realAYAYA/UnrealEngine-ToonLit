// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupToolbar.h"

#include "Algo/AnyOf.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXEditorStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityReference.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/Filter/FilterModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/StyleColors.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroup"

void SDMXControlConsoleEditorFaderGroupToolbar::Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView)
{
	FaderGroupView = InFaderGroupView;

	if (!ensureMsgf(FaderGroupView.IsValid(), TEXT("Invalid fader group view, cannot create fader group widget correctly.")))
	{
		return;
	}

	OnAddFaderGroupDelegate = InArgs._OnAddFaderGroup;
	OnAddFaderGroupRowDelegate = InArgs._OnAddFaderGroupRow;

	UpdateComboBoxSource();

	ChildSlot
	[
		SNew(SHorizontalBox)
		// Fader Group tag section
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(6.f, 0.f, 4.f, 0.f)
		.AutoWidth()
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.MinDesiredWidth(8.f)
			.MinDesiredHeight(24.f)
			[
				SNew(SImage)
				.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
				.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupEditorColor)
			]
		]

		// Fixture Patch ComboBox section
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.MaxWidth(120.f)
		.Padding(4.f, 8.f)
		.AutoWidth()
		[
			SAssignNew(FixturePatchesComboBox, SComboBox<TSharedPtr<FDMXEntityFixturePatchRef>>)
			.OptionsSource(&ComboBoxSource)
			.OnGenerateWidget(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateFixturePatchesComboBoxWidget)
			.OnComboBoxOpening(this, &SDMXControlConsoleEditorFaderGroupToolbar::UpdateComboBoxSource)
			.OnSelectionChanged(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnComboBoxSelectionChanged)
			.ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
			.ItemStyle(&FDMXControlConsoleEditorStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("DMXControlConsole.FaderGroupToolbar")))
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4.f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FDMXEditorStyle::Get().GetBrush("Icons.FixturePatch"))
					.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupEditorColor)
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.MaxWidth(56.f)
				.Padding(4.f, 0.f)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(56.f)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupFixturePatchNameText)
						.OverflowPolicy(ETextOverflowPolicy::Clip)
					]
				]
			]
		]

		// Expand Arrow button section
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.MaxWidth(20.f)
		.AutoWidth()
		[
			SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
			.OnExpandClicked(InArgs._OnExpanded)
			.ToolTipText(LOCTEXT("FaderGroupExpandArrowButton_Tooltip", "Switch between Collapsed/Expanded view mode"))
		]

		// Info Combo button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(4.f, 8.f, 0.f, 8.f)
		[
			SNew(SComboButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
			.ContentPadding(2.f)
			.ForegroundColor(FSlateColor::UseStyle())
			.HasDownArrow(false)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateFaderGroupInfoMenuWidget)
			.ToolTipText(LOCTEXT("InfoButtonToolTipText", "Info"))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
			.ButtonContent()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::Get().GetBrush("Icons.Info"))
			]
		]

		//Searchbox section
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.MaxWidth(250.f)
		.Padding(4.f, 8.f)
		[
			SAssignNew(ToolbarSearchBox, SSearchBox)
			.MinDesiredWidth(100.f)
			.OnTextChanged(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnSearchTextChanged)
			.ToolTipText(LOCTEXT("SearchBarTooltip", "Searches for Fader Name, Attributes, Fixture ID, Universe or Patch. Examples:\n\n* FaderName\n* Dimmer\n* Pan, Tilt\n* 1\n* 1.\n* 1.1\n* Universe 1\n* Uni 1-3\n* Uni 1, 3\n* Uni 1, 4-5'."))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
		]

		// Add New button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(4.f, 8.f, 0.f, 8.f)
		[
			SNew(SComboButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HasDownArrow(true)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateAddNewFaderGroupMenuWidget)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(FStyleColors::AccentGreen)
					.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("AddFaderGroupComboButton", "Add New"))
					.ToolTipText(LOCTEXT("AddFaderGroupComboButton_ToolTip", "Add a new Fader Group to the Control Console."))
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
				]
			]
		]

		//Settings section
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(4.f, 8.f)
		[
			SNew(SComboButton)
			.ContentPadding(0.f)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateSettingsMenuWidget)
			.HasDownArrow(true)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
			.ButtonContent()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
			]
		]
	];

	RestoreFaderGroupFilter();
}

TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateSettingsMenuWidget()
{
	constexpr bool bShouldCloseWindowAfterClosing = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

	MenuBuilder.BeginSection("Options", LOCTEXT("FaderGroupViewOptionsCategory", "Options"));
	{
		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Info")),
			FText::FromString(TEXT("Info")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnGetInfoPanel)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Select All")),
			FText::FromString(TEXT("Select All")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnSelectAllFaders)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Duplicate")),
			FText::FromString(TEXT("Duplicate")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnDuplicateFaderGroup)
				, FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanDuplicateFaderGroup)
				, FIsActionChecked()
				, FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanDuplicateFaderGroup)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Remove")),
			FText::FromString(TEXT("Remove")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnRemoveFaderGroup),
				FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanRemoveFaderGroup)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Controls", LOCTEXT("FaderGroupViewControlsCategory", "Controls"));
	{
		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Reset To Default")),
			FText::FromString(TEXT("Reset To Default")),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.ResetToDefault"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnResetFaderGroup)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("LockLabel", "Lock"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnLockFaderGroup, true),
				FCanExecuteAction::CreateLambda([this]() { return GetFaderGroup() && !GetFaderGroup()->IsLocked(); }),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([this]() { return GetFaderGroup() && !GetFaderGroup()->IsLocked(); })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("UnlockLabel", "Unlock"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnLockFaderGroup, false),
				FCanExecuteAction::CreateLambda([this]() { return GetFaderGroup() && GetFaderGroup()->IsLocked(); }),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([this]() { return GetFaderGroup() && GetFaderGroup()->IsLocked(); })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

UDMXControlConsoleFaderGroup* SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroup() const
{
	return FaderGroupView.IsValid() ? FaderGroupView.Pin()->GetFaderGroup() : nullptr;
}

TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateFixturePatchesComboBoxWidget(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef)
{
	if (FixturePatchRef.IsValid())
	{
		const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef->GetFixturePatch();
		const FLinearColor EditorTagColor = IsValid(FixturePatch) ? FixturePatch->EditorColor : FLinearColor::White;
		const FString FixturePatchName = IsValid(FixturePatch) ? FixturePatch->Name : TEXT("Undefined");

		const TSharedRef<SWidget> ComboBoxWidget =
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.MinDesiredWidth(6.f)
				.MinDesiredHeight(14.f)
				[
					SNew(SImage)
					.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
					.ColorAndOpacity(EditorTagColor)
				]
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.MaxWidth(140.f)
			.Padding(6.f, 0.f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(FText::FromString(FixturePatchName))
				.ToolTipText(FText::FromString(FixturePatchName))
			];

		if (FixturePatch)
		{
			ComboBoxWidget->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::IsFixturePatchStillAvailable, FixturePatch));
		}

		return ComboBoxWidget;
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateFaderGroupInfoMenuWidget()
{
	if (FaderGroupView.IsValid())
	{
		constexpr bool bShouldCloseWindowAfterClosing = false;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("Info", LOCTEXT("FaderGroupInfoMenuCategory", "Info"));
		{
			const TSharedRef<SWidget> FaderGroupInfoPanel =
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(8.f)
				.MaxWidth(116.f)
				[
					SNew(SDMXControlConsoleEditorFaderGroupPanel, FaderGroupView.Pin())
				];

			MenuBuilder.AddWidget(FaderGroupInfoPanel, FText::GetEmpty());
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateAddNewFaderGroupMenuWidget()
{
	constexpr bool bShouldCloseWindowAfterClosing = false;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

	MenuBuilder.BeginSection("Options", LOCTEXT("AddNewFaderGroupMenuCategory", "New Fader Group"));
	{
		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Next"))
			, FText::FromString(TEXT("Add new Fader Group next"))
			, FSlateIcon()
			, FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroup),
				FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroup)
			)
			, NAME_None
			, EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			FText::FromString(TEXT("Next Row"))
			, FText::FromString(TEXT("Add new Fader Group to next row"))
			, FSlateIcon()
			, FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroupRow),
				FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroupRow)
			)
			, NAME_None
			, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SDMXControlConsoleEditorFaderGroupToolbar::RestoreFaderGroupFilter()
{
	if (const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		const FString& FilterString = FaderGroup->FilterString;
		const FText FilterText = FText::FromString(FilterString);
		ToolbarSearchBox->SetText(FilterText);
	}
}

bool SDMXControlConsoleEditorFaderGroupToolbar::IsFixturePatchStillAvailable(const UDMXEntityFixturePatch* InFixturePatch) const
{
	if (InFixturePatch)
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		if (const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = CurrentLayout->GetAllFaderGroups();

			auto IsFixturePatchInUseLambda = [InFixturePatch](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
				{
					if (!FaderGroup.IsValid() || !FaderGroup->IsActive() || !FaderGroup->HasFixturePatch())
					{
						return false;
					}

					const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
					if (FixturePatch != InFixturePatch)
					{
						return false;
					}

					return true;
				};

			return !Algo::AnyOf(AllFaderGroups, IsFixturePatchInUseLambda);
		}
	}

	return false;
}

void SDMXControlConsoleEditorFaderGroupToolbar::UpdateComboBoxSource()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	DMXLibrary = EditorConsoleData->GetDMXLibrary();

	ComboBoxSource.Reset(ComboBoxSource.Num());
	ComboBoxSource.Add(MakeShared<FDMXEntityFixturePatchRef>());

	if (DMXLibrary.IsValid())
	{
		const TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
		for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesInLibrary)
		{
			if (FixturePatch && IsFixturePatchStillAvailable(FixturePatch))
			{
				const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef = MakeShared<FDMXEntityFixturePatchRef>();
				FixturePatchRef->SetEntity(FixturePatch);
				ComboBoxSource.Add(FixturePatchRef);
			}
		}
	}

	if (FixturePatchesComboBox.IsValid())
	{
		FixturePatchesComboBox->RefreshOptions();
	}
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnComboBoxSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef, ESelectInfo::Type SelectInfo)
{
	UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (!FaderGroup)
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	SelectionHandler->ClearFadersSelection(FaderGroup);
		
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
	UDMXControlConsoleFaderGroup* FaderGroupToAdd = nullptr;

	const FScopedTransaction GenerateFaderGroupFromComboBoxSelectionTransaction(LOCTEXT("GenerateFaderGroupFromComboBoxSelectionTransaction", "Replace Fader Group"));
	if (FixturePatch)
	{
		// Find Fader Group to Add in Control Console Data
		FaderGroupToAdd = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	}
	else if (CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
	{
		// Fader Group to Add is a new Fader Group
		UDMXControlConsoleFaderGroupRow& OwnerRow = FaderGroup->GetOwnerFaderGroupRowChecked();
		OwnerRow.PreEditChange(nullptr);
		FaderGroupToAdd = OwnerRow.AddFaderGroup(FaderGroup->GetIndex());
		OwnerRow.PostEditChange();
	}

	if (FaderGroupToAdd)
	{
		int32 RowIndex = CurrentLayout->GetFaderGroupRowIndex(FaderGroup);
		int32 ColumnIndex = CurrentLayout->GetFaderGroupColumnIndex(FaderGroup);

		// Emplace Fader Group with FaderGroupToAdd
		CurrentLayout->PreEditChange(nullptr);
		if (CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass())
		{
			CurrentLayout->RemoveFromLayout(FaderGroupToAdd);
		}
		else if (CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
		{
			CurrentLayout->RemoveFromLayout(FaderGroup);
		}

		CurrentLayout->AddToLayout(FaderGroupToAdd, RowIndex, ColumnIndex);
		CurrentLayout->PostEditChange();

		FaderGroupToAdd->Modify();
		FaderGroupToAdd->SetIsActive(true);
		FaderGroupToAdd->SetIsExpanded(FaderGroup->IsExpanded());

		FaderGroup->Modify();
		FaderGroup->SetIsActive(false);

		if (SelectionHandler->IsSelected(FaderGroup))
		{
			constexpr bool bNotifySelectionChange = false;
			SelectionHandler->AddToSelection(FaderGroupToAdd, bNotifySelectionChange);
			SelectionHandler->RemoveFromSelection(FaderGroup);
		}

		if (!FaderGroup->HasFixturePatch())
		{
			FaderGroup->Destroy();
		}
	}

	EditorConsoleModel->RequestRefresh();
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnSearchTextChanged(const FText& SearchText)
{
	if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		using namespace UE::DMXControlConsoleEditor::FilterModel::Private;
		FFilterModel::Get().SetFaderGroupFilter(FaderGroup, SearchText.ToString());

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		if (!SearchText.IsEmpty() && EditorConsoleModel->GetAutoSelectFilteredElements())
		{
			TArray<UObject*> FadersToSelect;
			TArray<UObject*> FadersToUnselect;
			const TArray<UDMXControlConsoleFaderBase*> AllFaders = FaderGroup->GetAllFaders();
			for (UDMXControlConsoleFaderBase* Fader : AllFaders)
			{
				if (!Fader)
				{
					continue;
				}

				if (Fader->IsMatchingFilter())
				{
					FadersToSelect.Add(Fader);
				}
				else
				{
					FadersToUnselect.Add(Fader);
				}
			}

			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
			constexpr bool bNotifySelection = false;
			SelectionHandler->AddToSelection(FadersToSelect, bNotifySelection);
			SelectionHandler->RemoveFromSelection(FadersToUnselect);
		}
	}
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroup() const
{
	OnAddFaderGroupDelegate.ExecuteIfBound();
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroupRow() const
{
	OnAddFaderGroupRowDelegate.ExecuteIfBound();
}

bool SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroup() const
{
	return FaderGroupView.IsValid() ? FaderGroupView.Pin()->CanAddFaderGroup() : false;
}

bool SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroupRow() const
{
	return FaderGroupView.IsValid() ? FaderGroupView.Pin()->CanAddFaderGroupRow() : false;
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnGetInfoPanel()
{
	if (FaderGroupView.IsValid())
	{
		FWidgetPath WidgetPath;
		FSlateApplication::Get().GeneratePathToWidgetChecked(FaderGroupView.Pin().ToSharedRef(), WidgetPath);

		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateFaderGroupInfoMenuWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnSelectAllFaders() const
{
	if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		SelectionHandler->AddAllFadersFromFaderGroupToSelection(FaderGroup, true);
	}
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnDuplicateFaderGroup()  const
{
	if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		const FScopedTransaction DuplicateFaderGroupOptionTransaction(LOCTEXT("DuplicateFaderGroupOptionTransaction", "Fader Group duplicated"));
		FaderGroup->PreEditChange(nullptr);
		FaderGroup->Duplicate();
		FaderGroup->PostEditChange();
	}
}

bool SDMXControlConsoleEditorFaderGroupToolbar::CanDuplicateFaderGroup() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	return IsValid(FaderGroup) ? !FaderGroup->HasFixturePatch() : false;
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnRemoveFaderGroup() const
{
	UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (!FaderGroup)
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	const FScopedTransaction RemoveFaderGroupOptionTransaction(LOCTEXT("RemoveFaderGroupOptionTransaction", "Remove Fader Group"));
	CurrentLayout->PreEditChange(nullptr);
	CurrentLayout->RemoveFromLayout(FaderGroup);
	CurrentLayout->ClearEmptyLayoutRows();
	CurrentLayout->PostEditChange();

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	SelectionHandler->RemoveFromSelection(FaderGroup);

	if (!FaderGroup->HasFixturePatch())
	{
		FaderGroup->Destroy();
	}
}

bool SDMXControlConsoleEditorFaderGroupToolbar::CanRemoveFaderGroup() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	bool bCanRemove = IsValid(FaderGroup);

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bCanRemove &= IsValid(CurrentLayout) && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass();
	}

	return bCanRemove;
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnResetFaderGroup() const
{
	if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		const FScopedTransaction ResetFaderGroupOptionTransaction(LOCTEXT("ResetFaderGroupOptionTransaction", "Fader Group reset to default"));
		FaderGroup->PreEditChange(nullptr);
		FaderGroup->ResetToDefault();
		FaderGroup->PostEditChange();
	}
}

void SDMXControlConsoleEditorFaderGroupToolbar::OnLockFaderGroup(bool bLock) const
{
	if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		const FScopedTransaction LockFaderGroupOptionTransaction(LOCTEXT("LockFaderGroupOptionTransaction", "Edit Fader Group lock state"));

		const TArray<UDMXControlConsoleFaderBase*> AllFaders = FaderGroup->GetAllFaders();
		for (UDMXControlConsoleFaderBase* Fader : AllFaders)
		{
			if (!Fader)
			{
				continue;
			}

			Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsLockedPropertyName()));
			Fader->SetLock(bLock);
			Fader->PostEditChange();
		}
	}
}

FSlateColor SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupEditorColor() const
{
	if (const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		return FaderGroup->GetEditorColor();	
	}

	return FLinearColor::White;
}

FText SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupFixturePatchNameText() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (FaderGroup && FaderGroup->HasFixturePatch())
	{
		return FText::FromString(FaderGroup->GetFixturePatch()->Name);
	}

	return FText::FromString(TEXT("Undefined"));
}

EVisibility SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility() const
{
	const bool bIsVisible =
		FaderGroupView.IsValid() &&
		FaderGroupView.Pin()->GetViewMode() == EDMXControlConsoleEditorViewMode::Expanded;

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
