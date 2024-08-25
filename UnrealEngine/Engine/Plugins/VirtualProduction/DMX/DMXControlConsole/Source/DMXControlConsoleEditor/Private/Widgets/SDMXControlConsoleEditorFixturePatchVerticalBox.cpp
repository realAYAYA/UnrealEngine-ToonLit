// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFixturePatchVerticalBox.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleAddEmptyFaderGroupMenu.h"
#include "Widgets/SDMXControlConsoleAddFixturePatchMenu.h"
#include "Widgets/SDMXControlConsoleFixturePatchList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFixturePatchVerticalBox"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorFixturePatchVerticalBox::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct fixture patch vertical box widget correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;

		const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		UDMXLibrary* DMXLibrary = ControlConsoleData ? ControlConsoleData->GetDMXLibrary() : nullptr;

		ChildSlot
			.Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					GenerateFixturePatchListToolbar()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(FixturePatchList, SDMXControlConsoleFixturePatchList, EditorModel.Get())
					.DMXLibrary(DMXLibrary)
				]
			];
	}

	void SDMXControlConsoleEditorFixturePatchVerticalBox::ForceRefresh()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (ControlConsoleData && FixturePatchList.IsValid())
		{
			UDMXLibrary* NewLibrary = ControlConsoleData->GetDMXLibrary();
			FixturePatchList->SetDMXLibrary(NewLibrary);
		}
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFixturePatchListToolbar()
	{
		const auto GenerateAddButtonContentLambda = [](const FText& AddButtonText, const FText& AddButtonToolTip)
			{
				return
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
						.Text(AddButtonText)
						.ToolTipText(AddButtonToolTip)
						.TextStyle(FAppStyle::Get(), "SmallButtonText")
					];
			};

		const TSharedRef<SWidget> FixturePatchListToolbar =
			SNew(SHorizontalBox)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility))

			// Add All Patches Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(160.f)
			.HAlign(HAlign_Left)
			.Padding(8.f, 0.f, 4.f, 8.f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.IsEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddPatchesButtonEnabled)
				.OnClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked)
				[
					GenerateAddButtonContentLambda
					(
						LOCTEXT("AddAllFixturePatchesFromList", "Add All Patches"),
						LOCTEXT("AddAllFixturePatchesFromList_ToolTip", "Add all Fixture Patches from the list.")
					)
				]
			]

			// Add Patch Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(160.f)
			.HAlign(HAlign_Left)
			.Padding(4.f, 0.f, 8.f, 8.f)
			[
				SNew(SComboButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HasDownArrow(true)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.IsEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddPatchesButtonEnabled)
				.OnGetMenuContent(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu)
				.ButtonContent()
				[
					GenerateAddButtonContentLambda
					(
						LOCTEXT("AddFixturePatchFromList", "Add Patch"),
						LOCTEXT("AddFixturePatchFromList_ToolTip", "Add a Fixture Patch from the list.")
					)
				]
			]

			// Add Empty Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(160.f)
			.HAlign(HAlign_Left)
			.Padding(4.f, 0.f, 8.f, 8.f)
			[
				SNew(SComboButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HasDownArrow(true)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.IsEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddEmptyButtonEnabled)
				.OnGetMenuContent(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddEmptyMenu)
				.ButtonContent()
				[
					GenerateAddButtonContentLambda
					(
						LOCTEXT("AddEmptyFaderGroup", "Add Empty"),
						LOCTEXT("AddEmptyFaderGroup_ToolTip", "Add an Empty fader group to the Control Console.")
					)
				]
			];

		return FixturePatchListToolbar;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu()
	{
		if (!EditorModel.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		// Show Add Patch buttons only if the current layout is the user layout
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return SNullWidget::NullWidget;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (ActiveLayout && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatches;
			TArray<UDMXEntityFixturePatch*> SelectedFixturePatches = FixturePatchList->GetSelectedFixturePatches();
			Algo::Transform(SelectedFixturePatches, WeakFixturePatches, [](UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch;
				});

			return SNew(SDMXControlConsoleAddFixturePatchMenu, WeakFixturePatches, EditorModel.Get());
		}

		return SNullWidget::NullWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddEmptyMenu()
	{
		if (!EditorModel.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		// Show Add Empty buttons only if the current layout is the user layout
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return SNullWidget::NullWidget;
		}

		return SNew(SDMXControlConsoleAddEmptyFaderGroupMenu, EditorModel.Get());
	}

	FReply SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return FReply::Handled();
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return FReply::Handled();
		}

		const FScopedTransaction AddAllPatchesTransaction(LOCTEXT("AddAllPatchesTransaction", "Add All Patches"));
		ActiveLayout->PreEditChange(nullptr);

		const TArray<UDMXControlConsoleFaderGroupRow*>& FaderGroupRows = ControlConsoleData->GetFaderGroupRows();
		for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
		{
			if (!FaderGroupRow)
			{
				continue;
			}

			const TArray<UDMXControlConsoleFaderGroup*>& FaderGroups = FaderGroupRow->GetFaderGroups();
			if (FaderGroups.IsEmpty())
			{
				continue;
			}

			UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->AddNewRowToLayout();
			if (!LayoutRow)
			{
				continue;
			}

			// Remove Fader Groups already in the layout and all unpatched Fader Groups
			for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
			{
				if (!FaderGroup || ActiveLayout->ContainsFaderGroup(FaderGroup))
				{
					continue;
				}

				LayoutRow->Modify();
				UDMXControlConsoleFaderGroupController* NewController = LayoutRow->CreateFaderGroupController(FaderGroup, FaderGroup->GetFaderGroupName());
				if (NewController)
				{
					NewController->Modify();
					NewController->SetIsActive(true);
					ActiveLayout->AddToActiveFaderGroupControllers(NewController);
				}
			}
		}

		ActiveLayout->PostEditChange();

		return FReply::Handled();
	}

	bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddPatchesButtonEnabled() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		return ControlConsoleData && ControlConsoleData->GetDMXLibrary() && ControlConsoleData->FilterString.IsEmpty();
	}

	bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddEmptyButtonEnabled() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		return ControlConsoleData && ControlConsoleData->FilterString.IsEmpty();
	}

	EVisibility SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility() const
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (ControlConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
			const bool bIsVisible = IsValid(ActiveLayout) && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked();
			return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
		}

		return EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
