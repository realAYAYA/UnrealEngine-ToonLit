// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FCurveExpressionEditorStyle
    : public FSlateStyleSet
{
public:
	static FCurveExpressionEditorStyle& Get();

	static void Register();
	static void Unregister();

private:
	FCurveExpressionEditorStyle();
};
