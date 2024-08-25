// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaTagEditorStyle final : public FSlateStyleSet
{
public:
	static FAvaTagEditorStyle& Get()
	{
		static FAvaTagEditorStyle Instance;
		return Instance;
	}

	FAvaTagEditorStyle();
	virtual ~FAvaTagEditorStyle() override;
};
