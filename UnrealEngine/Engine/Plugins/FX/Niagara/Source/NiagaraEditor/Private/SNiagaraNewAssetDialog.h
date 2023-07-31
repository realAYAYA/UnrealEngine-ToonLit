// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "IAssetTypeActions.h"
#include "ContentBrowserDelegates.h"
#include "Widgets/Workflow/SWizard.h"


class SNiagaraAssetPickerList;
class SBox;

/** A modal dialog to collect information needed to create a new niagara system. */
class SNiagaraNewAssetDialog : public SWindow
{
public:
	DECLARE_DELEGATE_OneParam(FOnGetSelectedAssetsFromPicker, TArray<FAssetData>& /* OutSelectedAssets */);
	DECLARE_DELEGATE(FOnSelectionConfirmed);

public:
	class FNiagaraNewAssetDialogOption
	{
	public:
		FText OptionText;
		FText OptionDescription;
		FText AssetPickerHeader;
		TSharedRef<SWidget> AssetPicker;
		TSharedPtr<SWidget> WidgetToFocusOnEntry;
		FOnGetSelectedAssetsFromPicker OnGetSelectedAssetsFromPicker;
		FOnSelectionConfirmed OnSelectionConfirmed;

		FNiagaraNewAssetDialogOption(FText InOptionText, FText InOptionDescription, FText InAssetPickerHeader, FOnGetSelectedAssetsFromPicker InOnGetSelectedAssetsFromPicker, FOnSelectionConfirmed InOnSelecitonConfirmed, TSharedRef<SWidget> InAssetPicker, TSharedPtr<SWidget> InWidgetToFocus = nullptr)
			: OptionText(InOptionText)
			, OptionDescription(InOptionDescription)
			, AssetPickerHeader(InAssetPickerHeader)
			, AssetPicker(InAssetPicker)
			, WidgetToFocusOnEntry(InWidgetToFocus)
			, OnGetSelectedAssetsFromPicker(InOnGetSelectedAssetsFromPicker)
			, OnSelectionConfirmed(InOnSelecitonConfirmed)
		{
		}
	};

public:
	SLATE_BEGIN_ARGS(SNiagaraNewAssetDialog)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FName InSaveConfigKey, FText AssetTypeDisplayName, TArray<FNiagaraNewAssetDialogOption> InOptions);
	void ShowAssetPicker();
	void ResetStage();
	bool GetUserConfirmedSelection() const;

protected:
	const TArray<FAssetData>& GetSelectedAssets() const;

	void ConfirmSelection();
	void ConfirmSelection(const FAssetData& AssetData);

	int32 GetSelectedObjectIndex() const { return SelectedOptionIndex; };

protected:
	TSharedPtr<SBox> AssetSettingsPage;

private:

	void OnWindowClosed(const TSharedRef<SWindow>& Window);

	FSlateColor GetOptionBorderColor(int32 OptionIndex) const;
	FReply OnOptionDoubleClicked(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 OptionIndex);

	ECheckBoxState GetOptionCheckBoxState(int32 OptionIndex) const;

	void OptionCheckBoxStateChanged(ECheckBoxState InCheckBoxState, int32 OptionIndex);
	FSlateColor GetOptionTextColor(int32 OptionIndex) const;

	FText GetAssetPickersLabelText() const;
	bool IsOkButtonEnabled() const;
	void OnOkButtonClicked();
	void OnCancelButtonClicked();
	bool HasAssetPage() const;
	void SaveConfig();

private:
	FName SaveConfigKey;
	TSharedPtr<SWizard> Wizard;
	TArray<FNiagaraNewAssetDialogOption> Options;
	int32 SelectedOptionIndex;
	bool bUserConfirmedSelection;
	TArray<FAssetData> SelectedAssets;
	bool bOnAssetStage;
};
