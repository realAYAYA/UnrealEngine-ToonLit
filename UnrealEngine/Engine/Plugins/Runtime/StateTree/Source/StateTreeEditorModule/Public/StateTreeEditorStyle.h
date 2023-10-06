// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class ISlateStyle;

class STATETREEEDITORMODULE_API FStateTreeEditorStyle
	: public FSlateStyleSet
{
public:
	static FStateTreeEditorStyle& Get();

protected:
	friend class FStateTreeEditorModule;

	static void Register();
	static void Unregister();

private:
	FStateTreeEditorStyle();
};
