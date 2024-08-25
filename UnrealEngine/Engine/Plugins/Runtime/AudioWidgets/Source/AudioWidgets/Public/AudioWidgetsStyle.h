// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/**
 * Slate style set that defines all the styles for audio widgets
 */
class AUDIOWIDGETS_API FAudioWidgetsStyle
	: public FSlateStyleSet

{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FAudioWidgetsStyle& Get();

private:
	void SetResources();

	FAudioWidgetsStyle();
	~FAudioWidgetsStyle();
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#endif
