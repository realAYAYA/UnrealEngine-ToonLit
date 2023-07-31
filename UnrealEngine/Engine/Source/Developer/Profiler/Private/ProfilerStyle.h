// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"
#include "UObject/NameTypes.h"
#include "Templates/UniquePtr.h"

class FProfilerStyle
	: public FSlateStyleSet
{
public:
	virtual const FName& GetStyleSetName() const override;

	static const FProfilerStyle& Get();
	static void Shutdown();
	
	~FProfilerStyle();

private:
	FProfilerStyle();

	static FName StyleName;
	static TUniquePtr<FProfilerStyle> Inst;
};
