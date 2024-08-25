// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"

class FAvaInteractiveToolsEdModeToolkit : public FModeToolkit
{
public:
	FAvaInteractiveToolsEdModeToolkit();
	virtual ~FAvaInteractiveToolsEdModeToolkit() override;

	//~ Begin IToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;
	//~ End IToolkit

	//~ Begin FModeToolkit
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual void GetToolPaletteNames(TArray<FName>& InPaletteName) const override;
	virtual FText GetToolPaletteDisplayName(FName InPaletteName) const override;
	virtual void OnToolPaletteChanged(FName InPaletteName) override;
	virtual bool HasIntegratedToolPalettes() const override { return false; }
	virtual bool HasExclusiveToolPalettes() const override { return false; }
	virtual void OnToolStarted(UInteractiveToolManager* InManager, UInteractiveTool* InTool) override;
	virtual void OnToolEnded(UInteractiveToolManager* InManager, UInteractiveTool* InTool) override;
	virtual void InvokeUI() override;
	//~ End FModeToolkit

protected:
	//~ Begin FModeToolkit
	virtual void RequestModeUITabs() override;
	//~ End FModeToolkit

private:
	TSharedPtr<SWidget> ToolkitWidget;

	/** A utility function to register the tool palettes with the ToolkitBuilder */
	void RegisterPalettes();

	FText GetToolWarningText() const;
};
