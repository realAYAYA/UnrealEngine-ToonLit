// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModeToolkit.h"

/**
 * The cloth editor mode toolkit is responsible for the panel on the side in the cloth editor
 * that shows mode and tool properties. Tool buttons would go in Init().
 */
class CHAOSCLOTHASSETEDITOR_API FChaosClothAssetEditorModeToolkit : public FBaseCharacterFXEditorModeToolkit
{
public:

    /** For a specific tool palette category, construct and fill ToolbarBuilder with the category's tools **/
	virtual void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder) override;

	virtual const FSlateBrush* GetActiveToolIcon(const FString& Identifier) const override;

	// IToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
};
