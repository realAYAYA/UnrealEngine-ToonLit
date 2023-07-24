// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * Slate style set that defines all the styles for the Switchboard Toolbar
 */
class SWITCHBOARDEDITOR_API FSwitchboardEditorStyle : public FSlateStyleSet
{
public:
	static FSwitchboardEditorStyle& Get();

private:
	FSwitchboardEditorStyle();
	~FSwitchboardEditorStyle();
};
