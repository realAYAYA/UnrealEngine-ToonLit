// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaLevelViewportStyle final : public FSlateStyleSet
{
public:
	static FAvaLevelViewportStyle& Get()
	{
		static FAvaLevelViewportStyle Instance;
		return Instance;
	}

	FAvaLevelViewportStyle();
	virtual ~FAvaLevelViewportStyle() override;
};
