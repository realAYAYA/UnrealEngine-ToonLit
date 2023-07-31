// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "IAssetTypeActions.h"
#include "ContentBrowserDelegates.h"
#include "SNiagaraNewAssetDialog.h"

class SNiagaraAssetPickerList;
class SWrapBox;

/** A modal dialog to collect information needed to create a new niagara system. */
class SNewSystemDialog : public SNiagaraNewAssetDialog
{
public:
	SLATE_BEGIN_ARGS(SNewSystemDialog)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<FAssetData> GetSelectedSystemAsset() const;

	TArray<FAssetData> GetSelectedEmitterAssets() const;

private:

	void OnEmitterAssetsActivated(const FAssetData& ActivatedTemplateAsset);

	void GetSelectedSystemTemplateAssets(TArray<FAssetData>& OutSelectedAssets);

	void GetSelectedProjectSystemAssets(TArray<FAssetData>& OutSelectedAssets);

	void GetSelectedProjectEmiterAssets(TArray<FAssetData>& OutSelectedAssets);

	bool IsAddEmittersToSelectionButtonEnabled() const;
	
	FReply AddEmittersToSelectionButtonClicked();

	void AddEmitterAssetsToSelection(const TArray<FAssetData>& EmitterAssets);

	FReply RemoveEmitterFromSelectionButtonClicked(FAssetData EmitterAsset);

private:

	TArray<FAssetData> SelectedEmitterAssets;

	TArray<TSharedPtr<SWidget>> SelectedEmitterAssetWidgets;

	TSharedPtr<SWrapBox> SelectedEmitterBox;

	TSharedPtr<SNiagaraAssetPickerList> TemplateBehaviorAssetPicker;

	TSharedPtr<SNiagaraAssetPickerList> EmitterAssetPicker;

	TSharedPtr<SNiagaraAssetPickerList> CopySystemAssetPicker;
};