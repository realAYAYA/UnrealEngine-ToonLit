// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditor.h"

#include "AdvancedPreviewScene.h"
#include "ToolContextInterfaces.h"
#include "UVEditorToolkit.h"
#include "UVEditorSubsystem.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"

#include "AssetEditorModeManager.h"
#include "UVEditorMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UVEditor)

void UUVEditor::Initialize(const TArray<TObjectPtr<UObject>>& InObjects)
{
	// Make sure we have valid targets.
	UUVEditorSubsystem* UVSubsystem = GEditor->GetEditorSubsystem<UUVEditorSubsystem>();
	check(UVSubsystem && UVSubsystem->AreObjectsValidTargets(InObjects));

	OriginalObjectsToEdit = InObjects;

	// This will do a variety of things including registration of the asset editor, creation of the toolkit
	// (via CreateToolkit()), and creation of the editor mode manager within the toolkit.
	// The asset editor toolkit will do the rest of the necessary initialization inside its PostInitAssetEditor.
	UAssetEditor::Initialize();
}

IAssetEditorInstance* UUVEditor::GetInstanceInterface() 
{ 
	return ToolkitInstance; 
}

void UUVEditor::GetObjectsToEdit(TArray<UObject*>& OutObjects)
{
	OutObjects.Append(OriginalObjectsToEdit);
	check(OutObjects.Num() > 0);
}

TSharedPtr<FBaseAssetToolkit> UUVEditor::CreateToolkit()
{
	TSharedPtr<FUVEditorToolkit> Toolkit = MakeShared<FUVEditorToolkit>(this);

	return Toolkit;
}

