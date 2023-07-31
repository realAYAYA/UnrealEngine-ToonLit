// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityBlueprint.h"

/////////////////////////////////////////////////////
// UEditorUtilityBlueprint

UEditorUtilityBlueprint::UEditorUtilityBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UEditorUtilityBlueprint::SupportedByDefaultBlueprintFactory() const
{
	return false;
}

bool UEditorUtilityBlueprint::AlwaysCompileOnLoad() const
{
	return true;
}
