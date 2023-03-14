// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * 
 */
class FPluginStyleSet : public FSlateStyleSet
{
public:
	FPluginStyleSet(const FName& InPluginName, const FName& InStyleSetName = NAME_None);
};

/**
 * 
 */
class FSearchStyle : public FPluginStyleSet
{
public:

	/** Access the singleton instance for this style set */
	static FSearchStyle& Get();

private:

	FSearchStyle();
	~FSearchStyle();
};
