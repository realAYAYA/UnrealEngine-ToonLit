// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaMRQEditorStyle final : public FSlateStyleSet
{
public:
	static FAvaMRQEditorStyle& Get()
	{
		static FAvaMRQEditorStyle Instance;
		return Instance;
	}

	FAvaMRQEditorStyle();
	virtual ~FAvaMRQEditorStyle() override;
};
