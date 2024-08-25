// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/ScriptableClickDragTool.h"
#include "EditorScriptableClickDragTool.generated.h"

/**
 * Editor-Only variant of UScriptableClickDragTool, which gives access to Editor-Only BP functions
 */
UCLASS(Transient, Blueprintable)
class EDITORSCRIPTABLETOOLSFRAMEWORK_API UEditorScriptableClickDragTool : public UScriptableClickDragTool
{
	GENERATED_BODY()
public:

};