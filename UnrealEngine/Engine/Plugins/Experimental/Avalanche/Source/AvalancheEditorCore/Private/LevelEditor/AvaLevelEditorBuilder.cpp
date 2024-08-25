// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditor/AvaLevelEditorBuilder.h"
#include "AvaLevelEditor.h"

TSharedRef<IAvaEditor> FAvaLevelEditorBuilder::CreateEditor()
{
	return MakeShared<FAvaLevelEditor>(*this);
}
