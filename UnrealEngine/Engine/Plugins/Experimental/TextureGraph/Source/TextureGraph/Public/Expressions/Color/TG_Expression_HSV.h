// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression.h"
#include "TG_Texture.h"

#include "TG_Expression_HSV.generated.h"

//////////////////////////////////////////////////////////////////////////
/// RGB2HSV Correction
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_RGB2HSV : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_EXPRESSION(TG_Category::Adjustment);
	virtual void						Evaluate(FTG_EvaluationContext* InContext) override;

	// The input image in RGB to be converted to in HSV color space
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = ""))
	FTG_Texture							Input;

	// The output image which will be in HSV color space 
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = ""))
	FTG_Texture							Output;

	virtual FTG_Name					GetDefaultName() const override { return TEXT("RGBtoHSV");}
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Interprets the input as RGB and converts it to HSV.")); } 

};

//////////////////////////////////////////////////////////////////////////
/// HSV2RGB Correction
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_HSV2RGB : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_EXPRESSION(TG_Category::Adjustment);
	virtual void						Evaluate(FTG_EvaluationContext* InContext) override;

	// The input image in HSV to be converted to in RGB color space
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = ""))
	FTG_Texture							Input;

	// The output image which will be in RGB color space 
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = ""))
	FTG_Texture							Output;

	virtual FTG_Name					GetDefaultName() const override { return TEXT("HSVtoRGB");}
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Interprets the input as HSV and converts it to RGB.")); } 

};

//////////////////////////////////////////////////////////////////////////
/// HSV Correction
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_HSV : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_EXPRESSION(TG_Category::Adjustment);
	virtual void						Evaluate(FTG_EvaluationContext* InContext) override;

	// The input image to be adjusted in HSV space
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = ""))
	FTG_Texture							Input;

	// Defines the basic color tone, such as red, green, or blue, without considering brightness or intensity. Adjusting the hue changes the overall color appearance while maintaining its saturation and brightness.
	// The normalized hue. Please divide your [0, 359] hue values by 359 for this input
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1", PinDisplayName = "Hue (Normalized)", DisplayName = "Hue (Normalized)"))
	float								Hue = 1.0f;
	
	// Controls the intensity of the color. Higher values represent more vivid colors, while lower values produce muted tones.
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
	float								Saturation = 1.0f;

	// Specifies the brightness or darkness of the color. Higher values correspond to brighter colors, while lower values result in darker shades.
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
	float								Value = 1.0f;

	// The output image which is a single channel grayscale version of the input image 
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = ""))
	FTG_Texture							Output;

	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Converts Hue, Saturation and Value inputs to an RGB Image.")); } 

};

