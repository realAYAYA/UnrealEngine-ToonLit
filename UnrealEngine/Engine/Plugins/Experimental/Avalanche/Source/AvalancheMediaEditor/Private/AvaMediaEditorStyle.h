// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaMediaEditorStyle final : public FSlateStyleSet
{
public:
	static FAvaMediaEditorStyle& Get()
	{
		static FAvaMediaEditorStyle Instance;
		return Instance;
	}

	FAvaMediaEditorStyle();
	virtual ~FAvaMediaEditorStyle() override;
};
