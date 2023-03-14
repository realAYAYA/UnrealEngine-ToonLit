// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

/**
 * Slate style set that defines all the styles for audio widgets
 */
class FAudioWidgetsStyle
	: public FSlateStyleSet

{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FAudioWidgetsStyle& Get();

private:

	FAudioWidgetsStyle();
	~FAudioWidgetsStyle();
};
