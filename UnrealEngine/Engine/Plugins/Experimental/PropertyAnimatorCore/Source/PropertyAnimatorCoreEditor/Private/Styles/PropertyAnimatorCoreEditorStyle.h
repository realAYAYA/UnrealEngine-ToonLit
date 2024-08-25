// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FPropertyAnimatorCoreEditorStyle : public FSlateStyleSet
{
public:
	FPropertyAnimatorCoreEditorStyle();

	virtual ~FPropertyAnimatorCoreEditorStyle() override;

	static FPropertyAnimatorCoreEditorStyle& Get()
	{
		static FPropertyAnimatorCoreEditorStyle StyleSet;
		return StyleSet;
	}
};