// Copyright Epic Games, Inc. All Rights Reserved.
#include "LevelInstanceEditorModeToolkit.h"

#include "Internationalization/Internationalization.h"

class FAssetEditorModeUILayer;

#define LOCTEXT_NAMESPACE "LevelInstanceEditorModeToolkit"

FLevelInstanceEditorModeToolkit::FLevelInstanceEditorModeToolkit()
{
}

FName FLevelInstanceEditorModeToolkit::GetToolkitFName() const
{
	return FName("LevelInstanceEditorModeToolkit");
}

FText FLevelInstanceEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitDisplayName", "Level Instance Editor Mode");
}

void FLevelInstanceEditorModeToolkit::SetModeUILayer(const TSharedPtr<FAssetEditorModeUILayer> InLayer)
{
	// Do nothing (prevents Ed Mode Tab spawn)
}

#undef LOCTEXT_NAMESPACE