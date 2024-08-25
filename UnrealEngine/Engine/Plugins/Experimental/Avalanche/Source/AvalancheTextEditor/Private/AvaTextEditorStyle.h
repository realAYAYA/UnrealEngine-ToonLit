// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaTextEditorStyle final : public FSlateStyleSet
{
public:
	static FAvaTextEditorStyle& Get()
	{
		static FAvaTextEditorStyle Instance;
		return Instance;
	}

	FAvaTextEditorStyle();
	virtual ~FAvaTextEditorStyle() override;
};
