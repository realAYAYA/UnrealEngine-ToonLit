// Copyright Epic Games, Inc. All Rights Reserved.

#include "UExampleAssetEditor.h"

#include "EditorModeManager.h"
#include "ExampleAssetToolkit.h"
#include "Engine/Level.h"

void UExampleAssetEditor::GetObjectsToEdit(TArray<UObject*>& InObjectsToEdit)
{
	InObjectsToEdit.Add(NewObject<ULevel>(this, NAME_None, RF_Transient));
}

TSharedPtr<FBaseAssetToolkit> UExampleAssetEditor::CreateToolkit()
{
	return MakeShared<FExampleAssetToolkit>(this);
}
