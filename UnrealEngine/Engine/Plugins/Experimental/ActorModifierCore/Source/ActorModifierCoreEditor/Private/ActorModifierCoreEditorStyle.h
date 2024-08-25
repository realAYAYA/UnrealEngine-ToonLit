// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FActorModifierCoreEditorStyle : public FSlateStyleSet
{
public:
	FActorModifierCoreEditorStyle();
	
	virtual ~FActorModifierCoreEditorStyle() override;
	
	static FActorModifierCoreEditorStyle& Get()
	{
		static FActorModifierCoreEditorStyle StyleSet;
		return StyleSet;
	}
};