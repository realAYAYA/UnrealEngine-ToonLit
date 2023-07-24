// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroup.h"

#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"

#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroup"

void SDMXControlConsoleEditorFaderGroup::Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView)
{
	FaderGroupView = InFaderGroupView;

	if (!ensureMsgf(FaderGroupView.IsValid(), TEXT("Invalid fader group view, cannot create fader group widget correctly.")))
	{
		return;
	}

	OnAddFaderGroup = InArgs._OnAddFaderGroup;
	OnAddFaderGroupRow = InArgs._OnAddFaderGroupRow;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(120.f)
		.HeightOverride(300.f)
		[
			SNew(SBorder)
			.BorderImage(this, &SDMXControlConsoleEditorFaderGroup::GetFaderGroupBorderImage)
			[
				SNew(SVerticalBox)

				// Top interface
				+ SVerticalBox::Slot()
				.FillHeight(.1f)
				.Padding(2.f, 2.f, 2.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(.8f)
					[
						SAssignNew(FaderGroupNameTextBox, SEditableTextBox)
						.Text(this, &SDMXControlConsoleEditorFaderGroup::OnGetFaderGroupNameText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &SDMXControlConsoleEditorFaderGroup::OnFaderGroupNameCommitted)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(.2f)
					[
						SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
					]
				]

				// Add slot button
				+ SVerticalBox::Slot()
				.FillHeight(.6f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(.8f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(12.f, 0.f, 0.f, 0.f)
					[
						SNew(SBox)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(25.f)
						.HeightOverride(25.f)
						.Padding(5.f)
						[
							SNew(SDMXControlConsoleEditorAddButton)
							.OnClicked(OnAddFaderGroup)
						]
					]
				]

				// Add row button
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(25.f)
					.HeightOverride(25.f)
					.Padding(5.f)
					[
						SNew(SDMXControlConsoleEditorAddButton)
						.OnClicked(OnAddFaderGroupRow)
						.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroup::GetAddRowButtonVisibility))
					]
				]
			]
		]
	];

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	SelectionHandler->GetOnSelectionChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroup::OnSelectionChanged);
}

FReply SDMXControlConsoleEditorFaderGroup::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsSelected() && InKeyEvent.GetKey() == EKeys::Delete)
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();

		const FScopedTransaction DeleteFaderGroupTransaction(LOCTEXT("DeleteFaderGroupTransaction", "Delete Fader Group"));

		if (SelectedFaderGroupsObjects.Num() > 1)
		{
			for (const TWeakObjectPtr<UObject>& SelectedFaderGroupObject : SelectedFaderGroupsObjects)
			{
				UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject);
				if (!SelectedFaderGroup)
				{
					continue;
				}

				SelectedFaderGroup->PreEditChange(nullptr);

				SelectedFaderGroup->Destroy();

				SelectedFaderGroup->PostEditChange();

				SelectionHandler->RemoveFromSelection(SelectedFaderGroup);
			}
		}
		else
		{
			UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupsObjects[0]);
			if (SelectedFaderGroup)
			{
				SelectedFaderGroup->PreEditChange(nullptr);

				SelectionHandler->ReplaceInSelection(SelectedFaderGroup);
				SelectedFaderGroup->Destroy();

				SelectedFaderGroup->PostEditChange();
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXControlConsoleEditorFaderGroup::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (FaderGroup)
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();

			if (MouseEvent.IsLeftShiftDown())
			{
				SelectionHandler->Multiselect(FaderGroup);
			}
			else if (MouseEvent.IsControlDown())
			{
				if (IsSelected())
				{
					SelectionHandler->RemoveFromSelection(FaderGroup);
				}
				else
				{
					SelectionHandler->AddToSelection(FaderGroup);
				}
			}
			else
			{
				SelectionHandler->ClearSelection();
				SelectionHandler->AddToSelection(FaderGroup);
			}
		}
	}

	return FReply::Handled();
}

FReply SDMXControlConsoleEditorFaderGroup::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (ExpandArrowButton.IsValid())
		{
			ExpandArrowButton->ToggleExpandArrow();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

UDMXControlConsoleFaderGroup* SDMXControlConsoleEditorFaderGroup::GetFaderGroup() const
{
	return FaderGroupView.IsValid() ? FaderGroupView.Pin()->GetFaderGroup() : nullptr;
}

bool SDMXControlConsoleEditorFaderGroup::IsSelected() const
{
	UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (!FaderGroup)
	{
		return false;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	return SelectionHandler->IsSelected(FaderGroup);
}

void SDMXControlConsoleEditorFaderGroup::OnSelectionChanged()
{
	if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();

		// Remove from selection if not visible. Avoids selecting this when filtering
		const bool bMultiSelecting = SelectionHandler->GetSelectedFaders().Num() + SelectionHandler->GetSelectedFaderGroups().Num() > 1;
		if (bMultiSelecting && GetVisibility() == EVisibility::Collapsed)
		{
			SelectionHandler->RemoveFromSelection(FaderGroup);
			return;
		}

		// Set keyboard focus only if there are no more selected Faders in the Fader Group
		if (IsSelected() && SelectionHandler->GetSelectedFadersFromFaderGroup(FaderGroup).IsEmpty())
		{
			FSlateApplication::Get().SetKeyboardFocus(AsShared());
		}
	}
}

FText SDMXControlConsoleEditorFaderGroup::OnGetFaderGroupNameText() const
{
	return FaderGroupView.IsValid() ? FText::FromString(FaderGroupView.Pin()->GetFaderGroupName()) : FText::GetEmpty();
}

void SDMXControlConsoleEditorFaderGroup::OnFaderGroupNameCommitted(const FText& NewName, ETextCommit::Type InCommit)
{
	if (!ensureMsgf(FaderGroupView.IsValid(), TEXT("Invalid fader group view, cannot update fader group name correctly.")))
	{
		return;
	}

	if (!FaderGroupNameTextBox.IsValid())
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupView.Pin()->GetFaderGroup();

	const FScopedTransaction FaderGroupTransaction(LOCTEXT("FaderGroupTransaction", "Edit Fader Group Name"));
	FaderGroup->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetFaderGroupNamePropertyName()));
	FaderGroup->SetFaderGroupName(NewName.ToString());
	FaderGroup->PostEditChange();

	FaderGroupNameTextBox->SetText(FText::FromString(FaderGroup->GetFaderGroupName()));
}

EVisibility SDMXControlConsoleEditorFaderGroup::GetAddRowButtonVisibility() const
{
	if (!FaderGroupView.IsValid())
	{
		return EVisibility::Collapsed;
	}

	const int32 Index = FaderGroupView.Pin()->GetIndex();
	return Index == 0 ? EVisibility::Visible : EVisibility::Hidden;
}

FSlateColor SDMXControlConsoleEditorFaderGroup::GetFaderGroupBorderColor() const
{
	const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
	if (!FaderGroup)
	{
		return FLinearColor::White;
	}

	return FaderGroup->GetEditorColor();
}

const FSlateBrush* SDMXControlConsoleEditorFaderGroup::GetFaderGroupBorderImage() const
{
	if (IsHovered())
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Highlighted");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Hovered");
		}
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Selected");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.BlackBrush");
		}
	}
}

#undef LOCTEXT_NAMESPACE
