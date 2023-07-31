// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FPCGEditorStyle : public FSlateStyleSet
{
public:
	static void Register();
	static void Unregister();

	static const FPCGEditorStyle& Get();

private:	
	FPCGEditorStyle();
};
