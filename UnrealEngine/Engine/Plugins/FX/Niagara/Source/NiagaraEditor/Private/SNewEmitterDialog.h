// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraNewAssetDialog.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserDelegates.h"

class SNiagaraAssetPickerList;

/** A modal dialog to collect information needed to create a new niagara system. */
class SNewEmitterDialog : public SNiagaraNewAssetDialog
{
public:
	SLATE_BEGIN_ARGS(SNewEmitterDialog)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<FAssetData> GetSelectedEmitterAsset() const;

	bool GetUseInheritance() const;

private:
	void GetSelectedEmitterAssets_NewAssets(TArray<FAssetData>& OutSelectedAssets) const;

	void GetSelectedEmitterAssets_CopyAssets(TArray<FAssetData>& OutSelectedAssets) const;

	void InheritanceOptionConfirmed();

	void CheckUseInheritance();

private:
	TSharedPtr<SNiagaraAssetPickerList> NewAssetPicker;
	TSharedPtr<SNiagaraAssetPickerList> InheritAssetPicker;
	TSharedPtr<SNiagaraAssetPickerList> CopyAssetPicker;

	bool bUseInheritance = false;
};