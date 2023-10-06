// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateNoResource.h"

#include "Styling/SlateWidgetStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "FToolkitWidgetStyle.generated.h"

/**
 * FToolkitWidgetStyle is the FSlateWidgetStyle that defines the styling of a ToolkitWidget 
 */
USTRUCT(BlueprintType)
struct WIDGETREGISTRATION_API FToolkitWidgetStyle : public FSlateWidgetStyle
{
	GENERATED_BODY()

	FToolkitWidgetStyle() :
		TitleBackgroundBrush(FSlateNoResource()),
		ToolDetailsBackgroundBrush(FSlateNoResource()),
		TitleForegroundColor(FStyleColors::Panel)
	{
		
	}

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }

	static const FToolkitWidgetStyle& GetDefault();
		virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush TitleBackgroundBrush;
	FToolkitWidgetStyle& SetTitleBackgroundBrush(const FSlateBrush& InPaletteTitleBackgroundBrush);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush ToolDetailsBackgroundBrush;
	FToolkitWidgetStyle& SetToolDetailsBackgroundBrush(const FSlateBrush& InToolDetailsBackgroundBrush);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor TitleForegroundColor;
	FToolkitWidgetStyle& SetTitleForegroundColor(const FSlateColor& InTitleForegroundColor);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin TitlePadding;
	FToolkitWidgetStyle& SetTitlePadding(const FMargin& InTitlePadding);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin ActiveToolTitleBorderPadding;
	FToolkitWidgetStyle& SetActiveToolTitleBorderPadding(const FMargin& InActiveToolTitleBorderPadding);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin ToolContextTextBlockPadding;
	FToolkitWidgetStyle& SetToolContextTextBlockPadding(const FMargin& InToolContextTextBlockPadding);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateFontInfo TitleFont;
	FToolkitWidgetStyle& SetTitleFont(const FSlateFontInfo& InTitleFont);
};