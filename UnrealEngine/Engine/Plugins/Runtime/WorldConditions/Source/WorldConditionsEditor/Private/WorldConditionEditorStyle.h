// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FWorldConditionEditorStyle final : public FSlateStyleSet
{
public:
	virtual ~FWorldConditionEditorStyle() override;

	static FWorldConditionEditorStyle& Get();
	static void Shutdown();

	static FColor TypeColor;
private:
	FWorldConditionEditorStyle();

	static TUniquePtr<FWorldConditionEditorStyle> Instance;
};
