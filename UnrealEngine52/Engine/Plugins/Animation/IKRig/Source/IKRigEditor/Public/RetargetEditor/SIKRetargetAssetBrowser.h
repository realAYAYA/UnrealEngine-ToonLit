// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ContentBrowserDelegates.h"
#include "IKRetargetBatchOperation.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"


class FIKRetargetEditorController;

class SIKRetargetAssetBrowser : public SBox
{
public:
	SLATE_BEGIN_ARGS(SIKRetargetAssetBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FIKRetargetEditorController> InEditorController);

	void RefreshView();
	
private:

	FReply OnExportButtonClicked();
	bool IsExportButtonEnabled() const;
	
	void OnAssetDoubleClicked(const FAssetData& AssetData);
	bool OnShouldFilterAsset(const struct FAssetData& AssetData);
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets) const;

	/** Used to get the currently selected assets */
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
		
	/** editor controller */
	TWeakPtr<FIKRetargetEditorController> EditorController;

	/** the animation asset browser */
	TSharedPtr<SBox> AssetBrowserBox;

	/** remember the path the user chose between exports */
	FString PrevBatchOutputPath = "";

	friend FIKRetargetEditorController;
};

//** Dialog to select path to export to */
class SBatchExportDialog: public SWindow
{
public:
	SLATE_BEGIN_ARGS(SBatchExportDialog){}
	SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

	SBatchExportDialog() : UserResponse(EAppReturnType::Cancel)
	{
	}

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

	FIKRetargetBatchOperationContext BatchContext;

protected:

	/** The rename rule sample text */
	FText ExampleText;
	void UpdateExampleText();

	void OnPathChange(const FString& NewPath);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	EAppReturnType::Type UserResponse;
	FText AssetPath;
};
