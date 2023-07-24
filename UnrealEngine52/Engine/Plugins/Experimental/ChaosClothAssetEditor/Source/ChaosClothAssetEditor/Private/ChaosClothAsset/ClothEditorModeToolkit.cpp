// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorModeToolkit.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FChaosClothAssetEditorModeToolkit"

FName FChaosClothAssetEditorModeToolkit::GetToolkitFName() const
{
	return FName("ChaosClothAssetEditorMode");
}

FText FChaosClothAssetEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("ChaosClothAssetEditorModeToolkit", "DisplayName", "ChaosClothAssetEditorMode");
}

void FChaosClothAssetEditorModeToolkit::BuildToolPalette(FName PaletteIndex, class FToolBarBuilder& ToolbarBuilder)
{
	const FChaosClothAssetEditorCommands& Commands = FChaosClothAssetEditorCommands::Get();

	// TODO: make separate palettes to split up the tools tab
	if (PaletteIndex == FBaseCharacterFXEditorModeToolkit::ToolsTabName)
	{
		// TODO: Re-add the remesh tool when we have a way to remesh both 2d and 3d rest space meshes at the same time
		//ToolbarBuilder.AddToolBarButton(Commands.BeginRemeshTool);

		ToolbarBuilder.AddToolBarButton(Commands.BeginAttributeEditorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginWeightMapPaintTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginClothTrainingTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginTransferSkinWeightsTool);
	}
}

const FSlateBrush* FChaosClothAssetEditorModeToolkit::GetActiveToolIcon(const FString& ActiveToolIdentifier) const
{
	FName ActiveToolIconName = ISlateStyle::Join(FChaosClothAssetEditorCommands::Get().GetContextName(), TCHAR_TO_ANSI(*ActiveToolIdentifier));
	return FChaosClothAssetEditorStyle::Get().GetOptionalBrush(ActiveToolIconName);
}

#undef LOCTEXT_NAMESPACE
