// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FName;
class ISlateStyle;

class FEditorWidgetsStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();

	static const FName& GetStyleSetName();

private:
	/** Singleton instances of this style. */
	static TSharedPtr<class FSlateStyleSet> StyleSet;	
};
