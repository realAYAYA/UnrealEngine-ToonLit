// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/ScriptableSingleClickTool.h"
#include "EditorScriptableSingleClickTool.generated.h"

/**
 * Editor-Only variant of UScriptableSingleClickTool, which gives access to Editor-Only BP functions
 */
UCLASS(Transient, Blueprintable)
class EDITORSCRIPTABLETOOLSFRAMEWORK_API UEditorScriptableSingleClickTool : public UScriptableSingleClickTool
{
	GENERATED_BODY()
public:

};