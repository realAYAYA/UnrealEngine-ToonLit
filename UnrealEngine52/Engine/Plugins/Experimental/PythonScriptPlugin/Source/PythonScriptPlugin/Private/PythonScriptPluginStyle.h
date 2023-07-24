// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * Slate style set that defines the styles for Python UI in the Editor 
 */
class FPythonScriptPluginEditorStyle
	: public FSlateStyleSet
{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FPythonScriptPluginEditorStyle& Get();

private:

	FPythonScriptPluginEditorStyle();
	~FPythonScriptPluginEditorStyle();
};
