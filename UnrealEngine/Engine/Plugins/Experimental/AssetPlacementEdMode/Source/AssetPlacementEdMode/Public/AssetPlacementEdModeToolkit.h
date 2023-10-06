// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

/**
 * This FModeToolkit just creates a basic UI panel that allows various InteractiveTools to
 * be initialized, and a DetailsView used to show properties of the active Tool.
 */
class FAssetPlacementEdModeToolkit : public FModeToolkit
{
public:
	FAssetPlacementEdModeToolkit();
	
	// FModeToolkit interface 
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;

	// IToolkit interface 
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual void InvokeUI() override;
protected:
	virtual void RequestModeUITabs() override;
	TSharedRef<SDockTab> CreateAssetPalette(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> CreatePaletteItemDetailsView(const FSpawnTabArgs& Args);
	EVisibility GetDetailsViewHintTextVisibility() const;

private:
	TWeakPtr<SDockTab> AssetPaletteTab;
	TWeakPtr<SDockTab> PaletteItemDetailsViewTab;
	FMinorTabConfig AssetPaletteInfo;
	FMinorTabConfig PaletteItemDetailsViewInfo;
	TSharedPtr<IDetailsView> PaletteItemDetailsView;
};
