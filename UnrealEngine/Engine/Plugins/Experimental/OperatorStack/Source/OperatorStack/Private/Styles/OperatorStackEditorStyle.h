// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FOperatorStackEditorStyle : public FSlateStyleSet
{
public:
	FOperatorStackEditorStyle();
	
	virtual ~FOperatorStackEditorStyle() override;
	
	static FOperatorStackEditorStyle& Get()
	{
		static FOperatorStackEditorStyle StyleSet;
		return StyleSet;
	}
};
