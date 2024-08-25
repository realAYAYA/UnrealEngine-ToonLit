// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaShapesEditorStyle final : public FSlateStyleSet
{
public:
	static FAvaShapesEditorStyle& Get()
	{
		static FAvaShapesEditorStyle Instance;
		return Instance;
	}

	FAvaShapesEditorStyle();
	virtual ~FAvaShapesEditorStyle() override;
};
