// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StatusBarSubsystem.h"
#include "Toolkits/BaseToolkit.h"


class IToolkitHost;
class STextBlock;
class SButton;


class FSkeletalMeshModelingToolsEditorModeToolkit : public FModeToolkit
{
public:

	virtual ~FSkeletalMeshModelingToolsEditorModeToolkit() override;
	
	// IToolkit overrides
	virtual void Init(const TSharedPtr<IToolkitHost>& InToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override { return ToolkitWidget; }

	// FModeToolkit overrides
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual FText GetActiveToolDisplayName() const override;
	virtual FText GetActiveToolMessage() const override;

private:
	void PostNotification(const FText& Message);
	void ClearNotification();

	void PostWarning(const FText& Message);
	void ClearWarning();

	void UpdateActiveToolProperties(UInteractiveTool* Tool);

	void RegisterPalettes();
	FDelegateHandle ActivePaletteChangedHandle;

	void MakeToolAcceptCancelWidget();

	FText ActiveToolName;
	FText ActiveToolMessage;
	FStatusBarMessageHandle ActiveToolMessageHandle;

	TSharedPtr<SWidget> ToolkitWidget;
	TSharedPtr<IDetailsView> DetailsView;

	TSharedPtr<SWidget> ViewportOverlayWidget;
	const FSlateBrush* ActiveToolIcon = nullptr;
	
	TSharedPtr<STextBlock> ModeWarningArea;
	TSharedPtr<STextBlock> ModeHeaderArea;
	TSharedPtr<STextBlock> ToolWarningArea;
};
