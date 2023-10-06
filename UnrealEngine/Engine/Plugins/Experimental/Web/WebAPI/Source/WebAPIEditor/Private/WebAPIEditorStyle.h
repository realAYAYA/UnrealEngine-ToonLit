// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

class ISlateStyle;

/** Manages the style which provides resources for WebAPI Editor widgets. */
class FWebAPIEditorStyle
{
public:
	static void Register();
	static void Unregister();

	static FName GetStyleSetName();
	
	static const ISlateStyle& Get();

private:
	static TUniquePtr<class FSlateStyleSet> Create();
	static TUniquePtr<class FSlateStyleSet> StyleInstance;
};
