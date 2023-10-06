// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FPluginReferenceViewerStyle
	: public FSlateStyleSet
{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FPluginReferenceViewerStyle& Get();

private:

	FPluginReferenceViewerStyle();
	~FPluginReferenceViewerStyle();
};
