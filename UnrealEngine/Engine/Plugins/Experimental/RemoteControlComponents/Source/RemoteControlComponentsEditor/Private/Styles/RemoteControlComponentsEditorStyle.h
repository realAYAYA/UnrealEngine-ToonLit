// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FRemoteControlComponentsEditorStyle : public FSlateStyleSet
{
public:
	FRemoteControlComponentsEditorStyle();
	
	virtual ~FRemoteControlComponentsEditorStyle() override;

	static FRemoteControlComponentsEditorStyle& Get()
	{
		static FRemoteControlComponentsEditorStyle StyleSet;
		return StyleSet;
	}
};
