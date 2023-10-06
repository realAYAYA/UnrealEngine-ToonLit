// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayoutPicker.h"

#include "DMXControlConsoleEditorSelection.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorLayoutPicker"

void SDMXControlConsoleEditorLayoutPicker::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.Padding(0.f, 8.f, 0.f, 4.f)
			.AutoHeight()
			[
				GenerateLayoutCheckBoxWidget()
			]

			+SVerticalBox::Slot()
			.Padding(0.f, 4.f, 0.f, 8.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorLayoutPicker::GetComboBoxVisibility))

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SAssignNew(UserLayoutsComboBox, SComboBox<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutUser>>)
					.OptionsSource(&ComboBoxSource)
					.OnGenerateWidget(this, &SDMXControlConsoleEditorLayoutPicker::GenerateLayoutComboBoxWidget)
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
							.Text_Lambda([this](){ return LayoutNameText; })
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
					.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnAddLayoutClicked)
					.Content()
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
					.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnRenameLayoutClicked)
					.Content()
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
					.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnDeleteLayoutClicked)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		];

	UpdateComboBoxSource();

	// Sync to Control Console's active layout
	UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetEditorConsoleLayouts();
	if (EditorConsoleLayouts)
	{
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (UDMXControlConsoleEditorGlobalLayoutUser* UserLayout = Cast<UDMXControlConsoleEditorGlobalLayoutUser>(ActiveLayout))
		{
			UserLayoutsComboBox->SetSelectedItem(UserLayout);
			LastSelectedItem = UserLayout;
			LayoutNameText = FText::FromString(UserLayout->GetLayoutName());
		}
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
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnDefaultLayoutChecked)
				.IsChecked(this, &SDMXControlConsoleEditorLayoutPicker::IsActiveLayoutClass, UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass())

			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("DefaultLayoutModeLabel", "Default"))
			]
		]

		// User Layout Mode
		+ SHorizontalBox::Slot()
		.Padding(4.f, 2.f)
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnUserLayoutChecked)
				.IsChecked(this, &SDMXControlConsoleEditorLayoutPicker::IsActiveLayoutClass, UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("UserLayoutModeLabel", "User"))
			]
		];

	return LayoutCheckBoxWidget;
}

TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateLayoutComboBoxWidget(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutUser> InLayout)
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
				.Text_Lambda([InLayout](){ return InLayout.IsValid() ? FText::FromString(InLayout->GetLayoutName()) : FText::GetEmpty(); })
			];

		return ComboBoxWidget;
	}

	return SNullWidget::NullWidget;
}

void SDMXControlConsoleEditorLayoutPicker::OnDefaultLayoutChecked(ECheckBoxState CheckBoxState)
{
	UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	if (UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetEditorConsoleLayouts())
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();

		UDMXControlConsoleEditorGlobalLayoutDefault* DefaultLayout = EditorConsoleLayouts->GetDefaultLayout();

		const FScopedTransaction SetActiveDefaultLayoutTransaction(LOCTEXT("SetActiveDefaultLayoutTransaction", "Change Layout"));
		EditorConsoleLayouts->Modify();
		EditorConsoleLayouts->SetActiveLayout(DefaultLayout);
	}
}

void SDMXControlConsoleEditorLayoutPicker::OnUserLayoutChecked(ECheckBoxState CheckBoxState)
{
	UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return;
	}

	const FScopedTransaction AddFirstUserLayoutTransaction(LOCTEXT("AddFirstUserLayoutTransaction", "Add New Layout"));
	EditorConsoleLayouts->Modify();

	if (LastSelectedItem.IsValid())
	{
		EditorConsoleLayouts->SetActiveLayout(LastSelectedItem.Get());
		LayoutNameText = FText::FromString(LastSelectedItem->GetLayoutName());
	}
	else
	{
		const TArray<UDMXControlConsoleEditorGlobalLayoutUser*> UserLayouts = EditorConsoleLayouts->GetUserLayouts();
		UDMXControlConsoleEditorGlobalLayoutUser* NewSelectedLayout = nullptr;
		if (UserLayouts.IsEmpty())
		{
			NewSelectedLayout = EditorConsoleLayouts->AddUserLayout("");
		}
		else
		{
			NewSelectedLayout = UserLayouts.Last();
		}

		if (NewSelectedLayout)
		{
			EditorConsoleLayouts->SetActiveLayout(NewSelectedLayout);

			UpdateComboBoxSource();
			UserLayoutsComboBox->SetSelectedItem(NewSelectedLayout);
			LastSelectedItem = NewSelectedLayout;
			LayoutNameText = FText::FromString(NewSelectedLayout->GetLayoutName());
		}
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	SelectionHandler->ClearSelection();
}

ECheckBoxState SDMXControlConsoleEditorLayoutPicker::IsActiveLayoutClass(UClass* InLayoutClass) const
{
	const UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetEditorConsoleLayouts();
	if (EditorConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		return ActiveLayout && ActiveLayout->GetClass() == InLayoutClass ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void SDMXControlConsoleEditorLayoutPicker::UpdateComboBoxSource()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
	{
		const TArray<UDMXControlConsoleEditorGlobalLayoutUser*> UserLayouts = EditorConsoleLayouts->GetUserLayouts();
		ComboBoxSource.Reset(UserLayouts.Num());
		ComboBoxSource.Append(UserLayouts);

		if (UserLayoutsComboBox.IsValid())
		{
			UserLayoutsComboBox->RefreshOptions();
		}
	}
}

void SDMXControlConsoleEditorLayoutPicker::OnComboBoxSelectionChanged(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutUser> InLayout, ESelectInfo::Type SelectInfo)
{
	if (!InLayout.IsValid() || !UserLayoutsComboBox.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	if (UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();

		if (!GIsTransacting)
		{
			const FScopedTransaction UserLayoutSelectionChangedTransaction(LOCTEXT("UserLayoutSelectionChangedTransaction", "Change Layout"));
		}
		EditorConsoleLayouts->Modify();
		EditorConsoleLayouts->SetActiveLayout(InLayout.Get());

		UserLayoutsComboBox->SetSelectedItem(InLayout);
		LastSelectedItem = InLayout;
		LayoutNameText = FText::FromString(InLayout->GetLayoutName());
	}
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
		LastSelectedItem->PreEditChange(UDMXControlConsoleEditorGlobalLayoutUser::StaticClass()->FindPropertyByName(UDMXControlConsoleEditorGlobalLayoutUser::GetLayoutNamePropertyName()));
		LastSelectedItem->SetLayoutName(NewName);
		LastSelectedItem->PostEditChange();

		LayoutNameText = FText::FromString(LastSelectedItem->GetLayoutName());
	}
}

FReply SDMXControlConsoleEditorLayoutPicker::OnAddLayoutClicked()
{
	UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	if (UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetEditorConsoleLayouts())
	{
		const FString& NewLayoutName = LayoutNameText.ToString();

		const FScopedTransaction AddUserLayoutTransaction(LOCTEXT("AddUserLayoutTransaction", "Add New Layout"));
		EditorConsoleLayouts->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutUser* NewUserLayout = EditorConsoleLayouts->AddUserLayout(NewLayoutName);
		EditorConsoleLayouts->SetActiveLayout(NewUserLayout);
		EditorConsoleLayouts->PostEditChange();

		UpdateComboBoxSource();
		UserLayoutsComboBox->SetSelectedItem(NewUserLayout);
		LastSelectedItem = NewUserLayout;

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
	UDMXControlConsoleEditorModel* EditorModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return FReply::Handled();
	}

	const TArray<UDMXControlConsoleEditorGlobalLayoutUser*> UserLayouts = EditorConsoleLayouts->GetUserLayouts();
	if (LastSelectedItem.IsValid() && UserLayouts.Contains(LastSelectedItem))
	{
		const int32 LayoutIndex = UserLayouts.IndexOfByKey(LastSelectedItem);

		const FScopedTransaction DeleteUserLayoutTransaction(LOCTEXT("DeleteUserLayoutTransaction", "Delete Layout"));
		EditorConsoleLayouts->Modify();
		EditorConsoleLayouts->DeleteUserLayout(LastSelectedItem.Get());

		if (LayoutIndex > 0)
		{
			// Select the previous User Layout in the array
			UDMXControlConsoleEditorGlobalLayoutUser* LayoutToSelect = UserLayouts[LayoutIndex - 1];
			EditorConsoleLayouts->SetActiveLayout(LayoutToSelect);

			UpdateComboBoxSource();
			UserLayoutsComboBox->SetSelectedItem(LayoutToSelect);
			LastSelectedItem = LayoutToSelect;
			LayoutNameText = FText::FromString(LastSelectedItem->GetLayoutName());
		}
		else
		{
			// If there are no more User Layouts switch to Default Layout
			UDMXControlConsoleEditorGlobalLayoutDefault* DefaultLayout = EditorConsoleLayouts->GetDefaultLayout();
			EditorConsoleLayouts->SetActiveLayout(DefaultLayout);

			UserLayoutsComboBox->ClearSelection();
			LastSelectedItem = nullptr;
			LayoutNameText = FText::GetEmpty();
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();
	}

	return FReply::Handled();
}

EVisibility SDMXControlConsoleEditorLayoutPicker::GetComboBoxVisibility() const
{
	bool bIsVisible = false;

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		bIsVisible = IsValid(ActiveLayout) && ActiveLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass();
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
