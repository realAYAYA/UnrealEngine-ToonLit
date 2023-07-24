// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FAssetData;
class FUICommandList;
class SComboButton;


/** A custom asset picker for control consoles */
class SDMXControlConsoleEditorAssetPicker
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorAssetPicker)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

private:
	/** Registers commands for this widget */
	void RegisterCommands();

	/** Creates the menu this picker consists of */
	TSharedRef<SWidget> CreateMenu();

	/** Returns the editor console name as text */
	FText GetEditorConsoleName() const;

	/** Called when an asset was selected in the asset picker */
	void OnAssetSelected(const FAssetData& AssetData);

	/** Called when enter was pressed in the asset picker */
	void OnAssetEnterPressed(const TArray<FAssetData>& SelectedAssets);

	/** Combo button that expands the asset picker */
	TSharedPtr<SComboButton> AssetComboButton;

	/** Command list for the Asset Combo Button */
	TSharedPtr<FUICommandList> CommandList;
};
