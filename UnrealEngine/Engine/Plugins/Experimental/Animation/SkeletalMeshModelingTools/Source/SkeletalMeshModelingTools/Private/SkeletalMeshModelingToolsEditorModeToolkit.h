// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"


class IToolkitHost;
class STextBlock;
class SButton;


class FSkeletalMeshModelingToolsEditorModeToolkit : 
	 public FModeToolkit
{
public:
	// IToolkit overrides
	void Init(const TSharedPtr<IToolkitHost>& InToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	FName GetToolkitFName() const override;
	FText GetBaseToolkitName() const override;
	TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

	void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

	FText GetActiveToolDisplayName() const override;
	FText GetActiveToolMessage() const override;

	void GetToolPaletteNames(TArray<FName>& InPaletteName) const override;
	FText GetToolPaletteDisplayName(FName PaletteName) const override;
	void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder) override;
	void OnToolPaletteChanged(FName PaletteName) override;


private:
	void PostNotification(const FText& Message);
	void ClearNotification();

	void PostWarning(const FText& Message);
	void ClearWarning();

	void UpdateActiveToolProperties(UInteractiveTool* Tool);

	FText ActiveToolName;
	FText ActiveToolMessage;

	TSharedPtr<SWidget> ToolkitWidget;
	TSharedPtr<IDetailsView> DetailsView;

	TSharedPtr<SWidget> ViewportOverlayWidget;
	const FSlateBrush* ActiveToolIcon = nullptr;
	
	TSharedPtr<STextBlock> ModeWarningArea;
	TSharedPtr<STextBlock> ModeHeaderArea;
	TSharedPtr<STextBlock> ToolWarningArea;
};
