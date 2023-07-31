// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"


class FOptimusEditorStyle
    : public FSlateStyleSet
{
public:
	static FOptimusEditorStyle& Get();

protected:
	friend class FOptimusEditorModule;

	static void Register();
	static void Unregister();

private:
	FOptimusEditorStyle();
};
