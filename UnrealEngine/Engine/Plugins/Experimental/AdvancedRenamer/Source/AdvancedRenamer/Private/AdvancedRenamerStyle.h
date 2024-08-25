// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FSlateStyleSet;
class ISlateStyle;
class FName;

class FAdvancedRenamerStyle
{
public:

	static void Initialize();

	static void Shutdown();

	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef<FSlateStyleSet> Create();
	static TSharedPtr<FSlateStyleSet> StyleInstance;

};
