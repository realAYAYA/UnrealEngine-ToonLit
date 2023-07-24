// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExampleCharacterFXEditor.h"
#include "ExampleCharacterFXEditorToolkit.h"

TSharedPtr<FBaseAssetToolkit> UExampleCharacterFXEditor::CreateToolkit()
{
	TSharedPtr<FExampleCharacterFXEditorToolkit> Toolkit = MakeShared<FExampleCharacterFXEditorToolkit>(this);
	return Toolkit;
}
