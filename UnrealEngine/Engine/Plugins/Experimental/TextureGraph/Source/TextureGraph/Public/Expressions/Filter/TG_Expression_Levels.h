// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression.h"
#include "TG_Texture.h"

#include "TG_Expression_Levels.generated.h"

USTRUCT(BlueprintType)
struct TEXTUREGRAPH_API FTG_LevelsSettings
{
	GENERATED_USTRUCT_BODY()

	// The Low value of the Levels adjustment, any pixel under that value is set to black. Default is 0.
	UPROPERTY(EditAnywhere, Category = "Levels", meta = (PinDisplayName = "Low", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
		float								Low = 0;

	// The mid value of the Levels adjustment, must be in the range [Min, Max] and the Default is 0.5.
	// The mid value determine where the smoothing curve applying the midpoint filter is crossing 0.5.
	UPROPERTY(EditAnywhere, Category = "Levels", meta = (PinDisplayName = "Mid", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
		float								Mid = 0.5f; // Midtones

	// The High value of the Levels adjustment, any pixel above that value is set to white. Default is 1.
	UPROPERTY(EditAnywhere, Category = "Levels", meta = (PinDisplayName = "High", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
		float								High = 1;

	bool SetLow(float InValue);
	bool SetMid(float InValue);
	bool SetHigh(float InValue);

	// Eval Low-High range mapping on value
	float EvalRange(float Val) const;
	// Eval reverse Low-High range mapping on value
	float EvalRangeInv(float Val) const;

	// Evaluate the MidExponent of the power curve applying the midpoint filter
	float EvalMidExponent() const;
	bool SetMidFromMidExponent(float InExponent);

	void InitFromString(const FString& StrVal)
	{
		FTG_LevelsSettings::StaticStruct()->ImportText(*StrVal, this, /*OwnerObject*/nullptr, 0, nullptr, FTG_LevelsSettings::StaticStruct()->GetName(), /*bAllowNativeOverride*/true);
	}

	FString ToString() const
	{
		FString ExportString;
		FTG_LevelsSettings::StaticStruct()->ExportText(ExportString, this, this, /*OwnerObject*/nullptr, /*PortFlags*/0, /*ExportRootScope*/nullptr);
		return ExportString;
	}


};

void FTG_LevelsSettings_VarPropertySerialize(FTG_Var::VarPropertySerialInfo&);
template <> FString TG_Var_LogValue(FTG_LevelsSettings& Value);
template <> void TG_Var_SetValueFromString(FTG_LevelsSettings& Value, const FString& StrVal);

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Levels : public UTG_Expression
{
	GENERATED_BODY()

public:

	TG_DECLARE_EXPRESSION(TG_Category::Adjustment);
	virtual void						Evaluate(FTG_EvaluationContext* InContext) override;

	// The input image to adjust the levels for
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Input", PinDisplayName = "", MD_HistogramLuminance))
	FTG_Texture							Input;

	// The Low-Mid-High settings of the Levels expression
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", PinDisplayName = "", MD_LevelsSettings, PinNotConnectable = true))
	FTG_LevelsSettings					Levels;

	// TODO: Show Property after 5.4 release
	//	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", PinNotConnectable = true))
	bool								AutoLevels = false;

	// The output image filtered result of the Levels operator 
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = ""))
	FTG_Texture							Output;

	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Remaps shadows and highlights of the input. Any values less or equal to Low are mapped to black, any values, greater or equal to High are mapped to white, and any values inbetween have Gamma applied as an exponent.")); } 

};
