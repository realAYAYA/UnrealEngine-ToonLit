// Copyright Epic Games, Inc. All Rights Reserved.

#include "ULevelAssetEditor.h"

#include "EditorModeManager.h"
#include "LevelAssetEditorToolkit.h"
#include "Engine/Level.h"

void ULevelAssetEditor::GetObjectsToEdit(TArray<UObject*>& InObjectsToEdit)
{
	InObjectsToEdit.Add(NewObject<ULevel>(this, NAME_None, RF_Transient));
}

TSharedPtr<FBaseAssetToolkit> ULevelAssetEditor::CreateToolkit()
{
	return MakeShared<FLevelEditorAssetToolkit>(this);
}
