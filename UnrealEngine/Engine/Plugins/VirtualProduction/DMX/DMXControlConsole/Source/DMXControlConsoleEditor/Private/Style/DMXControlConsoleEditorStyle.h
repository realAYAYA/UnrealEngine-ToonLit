// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"


/**  DMX Control Console Editor Style */
class FDMXControlConsoleEditorStyle
	: public FSlateStyleSet
{
public:
	/** Constructor */
	FDMXControlConsoleEditorStyle();

	/** Desonstructor */
	virtual ~FDMXControlConsoleEditorStyle();

	/**  Returns the style instance */
	static const FDMXControlConsoleEditorStyle& Get();
};
