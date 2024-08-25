// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FCEEditorStyle : public FSlateStyleSet
{
public:
	FCEEditorStyle();

	virtual ~FCEEditorStyle() override;

	static FCEEditorStyle& Get()
	{
		static FCEEditorStyle StyleSet;
		return StyleSet;
	}
};