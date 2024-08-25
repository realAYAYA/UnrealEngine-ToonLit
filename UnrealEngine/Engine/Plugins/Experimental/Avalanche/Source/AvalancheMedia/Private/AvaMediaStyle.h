// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaMediaStyle final : public FSlateStyleSet
{
public:
	static FAvaMediaStyle& Get()
	{
		static FAvaMediaStyle Instance;
		return Instance;
	}

	FAvaMediaStyle();
	virtual ~FAvaMediaStyle() override;
};
