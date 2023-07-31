// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraNewAssetDialog.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEmitter.h"
#include "SNiagaraAssetPickerList.h"

#include "AssetRegistry/AssetData.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Workflow/SWizard.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNiagaraNewAssetDialog"

void SNiagaraNewAssetDialog::Construct(const FArguments& InArgs, FName InSaveConfigKey, FText AssetTypeDisplayName, TArray<FNiagaraNewAssetDialogOption> InOptions)
{
	bUserConfirmedSelection = false;

	SaveConfigKey = InSaveConfigKey;
	FNiagaraNewAssetDialogConfig DialogConfig = GetDefault<UNiagaraEditorSettings>()->GetNewAssetDailogConfig(SaveConfigKey);
	SelectedOptionIndex = DialogConfig.SelectedOptionIndex;

	Options = InOptions;
	// It is possible that the number of options has changed since the options config was last saved; make sure the SelectedOptionsIndex from the config is valid.
	if (SelectedOptionIndex > Options.Num() - 1)
	{
		SelectedOptionIndex = Options.Num() - 1;
	}

	SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SNiagaraNewAssetDialog::OnWindowClosed));
	
	TSharedPtr<SVerticalBox> OptionsBox;
	TSharedPtr<SOverlay> AssetPickerOverlay;

	TSharedRef<SVerticalBox> RootBox =
		SNew(SVerticalBox)

		// Creation mode toggle buttons.
		+ SVerticalBox::Slot()
		.Padding(0, 5, 0, 5)
		.AutoHeight()
		[
			SNew(SBox)
			.Padding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
			[
					
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(7))
				[
					SAssignNew(OptionsBox, SVerticalBox)
				]
			]
		];


	int32 OptionIndex = 0;
	for (FNiagaraNewAssetDialogOption& Option : Options)
	{
		OptionsBox->AddSlot()
			.Padding(0, 0, 0, OptionIndex < Options.Num() - 1 ? 7 : 0)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.NewAssetDialog.SubBorder"))
				.BorderBackgroundColor(this, &SNiagaraNewAssetDialog::GetOptionBorderColor, OptionIndex)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.CheckBoxContentUsesAutoWidth(false)
					.IsChecked(this, &SNiagaraNewAssetDialog::GetOptionCheckBoxState, OptionIndex)
					.OnCheckStateChanged(this, &SNiagaraNewAssetDialog::OptionCheckBoxStateChanged, OptionIndex)
					.Content()
					[
						// this border catches the double click before the checkbox can
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
						.OnMouseDoubleClick(this, &SNiagaraNewAssetDialog::OnOptionDoubleClicked, OptionIndex)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5, 2)
							[		
								SNew(STextBlock)
								.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.OptionText")
								.ColorAndOpacity(this, &SNiagaraNewAssetDialog::GetOptionTextColor, OptionIndex)
								.Text(Option.OptionText)
								.AutoWrapText(true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(5, 2, 5, 7)
							[
								SNew(STextBlock)
								.ColorAndOpacity(this, &SNiagaraNewAssetDialog::GetOptionTextColor, OptionIndex)
								.Text(Option.OptionDescription)
								.AutoWrapText(true)
							]
						]
					]
				]
			];

		OptionIndex++;
	}

	SWindow::Construct(SWindow::FArguments()
		.Title(FText::Format(LOCTEXT("NewEmitterDialogTitle", "Pick a starting point for your {0}"), AssetTypeDisplayName))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(DialogConfig.WindowSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SAssignNew(Wizard, SWizard)
			.OnCanceled(this, &SNiagaraNewAssetDialog::OnCancelButtonClicked)
			.OnFinished(this, &SNiagaraNewAssetDialog::OnOkButtonClicked)
			.CanFinish(this, &SNiagaraNewAssetDialog::IsOkButtonEnabled)
			.ShowPageList(false)
			+SWizard::Page()
			.CanShow(true)
			.OnEnter(this, &SNiagaraNewAssetDialog::ResetStage)
			[
				RootBox
			]
			+ SWizard::Page()
			.CanShow(this, &SNiagaraNewAssetDialog::HasAssetPage)
			.OnEnter(this, &SNiagaraNewAssetDialog::ShowAssetPicker)
			[
				SAssignNew(AssetSettingsPage, SBox)
			]
		]);
}

void SNiagaraNewAssetDialog::ShowAssetPicker()
{
	bOnAssetStage = true;
	AssetSettingsPage->SetContent(Options[SelectedOptionIndex].AssetPicker);

	if(Options[SelectedOptionIndex].WidgetToFocusOnEntry.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Options[SelectedOptionIndex].WidgetToFocusOnEntry);
	}
}

void SNiagaraNewAssetDialog::ResetStage()
{
	bOnAssetStage = false;
}

bool SNiagaraNewAssetDialog::GetUserConfirmedSelection() const
{
	return bUserConfirmedSelection;
}

const TArray<FAssetData>& SNiagaraNewAssetDialog::GetSelectedAssets() const
{
	return SelectedAssets;
}

void SNiagaraNewAssetDialog::ConfirmSelection()
{
	const FNiagaraNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnGetSelectedAssetsFromPicker.IsBound())
	{
		SelectedOption.OnGetSelectedAssetsFromPicker.Execute(SelectedAssets);
		ensureMsgf(SelectedAssets.Num() > 0, TEXT("No assets selected when dialog was confirmed."));
	}
	SelectedOption.OnSelectionConfirmed.ExecuteIfBound();
	bUserConfirmedSelection = true;
	RequestDestroyWindow();
}

void SNiagaraNewAssetDialog::ConfirmSelection(const FAssetData& AssetData)
{
	SelectedAssets.Add(AssetData);
	
	const FNiagaraNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	SelectedOption.OnSelectionConfirmed.ExecuteIfBound();
	bUserConfirmedSelection = true;
	RequestDestroyWindow();
}

void SNiagaraNewAssetDialog::OnWindowClosed(const TSharedRef<SWindow>& Window)
{
	SaveConfig();
}

FSlateColor SNiagaraNewAssetDialog::GetOptionBorderColor(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex
		? FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.NewAssetDialog.ActiveOptionBorderColor")
		: FSlateColor(FLinearColor::Transparent);
}

FReply SNiagaraNewAssetDialog::OnOptionDoubleClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 OptionIndex)
{
	SelectedOptionIndex = OptionIndex;
	if(Wizard->CanShowPage(Wizard->GetCurrentPageIndex() + 1))
	{
		Wizard->AdvanceToPage(Wizard->GetCurrentPageIndex() + 1);
		return FReply::Handled();
	}
	// if we can't advance, we attempt to finish instead
	else
	{
		if(IsOkButtonEnabled())
		{
			OnOkButtonClicked();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FSlateColor SNiagaraNewAssetDialog::GetOptionTextColor(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex
		? FSlateColor(FLinearColor::White)
		: FSlateColor::UseForeground();
}

ECheckBoxState SNiagaraNewAssetDialog::GetOptionCheckBoxState(int32 OptionIndex) const
{
	return SelectedOptionIndex == OptionIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraNewAssetDialog::OptionCheckBoxStateChanged(ECheckBoxState InCheckBoxState, int32 OptionIndex)
{
	if (InCheckBoxState == ECheckBoxState::Checked)
	{
		SelectedOptionIndex = OptionIndex;
	}
}

FText SNiagaraNewAssetDialog::GetAssetPickersLabelText() const
{
	return Options[SelectedOptionIndex].AssetPickerHeader;
}

bool SNiagaraNewAssetDialog::IsOkButtonEnabled() const
{
	const FNiagaraNewAssetDialogOption& SelectedOption = Options[SelectedOptionIndex];
	if (SelectedOption.OnGetSelectedAssetsFromPicker.IsBound())
	{
		TArray<FAssetData> TempSelectedAssets;
		SelectedOption.OnGetSelectedAssetsFromPicker.Execute(TempSelectedAssets);
		return bOnAssetStage && TempSelectedAssets.Num() != 0;
	}
	else
	{
		return true;
	}
}

void SNiagaraNewAssetDialog::OnOkButtonClicked()
{
	ConfirmSelection();
}

void SNiagaraNewAssetDialog::OnCancelButtonClicked()
{
	bUserConfirmedSelection = false;
	SelectedAssets.Empty();

	RequestDestroyWindow();
}

bool SNiagaraNewAssetDialog::HasAssetPage() const
{
	return !IsOkButtonEnabled();
}

void SNiagaraNewAssetDialog::SaveConfig()
{
	FNiagaraNewAssetDialogConfig Config;
	Config.SelectedOptionIndex = SelectedOptionIndex;
	Config.WindowSize = GetClientSizeInScreen() / GetDPIScaleFactor();

	GetMutableDefault<UNiagaraEditorSettings>()->SetNewAssetDialogConfig(SaveConfigKey, Config);
}

#undef LOCTEXT_NAMESPACE
