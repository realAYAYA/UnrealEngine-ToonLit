// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * Slate style set that defines all the styles for the StageMonitor UI
 */
class STAGEMONITOREDITOR_API FStageMonitorEditorStyle : public FSlateStyleSet
{
public:
	/** Access the singleton instance for this style set */
	static FStageMonitorEditorStyle& Get();


private:
	FStageMonitorEditorStyle();
	~FStageMonitorEditorStyle();
};
