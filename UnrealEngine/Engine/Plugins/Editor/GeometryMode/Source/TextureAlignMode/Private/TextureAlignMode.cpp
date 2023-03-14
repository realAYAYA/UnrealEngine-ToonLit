// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureAlignMode.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "TextureAlignEdMode.h"
#include "GeometryModeModule.h"

#define LOCTEXT_NAMESPACE "TextureAlignMode"


FName FTextureAlignMode::GetToolkitFName() const
{
	return FName("TextureAlignMode");
}

FText FTextureAlignMode::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Texture Align Mode");
}

class FEdMode* FTextureAlignMode::GetEditorMode() const
{
	return (FEdModeTexture*)GLevelEditorModeTools().GetActiveMode(FGeometryEditingModes::EM_TextureAlign);
}

#undef LOCTEXT_NAMESPACE
