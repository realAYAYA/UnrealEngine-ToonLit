// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FVariantManagerStyle final : public FSlateStyleSet
{
public:
	FVariantManagerStyle();

	~FVariantManagerStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static FVariantManagerStyle& Get()
	{
		static FVariantManagerStyle Inst;
		return Inst;
	}
};
