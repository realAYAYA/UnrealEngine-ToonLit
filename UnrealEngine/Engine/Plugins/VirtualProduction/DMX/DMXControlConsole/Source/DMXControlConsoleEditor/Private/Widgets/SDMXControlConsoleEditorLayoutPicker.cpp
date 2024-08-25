// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayoutPicker.h"

#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorLayoutPicker"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorLayoutPicker::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct layout picker correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;

		ChildSlot
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(0.f, 8.f, 0.f, 4.f)
				.AutoHeight()
				[
					GenerateLayoutCheckBoxWidget()
				]

				+ SVerticalBox::Slot()
				.Padding(0.f, 4.f, 0.f, 8.f)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						GenerateDefaultLayoutPickerWidget()
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						GenerateUserLayoutPickerWidget()
					]
				]
			];

		UpdateComboBoxSource();

		// Sync to control console's active layout
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (ActiveLayout && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			UserLayoutsComboBox->SetSelectedItem(ActiveLayout);
			LastSelectedItem = ActiveLayout;
			LayoutNameText = FText::FromString(ActiveLayout->LayoutName);
		}
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateLayoutCheckBoxWidget()
	{
		const TSharedRef<SWidget> LayoutCheckBoxWidget =
			SNew(SHorizontalBox)

			// Default Layout Mode
			+ SHorizontalBox::Slot()
			.Padding(2.f, 2.f, 4.f, 2.f)
			.AutoWidth()
			[
				SNew(SHorizontalBox)

				// Checkbox section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "RadioButton")
					.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnDefaultLayoutCheckBoxStateChanged)
					.IsChecked_Lambda([this]() { return IsDefaultLayoutActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				]

				// Text label section
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("DefaultLayoutModeLabel", "Auto"))
				]
			]

			// User Layout Mode
			+ SHorizontalBox::Slot()
			.Padding(4.f, 2.f)
			.AutoWidth()
			[
				SNew(SHorizontalBox)

				// Checkbox section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "RadioButton")
					.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnUserLayoutCheckBoxStateChanged)
					.IsChecked_Lambda([this]() { return IsDefaultLayoutActive() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
				]

				// Text label section
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("UserLayoutModeLabel", "Custom"))
				]
			];

		return LayoutCheckBoxWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateDefaultLayoutPickerWidget()
	{
		const TSharedRef<SWidget> DefaultLayoutPickerWidget =
			SNew(SHorizontalBox)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorLayoutPicker::GetDefaultLayoutVisibility))

			// Auto-Group Checkbox section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SDMXControlConsoleEditorLayoutPicker::IsAutoGroupCheckBoxChecked)
				.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnAutoGroupCheckBoxStateChanged)
			]

			// Auto-Group Label section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("DefaultLayoutAutoGroupCheckBoxLabel", "Auto-Group Patches"))
			];

		return DefaultLayoutPickerWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateUserLayoutPickerWidget()
	{
		const TSharedRef<SWidget> UserLayoutPickerWidget =
			SNew(SHorizontalBox)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorLayoutPicker::GetUserLayoutVisibility))

			// Layouts ComboBox section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SAssignNew(UserLayoutsComboBox, SComboBox<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>>)
				.OptionsSource(&ComboBoxSource)
				.OnGenerateWidget(this, &SDMXControlConsoleEditorLayoutPicker::GenerateUserLayoutComboBoxWidget)
				.OnComboBoxOpening(this, &SDMXControlConsoleEditorLayoutPicker::UpdateComboBoxSource)
				.OnSelectionChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnComboBoxSelectionChanged)
				.ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
				.ItemStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row")))
				.Content()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.MaxWidth(80.f)
					.Padding(4.f, 0.f)
					.AutoWidth()
					[
						SAssignNew(LayoutNameEditableBox, SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text_Lambda([this]() { return LayoutNameText; })
						.OverflowPolicy(ETextOverflowPolicy::Clip)
						.OnTextChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextChanged)
						.OnTextCommitted(this, &SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextCommitted)
					]
				]
			]

			// Add Layout button
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton")))
				.ToolTipText(LOCTEXT("LayoutPickerAddNewButton_ToolTip", "Add New"))
				.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnAddLayoutClicked)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Plus"))
					.ColorAndOpacity(FStyleColors::AccentGreen)
				]
			]

			// Rename Layout button
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton")))
				.ToolTipText(LOCTEXT("LayoutPickerRenameButton_ToolTip", "Rename"))
				.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnRenameLayoutClicked)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Edit"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			// Delete Layout button
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton")))
				.ToolTipText(LOCTEXT("LayoutPickerDeleteButton_ToolTip","Delete"))
				.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnDeleteLayoutClicked)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Delete"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			];

		return UserLayoutPickerWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateUserLayoutComboBoxWidget(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout)
	{
		if (InLayout.IsValid())
		{
			const TSharedRef<SWidget> ComboBoxWidget =
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.MaxWidth(80.f)
				.Padding(6.f, 0.f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text_Lambda([InLayout](){ return InLayout.IsValid() ? FText::FromString(InLayout->LayoutName) : FText::GetEmpty(); })
				];

			return ComboBoxWidget;
		}

		return SNullWidget::NullWidget;
	}

	bool SDMXControlConsoleEditorLayoutPicker::IsDefaultLayoutActive() const
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		return ActiveLayout && ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked();
	}

	void SDMXControlConsoleEditorLayoutPicker::OnDefaultLayoutCheckBoxStateChanged(ECheckBoxState CheckBoxState)
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (ControlConsoleLayouts)
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->ClearSelection();

			UDMXControlConsoleEditorGlobalLayoutBase* DefaultLayout = &ControlConsoleLayouts->GetDefaultLayoutChecked();

			const FScopedTransaction SetActiveDefaultLayoutTransaction(LOCTEXT("SetActiveDefaultLayoutTransaction", "Change Layout"));
			ControlConsoleLayouts->Modify();
			ControlConsoleLayouts->SetActiveLayout(DefaultLayout);
		}
	}

	void SDMXControlConsoleEditorLayoutPicker::OnUserLayoutCheckBoxStateChanged(ECheckBoxState CheckBoxState)
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return;
		}

		const FScopedTransaction SetActiveUserLayoutTransaction(LOCTEXT("SetActiveUserLayoutTransaction", "Change Layout"));
		ControlConsoleLayouts->Modify();

		if (LastSelectedItem.IsValid())
		{
			ControlConsoleLayouts->SetActiveLayout(LastSelectedItem.Get());
			LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);
		}
		else
		{
			const TArray<UDMXControlConsoleEditorGlobalLayoutBase*>& UserLayouts = ControlConsoleLayouts->GetUserLayouts();
			UDMXControlConsoleEditorGlobalLayoutBase* NewSelectedLayout = nullptr;
			if (UserLayouts.IsEmpty())
			{
				NewSelectedLayout = ControlConsoleLayouts->AddUserLayout("");
			}
			else
			{
				NewSelectedLayout = UserLayouts.Last();
			}

			if (NewSelectedLayout)
			{
				ControlConsoleLayouts->SetActiveLayout(NewSelectedLayout);

				UpdateComboBoxSource();
				UserLayoutsComboBox->SetSelectedItem(NewSelectedLayout);
				LastSelectedItem = NewSelectedLayout;
				LayoutNameText = FText::FromString(NewSelectedLayout->LayoutName);
			}
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();
	}

	void SDMXControlConsoleEditorLayoutPicker::UpdateComboBoxSource()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleLayouts)
		{
			return;
		}
		
		const TArray<UDMXControlConsoleEditorGlobalLayoutBase*>& UserLayouts = ControlConsoleLayouts->GetUserLayouts();
		ComboBoxSource.Reset(UserLayouts.Num());
		ComboBoxSource.Append(UserLayouts);

		if (UserLayoutsComboBox.IsValid())
		{
			UserLayoutsComboBox->RefreshOptions();
		}
	}

	ECheckBoxState SDMXControlConsoleEditorLayoutPicker::IsAutoGroupCheckBoxChecked() const
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		const bool bAutoGroupActivePatches = ControlConsoleEditorData && ControlConsoleEditorData->GetAutoGroupActivePatches();
		return bAutoGroupActivePatches ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void SDMXControlConsoleEditorLayoutPicker::OnAutoGroupCheckBoxStateChanged(ECheckBoxState CheckBoxState)
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (ControlConsoleEditorData)
		{
			ControlConsoleEditorData->Modify();
			ControlConsoleEditorData->ToggleAutoGroupActivePatches();
		}
	}

	void SDMXControlConsoleEditorLayoutPicker::OnComboBoxSelectionChanged(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout, ESelectInfo::Type SelectInfo)
	{
		if (SelectInfo == ESelectInfo::Direct)
		{
			return;
		}

		if (!InLayout.IsValid() || !EditorModel.IsValid() || !UserLayoutsComboBox.IsValid())
		{
			return;
		}

		UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();

		const FScopedTransaction UserLayoutSelectionChangedTransaction(LOCTEXT("UserLayoutSelectionChangedTransaction", "Change Layout"));
		ControlConsoleLayouts->Modify();
		ControlConsoleLayouts->SetActiveLayout(InLayout.Get());

		UserLayoutsComboBox->SetSelectedItem(InLayout);
		LastSelectedItem = InLayout;
		LayoutNameText = FText::FromString(InLayout->LayoutName);
	}

	void SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextChanged(const FText& NewText)
	{
		LayoutNameText = NewText;
	}

	void SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter && !LayoutNameText.IsEmpty())
		{
			const FString& NewLayoutName = LayoutNameText.ToString();
			OnRenameLayout(NewLayoutName);
		}
	}

	void SDMXControlConsoleEditorLayoutPicker::OnRenameLayout(const FString& NewName)
	{
		if (LastSelectedItem.IsValid())
		{
			const FScopedTransaction LayoutNameEditedTransaction(LOCTEXT("LayoutNameEditedTransaction", "Edit Layout Name"));
			LastSelectedItem->PreEditChange(UDMXControlConsoleEditorGlobalLayoutBase::StaticClass()->FindPropertyByName(UDMXControlConsoleEditorGlobalLayoutBase::GetLayoutNamePropertyName()));
			LastSelectedItem->LayoutName = NewName;
			LastSelectedItem->PostEditChange();

			LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);
		}
	}

	FReply SDMXControlConsoleEditorLayoutPicker::OnAddLayoutClicked()
	{
		if (!EditorModel.IsValid())
		{
			return FReply::Handled();
		}

		UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (ControlConsoleLayouts)
		{
			const FString& NewLayoutName = LayoutNameText.ToString();

			const FScopedTransaction AddUserLayoutTransaction(LOCTEXT("AddUserLayoutTransaction", "Add New Layout"));
			ControlConsoleLayouts->PreEditChange(nullptr);
			UDMXControlConsoleEditorGlobalLayoutBase* NewUserLayout = ControlConsoleLayouts->AddUserLayout(NewLayoutName);
			ControlConsoleLayouts->SetActiveLayout(NewUserLayout);
			ControlConsoleLayouts->PostEditChange();

			UpdateComboBoxSource();
			UserLayoutsComboBox->SetSelectedItem(NewUserLayout);
			LastSelectedItem = NewUserLayout;
			LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);

			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->ClearSelection();
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorLayoutPicker::OnRenameLayoutClicked()
	{
		if (!LayoutNameText.IsEmpty())
		{
			const FString& NewLayoutName = LayoutNameText.ToString();
			OnRenameLayout(NewLayoutName);
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorLayoutPicker::OnDeleteLayoutClicked()
	{
		if (!EditorModel.IsValid())
		{
			return FReply::Handled();
		}

		UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return FReply::Handled();
		}

		const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
		if (LastSelectedItem.IsValid() && UserLayouts.Contains(LastSelectedItem))
		{
			const int32 LayoutIndex = UserLayouts.IndexOfByKey(LastSelectedItem);

			const FScopedTransaction DeleteUserLayoutTransaction(LOCTEXT("DeleteUserLayoutTransaction", "Delete Layout"));
			ControlConsoleLayouts->Modify();
			ControlConsoleLayouts->DeleteUserLayout(LastSelectedItem.Get());

			int32 LayoutIndexToSelect = INDEX_NONE;
			if (LayoutIndex > 0)
			{
				LayoutIndexToSelect = LayoutIndex - 1;
			}
			else if(LayoutIndex == 0 && UserLayouts.Num() > 1)
			{
				LayoutIndexToSelect = LayoutIndex + 1;
			}

			if(LayoutIndexToSelect != INDEX_NONE && UserLayouts.IsValidIndex(LayoutIndexToSelect))
			{
				// Select the previous user layout in the array
				UDMXControlConsoleEditorGlobalLayoutBase* LayoutToSelect = UserLayouts[LayoutIndexToSelect];
				ControlConsoleLayouts->SetActiveLayout(LayoutToSelect);

				UpdateComboBoxSource();
				UserLayoutsComboBox->SetSelectedItem(LayoutToSelect);
				LastSelectedItem = LayoutToSelect;
				LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);
			}
			else
			{
				// If there are no more user layouts switch to default layout
				UDMXControlConsoleEditorGlobalLayoutBase* DefaultLayout = &ControlConsoleLayouts->GetDefaultLayoutChecked();
				ControlConsoleLayouts->SetActiveLayout(DefaultLayout);

				UserLayoutsComboBox->ClearSelection();
				LastSelectedItem = nullptr;
				LayoutNameText = FText::GetEmpty();
			}

			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->ClearSelection();
		}

		return FReply::Handled();
	}

	EVisibility SDMXControlConsoleEditorLayoutPicker::GetDefaultLayoutVisibility() const
	{
		return IsDefaultLayoutActive() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorLayoutPicker::GetUserLayoutVisibility() const
	{
		
		return !IsDefaultLayoutActive() ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
