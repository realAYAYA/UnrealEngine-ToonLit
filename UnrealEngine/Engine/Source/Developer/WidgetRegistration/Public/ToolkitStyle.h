// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Templates/SharedPointer.h"

class ISlateStyle;

/** FToolkitStyle is the FSlateStyleSet that defines styles for FToolkitBuilders  */
class WIDGETREGISTRATION_API FToolkitStyle
	: public FSlateStyleSet
{
public:
	
	static FName StyleName;
	FToolkitStyle();
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();
	virtual const FName& GetStyleSetName() const override;

private:

	static TSharedRef< class FSlateStyleSet > Create();
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};

