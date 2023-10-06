// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScriptableInteractiveTool.h"
#include "EditorScriptableInteractiveTool.generated.h"

/**
 * Editor-Only variant of UScriptableInteractiveTool, which gives access to Editor-Only BP functions
 */
UCLASS(Transient, Blueprintable)
class EDITORSCRIPTABLETOOLSFRAMEWORK_API UEditorScriptableInteractiveTool : public UScriptableInteractiveTool
{
	GENERATED_BODY()
public:

};


UCLASS(Transient, Blueprintable)
class EDITORSCRIPTABLETOOLSFRAMEWORK_API UEditorScriptableInteractiveToolPropertySet : public UScriptableInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

};
