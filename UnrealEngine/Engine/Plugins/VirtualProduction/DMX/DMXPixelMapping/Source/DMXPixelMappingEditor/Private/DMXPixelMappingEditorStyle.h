// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"


class FDMXPixelMappingEditorStyle
	: public FSlateStyleSet
{
public:
	FDMXPixelMappingEditorStyle();

	virtual ~FDMXPixelMappingEditorStyle();

	/** @return The Slate style set for pixel mapping editor widgets */
	static const ISlateStyle& Get();
};
