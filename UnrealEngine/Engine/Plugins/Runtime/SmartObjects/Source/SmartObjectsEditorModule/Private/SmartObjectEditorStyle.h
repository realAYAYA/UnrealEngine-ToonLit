// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Templates/UniquePtr.h"

class FSmartObjectEditorStyle final : public FSlateStyleSet
{
public:
	virtual ~FSmartObjectEditorStyle();

	static FSmartObjectEditorStyle& Get();
	static void Shutdown();

	static FColor TypeColor;
private:
	FSmartObjectEditorStyle();

	static TUniquePtr<FSmartObjectEditorStyle> Instance;
};
