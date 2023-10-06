// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModeToolkit.h"

class UEditorInteractiveToolsContext;
class SChaosClothAssetEditorRestSpaceViewport;
class SChaosClothAssetEditor3DViewport;
class SBaseCharacterFXEditorViewport;

/**
 * The cloth editor mode toolkit is responsible for the panel on the side in the cloth editor
 * that shows mode and tool properties. Tool buttons would go in Init().
 * NOTE: the cloth editor has two separate viewports/worlds/modemanagers/toolscontexts, so we need to track which
 * one is currently active.
 */
namespace UE::Chaos::ClothAsset
{
class CHAOSCLOTHASSETEDITOR_API FChaosClothAssetEditorModeToolkit : public FBaseCharacterFXEditorModeToolkit
{
public:

	void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode);

    /** For a specific tool palette category, construct and fill ToolbarBuilder with the category's tools **/
	virtual void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder) override;

	virtual const FSlateBrush* GetActiveToolIcon(const FString& Identifier) const override;

	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

	// IToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	void SetRestSpaceViewportWidget(TWeakPtr<SChaosClothAssetEditorRestSpaceViewport>);
	void SetPreviewViewportWidget(TWeakPtr<SChaosClothAssetEditor3DViewport>);

private:

	// Get the viewport widget associated with the given manager
	SBaseCharacterFXEditorViewport* GetViewportWidgetForManager(UInteractiveToolManager* Manager);

	UEditorInteractiveToolsContext* GetCurrentToolsContext();

	TWeakPtr<SChaosClothAssetEditorRestSpaceViewport> RestSpaceViewportWidget;
	TWeakPtr<SChaosClothAssetEditor3DViewport> PreviewViewportWidget;

};
} // namespace UE::Chaos::ClothAsset
