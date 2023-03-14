// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ContentBrowserDelegates.h"
#include "Widgets/Layout/SBox.h"


class FIKRigEditorController;

class SIKRigAssetBrowser : public SBox
{
public:
	SLATE_BEGIN_ARGS(SIKRigAssetBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FIKRigEditorController> InEditorController);

private:

	void RefreshView();

	void OnPathChange(const FString& NewPath);
	
	void OnAssetDoubleClicked(const FAssetData& AssetData);
	bool OnShouldFilterAsset(const struct FAssetData& AssetData);
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets) const;
	
	/** Used to get the currently selected assets */
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
		
	/** editor controller */
	TWeakPtr<FIKRigEditorController> EditorController;

	/** the animation asset browser */
	TSharedPtr<SBox> AssetBrowserBox;

	friend FIKRigEditorController;
};
