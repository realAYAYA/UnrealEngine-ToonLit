// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * Slate style set for UV Editor
 */
class UVEDITORTOOLS_API FUVEditorStyle
	: public FSlateStyleSet
{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FUVEditorStyle& Get();

private:

	FUVEditorStyle();
	~FUVEditorStyle();
};