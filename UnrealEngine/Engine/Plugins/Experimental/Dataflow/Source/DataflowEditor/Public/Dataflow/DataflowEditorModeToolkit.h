// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModeToolkit.h"

class UEditorInteractiveToolsContext;
class SDataflowEditorViewport;
class SBaseCharacterFXEditorViewport;

/**
 * The dataflow editor mode toolkit is responsible for the panel on the side in the dataflow editor
 * that shows mode and tool properties. Tool buttons would go in Init().
 * Note: When there are separate viewports/worlds/modemanagers/toolscontexts, this ModeToolkit will track which
 * one is currently active.
 */

class DATAFLOWEDITOR_API FDataflowEditorModeToolkit : public FBaseCharacterFXEditorModeToolkit
{
public:
	void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode);

	// Add buttons to the toolbar with the specified name
	// Note: Most FModeToolkits would define BuildToolPalette, but we are putting buttons in the top toolbar instead
	void BuildEditorToolBar(const FName& EditorToolBarName);

	// FBaseCharacterFXEditorModeToolkit interface
	virtual const FSlateBrush* GetActiveToolIcon(const FString& Identifier) const override;

	// FModeToolkit interface
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	void SetRestSpaceViewportWidget(TWeakPtr<SDataflowEditorViewport>);
	//void SetPreviewViewportWidget(TWeakPtr<SChaosClothAssetEditor3DViewport>);

private:

	// Get the viewport widget associated with the given manager
	// TODO: This should not be necessary any more as we do not run tools in the Cloth Preview Viewport (JIRA UE-201248)
	SBaseCharacterFXEditorViewport* GetViewportWidgetForManager(UInteractiveToolManager* Manager);

	UEditorInteractiveToolsContext* GetCurrentToolsContext();

	TWeakPtr<SDataflowEditorViewport> RestSpaceViewportWidget;
	//TWeakPtr<SChaosClothAssetEditor3DViewport> PreviewViewportWidget;
};
